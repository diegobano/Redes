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

char buffer1[BUFFER_LENGTH];
char out1[BUFFER_LENGTH+6];
char out2[DHDR];
char buffer2[BUFFER_LENGTH];
char in1[DHDR];
char in2[BUFFER_LENGTH+6];
char acks[MAX_ACKS][DHDR];

int contador;
int fd; //archivo
int n; //indicador de cuanto se escribe
int bytes, cnt, packs; //contador de bytes, lecturas/escrituras ,packs enviados    
int sTCP, sUDP;//identificador del servidor
int seq_num_out, seq_num_in;
fd_set rfds;
int retval;
int last_wrong, last_right;

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
    int cnt, ack_num, not_acked = 1;
    struct timeval ti, curr_time;
    out1[DTYPE] = 'D';
    to_char_seq_inplace(seq_num_out, out1);
    write(sUDP, out1, 6);
    for(;;) {
        if((cnt = Dread(sTCP, buffer1, BUFFER_LENGTH)) <= 0) { //leer desde bwc 00000
            fprintf(stderr, "FIN DE LECTURA DESDE TCP\n");
            break;
        }
        strncpy(out1 + DHDR, buffer1, cnt);
        fprintf(stderr, "Traspasando de sTCP a sUDP\n");
        to_char_seq_inplace(seq_num_out, out1);
        do {
            write(sUDP, out1, cnt + DHDR); //enviar datos al servidor
            printf("Esperando ack de %d\n", seq_num_out);
            
            gettimeofday(&ti, NULL);
            while (1) {
                gettimeofday(&curr_time, NULL);
                if (curr_time.tv_sec - ti.tv_sec >= 1) {
                    break;
                }
                if (last_right != last_wrong) {
                    strncpy(in1, acks[last_right], 6);
                    last_right = (last_right + 1) % MAX_ACKS;
                    printf("%s\n", in1);
                    ack_num = to_int_seq_inplace(in1);
                    printf("ack recibido para %d\n", ack_num);
                    if (ack_num != seq_num_out) {
                        printf("ack distinto al esperado: %d\n", ack_num);
                        break;
                    } else {
                        seq_num_out++;
                        not_acked = 0;
                        break;
                    }
                }
            }
        } while (not_acked);
        not_acked = 1;
    }

    printf("Enviando fin de mensajes\n");
    to_char_seq_inplace(seq_num_out, out1);
    do {
        write(sUDP, out1, DHDR); //enviar datos al servidor
        gettimeofday(&ti, NULL);
        while (1) {
            gettimeofday(&curr_time, NULL);
            if (curr_time.tv_sec - ti.tv_sec >= 1) {
                break;
            }
            if (last_right != last_wrong) {
                strcpy(in1, acks[last_right]);
                last_right = (last_right + 1) % MAX_ACKS;
                printf("%s\n", in1);
                ack_num = to_int_seq_inplace(in1);
                printf("ack recibido para %d\n", ack_num);
                if (ack_num != seq_num_out) {
                    printf("ack distinto al esperado: %d\n", ack_num);
                    break;
                } else {
                    seq_num_out++;
                    not_acked = 0;
                    break;
                }
            }
        }
    } while (not_acked);
        
    printf("Terminando escritura a sUDP\n");
    return NULL;
}

void* bwcs_a_bwc() {
    int cnt, next_seq;
    for(;;) {
        if((cnt = read(sUDP, in2, BUFFER_LENGTH + DHDR)) <= 0) { //leer desde bwc
            fprintf(stderr, "FIN DE LECTURA\n");
            break;
        }
        if (in2[DTYPE] == 'A') {
            printf("ACK encontrado: %s, SEQ: %d\n", in2, seq_num_out);
            strncpy(acks[last_wrong], in2, DHDR);
            last_wrong = (last_wrong + 1) % MAX_ACKS;
        } else {
            next_seq = to_int_seq_inplace(in2);
            if (next_seq == seq_num_in + 1) {
                strncpy(buffer2, in2 + DHDR, cnt - DHDR);
                strncpy(out2, in2, 6);
                out2[DTYPE] = 'A';
                Dwrite(sTCP, buffer2, cnt - DHDR); //enviar datos al servidor
                write(sUDP, out2, DHDR);
                seq_num_in = next_seq;
            } else if (next_seq == seq_num_in) {
                out2[DTYPE] = 'A';
                to_char_seq_inplace(seq_num_in, out2);
                write(sUDP, out2, DHDR);
            } else {
                printf("wat\n");
            }
            if (cnt <= 6) {
                printf("Mensaje vacío final\n");
                break;
            }
        }
    }
    printf("Enviando mensaje final\n");
    Dwrite(sTCP, buffer2, 0);
    return NULL;
}