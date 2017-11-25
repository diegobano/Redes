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
char buffer1[WIN_SZ + 1][BUFFER_LENGTH+6]; //buffer de entrada para leer desde bwc
int cnt[WIN_SZ + 1]; //tamaño de cada paquete
char out1[BUFFER_LENGTH+6]; //ventana de salida para escribir en bwcs
char in1[DHDR]; //ack de entrada a bwcs

//uso de bwcs_a_bwc
char buffer2[BUFFER_LENGTH]; //buffer de salida para escribir en bwc
char out2[DHDR]; //ack de salida desde bwcs
char in2[BUFFER_LENGTH+6]; //buffer de entrada para leer desde bwcs

int timeout, inside, curr_pos;
int last_seen, acks_counter;

// arreglo con acks
//char acks[MAX_ACKS][DHDR];
char acks[WIN_SZ][DHDR];
int last_wrong, last_right; //contadores de ack esperado para arreglo de acks

int contador;
int fd; //archivo
int n; //indicador de cuanto se escribe
int bytes, cont, packs; //contador de bytes, lecturas/escrituras, packs enviados    
int sTCP, sUDP;//identificador del servidor
int seq_num_out, seq_num_in; //numeros de secuencia de salida y entrada
int ini_win_seq_num, end_win_seq_num;
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
    last_wrong = 0; last_right = 0; last_seen = 0; acks_counter = 0;

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


/* Tratamiento de la señal SIGALRM.
 * Escribe en pantalla que ha llegado la señal
 */
void resend_all_packets (int id) {
    int i = 0;            
    printf ("Recibida señal\n");
    while (timeout && i < inside) { //enviamos todos los paquetes dentro de la ventana
        int index = (curr_pos + i) % WIN_SZ; //siguiente paquete en la ventana
        write(sUDP, buffer1[index], cnt[index] + DHDR); //enviar datos al servidor                
        printf("Esperando ack de %d\n", ini_win_seq_num + i);
        i++;
    }
    alarm(TIMEOUT);    
}

