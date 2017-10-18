#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include "jsocket6.4.h"
#include "Data.h"
#include <pthread.h>

char buffer1[BUFFER_LENGTH];
char out[BUFFER_LENGTH+6];
char buffer2[BUFFER_LENGTH];
char in[BUFFER_LENGTH+6];
int contador;
int fd; //archivo
int n; //indicador de cuanto se escribe
int bytes, cnt, packs; //contador de bytes, lecturas/escrituras ,packs enviados    
int sTCP, sUDP;//identificador del servidor
int seq_num_out, seq_num_in;
fd_set rfds;
struct timeval timeout;
int retval;

void* bwc_a_bwcs();
void* bwcs_a_bwc();

int main (void) {

	pthread_t t1, t2;
	contador = 0;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

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
	pthread_create(&t1, NULL, bwc_a_bwcs, NULL);
	pthread_create(&t2, NULL, bwcs_a_bwc, NULL);

	pthread_join(t1, NULL);
	pthread_join(t2, NULL);

	close(sUDP);
	Dclose(sTCP);
	return 0;

}

int to_int_seq(unsigned char b) {
    int res=0;
    int i;

    for(i=DSEQ; i < DHDR; i++)
        res = (res*10)+(b[i]-'0');

    return res;
}

void to_char_seq(int seq, unsigned char *buf) {
    int i;
    int res = seq;

    for(i=DHDR - 1; i >= DSEQ; i--) {
        buf[i] = (res % 10) + '0';
        res /= 10;
    }
// fprintf(stderr, "to_char %d -> %c, %c, %c, %c, %c\n", seq, buf[0], buf[1], buf[2], buf[3], buf[4]);
}

/* 
int select(int nfds, fd_set *readfds, fd_set *writefds,
                  fd_set *exceptfds, struct timeval *timeout);
*/

void* bwc_a_bwcs() {   
    int cnt;
    out[DTYPE] = 'D';
    to_char_seq(seq_num_out, out);
    write(sUDP, out, 6);
	for(;;) {
        FD_ZERO(&rfds);
        FD_SET(, &rfds);

    	if((cnt = Dread(sTCP, buffer1, BUFFER_LENGTH)) <= 0) { //leer desde bwc 00000
    		fprintf(stderr, "FIN DE LECTURA\n");
    	    break;
    	}
    	fprintf(stderr, "Traspasando de sTCP a sUDP\n");
        write(sUDP, buffer1, cnt); //enviar datos al servidor
    }
    write(sUDP, buffer1, 0);
    return NULL;
}

void* bwcs_a_bwc() {
    int cnt;
    for(;;) {
        fprintf(stderr, "Traspasando de sUDP a sTCP\n");
        if((cnt = read(sUDP, buffer2, BUFFER_LENGTH)) <= 0) { //leer desde bwc
            fprintf(stderr, "FIN DE LECTURA\n");
            break;
        }
        Dwrite(sTCP, buffer2, cnt); //enviar datos al servidor
    }
    Dwrite(sTCP, buffer2, 0);
    return NULL;
}

