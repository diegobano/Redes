#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "Data.h"


char buffer[BUFFER_LENGTH];

/*
 * Servidor medidor de ancho de banda, con mensajes sobre TCP
 * Al terminar el archivo, envío un mensaje de largo 0
 */

void* server(void *pcl);

int main(int argc, char **argv) {
    char opt; 

    Dbind(server, PORT); /* no retorna */
}


void* server(void *pcl) {
    int n, nl;
    int bytes, cnt, packs;
    struct timeval t0, t1, t2;
    double t;
    char tmpfilename[15];
    int fd;
    int cl;

    cl = *((int *)pcl);
    free(pcl);

    strcpy(tmpfilename, "tmpbwXXXXXX");

    fd = mkstemp(tmpfilename);
    if(fd < 0) {
	fprintf(stderr, "Can't create temp %s\n", tmpfilename);
	exit(1);
    }

    fprintf(stderr, "cliente conectado\n");

    gettimeofday(&t0, NULL);
    for(bytes=0,packs=0;; bytes+=cnt,packs++) {
	cnt = Dread(cl, buffer, BUFFER_LENGTH);
	if(cnt <= 0) break;
	write(fd, buffer, cnt);
    }

    gettimeofday(&t1, NULL);
    t = (t1.tv_sec+t1.tv_usec*1e-6)-(t0.tv_sec+t0.tv_usec*1e-6); 
    fprintf(stderr, "read %d bytes in %g seconds at %g Mbps, %d packs\n", bytes, t, bytes*8.0/1024/1024/t, packs);

    n = lseek(fd, 0, SEEK_CUR);
    lseek(fd, 0, SEEK_SET);

    for(bytes=0,packs=0; bytes <= n-BUFFER_LENGTH; bytes+=BUFFER_LENGTH,packs++) {
	if(read(fd, buffer, BUFFER_LENGTH) != BUFFER_LENGTH) {
	    fprintf(stderr, "premature EOF!!!\n");
	    exit(1);
	}
        Dwrite(cl, buffer, BUFFER_LENGTH);
    }

    if(n-bytes > 0) {
	if(read(fd, buffer, n-bytes) != n-bytes) {
	    fprintf(stderr, "premature EOF!!!\n");
	    exit(1);
	}
        Dwrite(cl, buffer, n-bytes);
    }

    Dwrite(cl, buffer, 0);

    gettimeofday(&t2, NULL);
    t = (t2.tv_sec+t2.tv_usec*1e-6)-(t1.tv_sec+t1.tv_usec*1e-6); 
    fprintf(stderr, "write %d bytes in %g seconds at %g Mbps, %d packs\n", n, t, n*8.0/1024/1024/t, packs);
    close(fd);
    Dclose(cl);
    return NULL;
}
