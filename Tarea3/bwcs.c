#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <pthread.h>
#include "jsocket6.4.h"
#include "Data.h"

#define MAX_ACKS 10

// uso de bcw_a_bwcs
char buffer1[WIN_SZ][BUFFER_LENGTH+6]; //buffer de entrada para leer desde bwc
char out1[BUFFER_LENGTH+6]; //ventana de salida para escribir en bwcs
char in1[DHDR]; //ack de entrada a bwcs

//uso de bwcs_a_bwc
char buffer2[BUFFER_LENGTH]; //buffer de salida para escribir en bwc
char out2[DHDR]; //ack de salida desde bwcs
char in2[BUFFER_LENGTH+6]; //buffer de entrada para leer desde bwcs

// arreglo con acks
char acks[MAX_ACKS][DHDR];
int last_wrong, last_right; //contadores de ack esperado para arreglo de acks

int contador;
int fd; //archivo
int n; //indicador de cuanto se escribe
int bytes, cnt, packs; //contador de bytes, lecturas/escrituras, packs enviados    
int sTCP, sUDP;//identificador del servidor
int seq_num_out, seq_num_in; //numeros de secuencia de salida y entrada
int ini_win_seq_num, end_win_seq_num;
fd_set rfds; //???
int retval;

pthread_mutex_t reading;
pthread_cond_t no_data;

void* bwc_a_bwcs();
void* bwcs_a_bwc();

int main (void) {

	pthread_t t1, t2;
    pthread_mutex_init(&reading, NULL);
    pthread_cond_init(&no_data, NULL);
	contador = 0;
    last_wrong = 0, last_right = 0;

	//conexion desde bwc a bwcs
	int s = j_socket_tcp_bind("2001");
    if(s < 0) {
		fprintf(stderr, "bind failed\n");
		exit(1);
    }
    sTCP = j_accept(s);
    fprintf(stderr, "socket TCP conectado\n");
	sUDP = j_socket_udp_connect("localhost", "2000");
	fprintf(stderr, "socket UDP conectado\n");

    seq_num_out = 0;
    seq_num_in = -1;
	pthread_create(&t1, NULL, bwc_a_bwcs, NULL);
	pthread_create(&t2, NULL, bwcs_a_bwc, NULL);

    printf("Esperando a t1\n");
	pthread_join(t1, NULL);
    printf("Esperando a t2\n");
	pthread_join(t2, NULL);

	close(sUDP);
	Dclose(sTCP);
	return 0;
}

int to_int_seq_inplace(char* b) {
    int res=0;
    int i;

    for(i=DSEQ; i < DHDR; i++)
        res = (res*10)+(b[i]-'0');

    return res;
}

void to_char_seq_inplace(int seq, char *buf) {
    int i;
    int res = seq;

    for(i=DHDR - 1; i >= DSEQ; i--) {
        buf[i] = (res % 10) + '0';
        res /= 10;
    }
}


