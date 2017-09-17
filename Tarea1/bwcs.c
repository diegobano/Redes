#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>
#include "jsocket6.4.h"
#include "Data.h"
#include <pthread.h>

char buffer1[BUFFER_LENGTH];
char buffer2[BUFFER_LENGTH];
int contador;
int fd; //archivo
int n; //indicador de cuanto se escribe
int bytes, cnt, packs; //contador de bytes, lecturas/escrituras ,packs enviados    
int sTCP, sUDP;//identificador del servidor

void* bwc_a_bwcs();
void* bwcs_a_bwc();

int main (void) {

	pthread_t t1, t2;
	contador = 0;

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

	pthread_create(&t1, NULL, bwc_a_bwcs, NULL);
	pthread_create(&t2, NULL, bwcs_a_bwc, NULL);

	pthread_join(t1, NULL);
	pthread_join(t2, NULL);

	close(sUDP);
	Dclose(sTCP);
	return 0;

}

void* bwc_a_bwcs() {   
    int cnt; 
    write(sUDP, buffer1, 0);
	for(;;) {
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