void* bwc_a_bwcs() {
    //int kk = 0;
    signal (SIGALRM, resend_all_packets);
    int ack_num, not_acked = 1, diff;
    int all_read = 0, last_msg = 0;
    //struct timeval ti, curr_time;
    timeout = 1; 
    int i = 0; //se pone por defecto en 1 para que se envie todo al principio
    //proceso de recibir datos de bwc via socket TCP y enviarlo a socketUDP
    ini_win_seq_num = seq_num_out;
    end_win_seq_num = seq_num_out + WIN_SZ;
    int next_pos = 0;
    inside = 0; curr_pos = 0;
    out1[DTYPE] = 'D'; //paqueta de datos
    to_char_seq_inplace(seq_num_out, out1); //se escribe num de seq en buffer1[i]        
    while (i < WIN_SZ) {
        buffer1[i][DTYPE] = 'D'; //paqueta de datos    
        i++;
        //to_char_seq_inplace(ini_win_seq_num + i, buffer1[i]); //se escribe num de seq en buffer1[i]        
    }        
    write(sUDP, out1, DHDR); //escribir out1 en sUDP
    for(;;) {
        alarm(TIMEOUT);
        //enviamos toda la ventana si 
        while(!all_read && inside < WIN_SZ) { //llenamos paquetes para enviar a ventana
            printf("ping\n");            
            int c = Dread(sTCP, out1 + DHDR, BUFFER_LENGTH);
            if(c <= 0) { //leer desde bwc 00000            
                all_read = 1;
                break; //no hay mas que recibir
            }
            cnt[next_pos] = c;
            strncpy(buffer1[next_pos] + DHDR, out1 + DHDR, c);            
            printf("En ventana: %d\n", inside);
            to_char_seq_inplace(ini_win_seq_num + inside, buffer1[next_pos]); //se escribe numero de secuencia en paquete    
            //printf("Buffer %d: %s\n", next_pos, buffer1[next_pos]);
            write(sUDP, buffer1[next_pos], cnt[next_pos] + DHDR); //enviar datos al servidor
            printf("Esperando ack de %d\n", ini_win_seq_num + inside);
            //printf("Entran %d bytes, contenido: %s\n", cnt[next_pos], buffer1[next_pos]);
            next_pos = (next_pos + 1) % WIN_SZ; inside++;
        }

        if(inside == 0 && all_read && !last_msg) {
            fprintf(stderr, "FIN DE LECTURA DESDE TCP\n");
            printf("Enviando fin de mensajes\n");
            cnt[next_pos] = 0;
            //strcpy(buffer1[next_pos] + DHDR, out1 + DHDR, c);
            to_char_seq_inplace(ini_win_seq_num + inside, buffer1[next_pos]); //se escribe numero de secuencia en paquete                                    
            write(sUDP, buffer1[next_pos], DHDR); //paquete de cierre
            inside++;
            last_msg = 1;
        }

        if(inside == 0) {
            break; //no queda nah mas que enviar
        }

        fprintf(stderr, "Traspasando de sTCP a sUDP\n");
        do { //hacer esto mientras no llegue ack esperado    
            pthread_mutex_lock(&reading);
            while (1) { //esperamos confirmacion de recepcion                        
                if (acks_counter != 0) { //llego ack nuevo yaay                    
                    acks_counter--;
                    strncpy(in1, acks[last_seen], 6); //copiar ultimo ack bueno en in1
                    last_seen = (last_seen + 1) % MAX_ACKS; //siguiente ack
                    printf("%s\n", in1);
                    ack_num = to_int_seq_inplace(in1); //extraigo numero de secuencia
                    printf("ack recibido para %d\n", ack_num);
                    if (ack_num < ini_win_seq_num || ack_num > end_win_seq_num) { //no es el que esperada m3n
                        if (last_msg) {
                            printf("Reenviando ultimo mensaje\n");
                            last_msg = 0;
                        }
                        printf("ack distinto al esperado: %d\n", ini_win_seq_num);
                        break;
                    } else { //me llego mi amiwo ack esperado
                        printf("Llego, movemos ventana\n");
                        alarm(0); //desactivo solo por prrrrecaucion
                        diff = ack_num - ini_win_seq_num;
                        ini_win_seq_num = ack_num + 1; end_win_seq_num = ini_win_seq_num + WIN_SZ; //movemos ventana                        
                        inside -= diff + 1;
                        printf("Inicio: %d, Final: %d\n", ini_win_seq_num, ini_win_seq_num + inside);
                        curr_pos = (curr_pos + 1) % WIN_SZ;
                        not_acked = 0; //has been acked c:
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&reading);
        } while (not_acked); //mientras no llego mi ack esperado        
        not_acked = 1; //reset condicion
    }
    
    alarm(0); //desactivo solo por prrrrecaucion                        
    //Intento de confirmacion el cierre de la conexion            
    printf("Terminando escritura a sUDP\n");
    return NULL;
}

void* bwcs_a_bwc() {
    //int kk = 0;
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
            pthread_mutex_unlock(&reading);
            int c = acks_counter++;
            printf("ACK encontrado: %s, SEQ: %d\n", in2, ini_win_seq_num);
            strncpy(acks[last_seen + c], in2, DHDR); //se copia ack recibido en arreglo de acks
            //last_wrong = (last_wrong + 1) % MAX_ACKS; //se actualiza nuevo ack recibido
            pthread_mutex_unlock(&reading);
        } else { //paquete es de datos
            next_seq = to_int_seq_inplace(in2); //extraer siguiente numero de secuencia
            if (next_seq > seq_num_in) { //si es el siguiente que viene (mantener orden)
                printf("Encontrado %d\n", next_seq);
                strncpy(buffer2, in2 + DHDR, cnt - DHDR); //copiar datos de in2 a buffer2 (sin contar header)
                strncpy(out2, in2, 6); //copiar datos de in2 a out2
                out2[DTYPE] = 'A'; //confirmacion pa socket UDP
                Dwrite(sTCP, buffer2, cnt - DHDR); //enviar datos en buffer2 a socket TCP 
                write(sUDP, out2, DHDR); //se escribe paquete en socketUDP (confirmacion que se recibio)
                seq_num_in = next_seq;
            } else if (next_seq == seq_num_in) { //si es el mismo que se tiene
                out2[DTYPE] = 'A'; //confirmacion
                to_char_seq_inplace(seq_num_in, out2); //se escribe numero de secuencia en paquete
                write(sUDP, out2, DHDR); //se escribe header en socketUDP (confirmacion de que ya se recibio)
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