void* bwc_a_bwcs() {
    int cnt[WIN_SZ], ack_num, not_acked = 1;
    struct timeval ti, curr_time;
    int timeout = 1, i = 0; //se pone por defecto en 1 para que se envie todo al principio
    //proceso de recibir datos de bwc via socket TCP y enviarlo a socketUDP
    ini_win_seq_num = seq_num_out;
    end_win_seq_num = seq_num_out + WIN_SZ;
    int inside = 0, curr_pos = 0, next_pos = 0;
    while (i++ < WIN_SZ) {
        buffer1[i][DTYPE] = 'D'; //paqueta de datos    
        to_char_seq_inplace(seq_num_out, buffer1[i]); //se escribe num de seq en buffer1[i]        
    }        
    write(sUDP, buffer1[0], 6); //escribir buffer1[0] en sUDP
    for(;;) {
        while(inside < WIN_SZ){ //llenamos paquetes para enviar a ventana
            if((cnt[next_pos % WIN_SZ] = Dread(sTCP, buffer1[next_pos] + DHDR, BUFFER_LENGTH)) <= 0) { //leer desde bwc 00000                
                fprintf(stderr, "FIN DE LECTURA DESDE TCP\n");
                break; //no hay mas que recibir
            }
            //printf("En ventana: %d\n", inside);
            to_char_seq_inplace(ini_win_seq_num + inside, buffer1[next_pos]); //se escribe numero de secuencia en paquete    
            next_pos++; next_pos %= WIN_SZ; inside++;            
        }
        if(inside == 0) break; //no queda nah mas que enviar

        //se escribio lo que llego de sTCP en buffer1
        //strncpy(out1 + DHDR, buffer1, cnt); //se copian datos de buffer1 en out1 + offset_de_header
        fprintf(stderr, "Traspasando de sTCP a sUDP\n");
        
        do { //hacer esto mientras no llegue ack esperado            
            i = 0;
            while (timeout && i < inside) { //enviamos todos los paquetes dentro de la ventana
                int index = (curr_pos + i) % WIN_SZ; //siguiente paquete en la ventana
                write(sUDP, buffer1[index], cnt[index] + DHDR); //enviar datos al servidor                
                printf("Esperando ack de %d\n", ini_win_seq_num + i);
                i++;
            }
            if(!timeout) {
                int index = (curr_pos + inside - 1) % WIN_SZ; //hay un nuevo paquete para enviar
                write(sUDP, buffer1[index], cnt[index] + DHDR); //enviar datos al servidor
            } else timeout = 0;

            gettimeofday(&ti, NULL);            
            while (1) { //esperamos confirmacion de recepcion
                gettimeofday(&curr_time, NULL);
                if (curr_time.tv_sec - ti.tv_sec >= 1) { //si ha pasado mas de un segundo (timeout)
                    printf("Timeout!\n");
                    timeout = 1;
                    break;
                }
                if (last_right != last_wrong) { //llego ack nuevo yaay
                    strncpy(in1, acks[last_right], 6); //copiar ultimo ack bueno en in1
                    last_right = (last_right + 1) % MAX_ACKS; //siguiente ack
                    printf("%s\n", in1);
                    ack_num = to_int_seq_inplace(in1); //extraigo numero de secuencia
                    printf("ack recibido para %d\n", ack_num);
                    if (ack_num != ini_win_seq_num) { //no es el que esperada m3n
                        printf("ack distinto al esperado: %d\n", ack_num);
                        break;
                    } else { //me llego mi amiwo ack esperado
                        ini_win_seq_num++; end_win_seq_num++; //movemos ventana                        
                        inside--;
                        curr_pos = (curr_pos + 1) % WIN_SZ;
                        not_acked = 0; //has been acked c:
                        break;
                    }
                }
            }
        } while (not_acked); //mientras no llego mi ack esperado
        not_acked = 1; //reset condicion
    }
    
    //Intento de confirmacion el cierre de la conexion
    printf("Enviando fin de mensajes\n");
    to_char_seq_inplace(seq_num_out, out1); //se escribe de nuevo dado que num de seq cambio 
    do { //hacer esto mientras no llegue ack esperado
        write(sUDP, out1, DHDR); //enviar datos al servidor
        gettimeofday(&ti, NULL);
        while (1) {
            gettimeofday(&curr_time, NULL);
            if (curr_time.tv_sec - ti.tv_sec >= 1) { //si ha pasado mas de un segundo (timeout)
                break;
            }
            if (last_right != last_wrong) { //llego ack nuevo yaay
                strcpy(in1, acks[last_right]); //copiar ultimo ack bueno en in1
                last_right = (last_right + 1) % MAX_ACKS; //siguiente ack
                printf("%s\n", in1);
                ack_num = to_int_seq_inplace(in1); //extraigo numero de secuencia
                //printf("ack recibido para %d\n", ack_num);
                if (ack_num != seq_num_out) { //no es el que esperada m3n
                    printf("ack distinto al esperado: %d\n", ack_num);
                    break;
                } else { //me llego mi amiwo ack esperado
                    seq_num_out++; //movemos al siguiente
                    not_acked = 0; //has been acked c:
                    break;
                }
            }
        }
    } while (not_acked); //mientras no llego mi ack esperado
        
    printf("Terminando escritura a sUDP\n");
    return NULL;
}

void* bwcs_a_bwc() {
    int cnt, next_seq;
    //proceso de recibir datos de bwcs via socket UDP 
    //y escribirlos en bwc via socketTCP
    for(;;) {
        if((cnt = read(sUDP, in2, BUFFER_LENGTH + DHDR)) <= 0) { //leer desde bwc
            fprintf(stderr, "FIN DE LECTURA\n");
            break; //no hay mas que recibir
        }
        //se escribio lo que llego de sUDP en in2
        if (in2[DTYPE] == 'A') { //si paquete entrante es de confirmacion
            printf("ACK encontrado: %s, SEQ: %d\n", in2, ini_win_seq_num);
            strncpy(acks[last_wrong], in2, DHDR); //se copia ack recibido en arreglo de acks
            last_wrong = (last_wrong + 1) % MAX_ACKS; //se actualiza nuevo ack recibido
        } else { //paquete es de datos
            next_seq = to_int_seq_inplace(in2); //extraer siguiente numero de secuencia
            if (next_seq == seq_num_in + 1) { //si es el siguiente que viene (mantener orden)
                strncpy(buffer2, in2 + DHDR, cnt - DHDR); //copiar datos de in2 a buffer2 (sin contar header)
                strncpy(out2, in2, 6); //copiar datos de in2 a out2
                out2[DTYPE] = 'A'; //confirmacion pa socket UDP
                Dwrite(sTCP, buffer2, cnt - DHDR); //enviar datos en buffer2 a socket TCP 
                write(sUDP, out2, DHDR); //se escribe paquete en socketUDP (confirmacion que se recibio)
                seq_num_in = next_seq;
            } else if (next_seq == seq_num_in) { //si es el mismo que se tiene
                out2[DTYPE] = 'A'; //confirmacion
                to_char_seq_inplace(seq_num_in, out2); //se escribe numero de secuencia en paquete
                write(sUDP, out2, DHDR); //se escribe header en socketUDP (confirmacion pa que empiece a enviar?)
            } else { // paquete invalido, se descarta
                printf("Paquete descartado\n");
            }
            if (cnt <= 6) { //mensaje pa cerrar
                printf("Mensaje vacío final\n");
                break;
            }
        }
    }
    printf("Enviando mensaje final\n");
    Dwrite(sTCP, buffer2, 0); //para cerrar se envia paquete vacio
    return NULL;
}