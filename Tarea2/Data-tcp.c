#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <string.h>
#include "jsocket6.h"
#include "Data.h"

/*
 * Data layer para un sistema de mensajes sobre TCP:
 * Cada mensaje va con 5 bytes de header, que es su largo en ASCII
 * Un mensaje de largo 1024 viene entonces con 5 bytes:
 * '0' '1' '0' '2' '4'
 */

#define min(x, y) ((x) < (y) ? (x) : (y))

int Dconnect(char *server, char *port) {
    int s;

    s = j_socket_tcp_connect(server, port);
    return s;
}

void Dbind(void* (*f)(void *), char *port) {
    int s, s2;
    int *p;

    s = j_socket_tcp_bind(port);
    if(s < 0) {
	fprintf(stderr, "bind failed\n");
	exit(1);
    }

    for(;;) {
	s2 = j_accept(s);
	p = (int *)malloc(sizeof(int));
	*p = s2;
	f((void *)p);
    }
}

/* Conversion string-> entero de secuencia */
int to_int_seq(unsigned char *buf) {
    int res=0;
    int i;

    for(i=0; i < 5; i++)
        res = (res*10)+(buf[i]-'0');

// fprintf(stderr, "to_int %d <- %c, %c, %c, %c, %c\n", res, buf[0], buf[1], buf[2], buf[3], buf[4]);

    return res;
}

/* Conversion entero-> string de secuencia */
void to_char_seq(int seq, unsigned char *buf) {
    int i;
    int res = seq;

    for(i=4; i >= 0; i--) {
        buf[i] = (res % 10) + '0';
        res /= 10;
    }
// fprintf(stderr, "to_char %d -> %c, %c, %c, %c, %c\n", seq, buf[0], buf[1], buf[2], buf[3], buf[4]);
}


int Dread(int cl, char *buf, int l) {
int cnt, pos;
unsigned char sz[5];
int size;

/* Es un paquete de datos: 5 bytes con digitos 0-9 con el largo */
    pos = 0; size = 5;
    while(size > 0) {
        cnt = read(cl, sz+pos, size);
	if(cnt <= 0) return cnt;
	size -= cnt;
	pos += cnt;
    }
    
    size = to_int_seq(sz);
    if(size < 0 || size > 99999) {
	fprintf(stderr, "Largo invalido, no debe pasar nunca!\n");
	exit(1);
    }
    if(size > l) {
	fprintf(stderr, "Paquete demasiado grande!\n");
	exit(1);
    }

// fprintf(stderr, "Dread: %d bytes\n", size);
    pos = 0;
    while(size > 0) {
        cnt = read(cl, buf+pos, size);
	if(cnt <= 0) {
	    fprintf(stderr, "Dread: bad read returned: %d\n", cnt);
	    return cnt;
 	}
	size -= cnt;
	pos += cnt;
    }

    return pos;
}

void Dwrite(int cl, char *buf, int l) {
unsigned char sz[5];

    if(l < 0 || l > 99999) {
	fprintf(stderr, "Dwrite: demasiado largo!\n");
	exit(1);
    }
    to_char_seq(l, sz);
    if(write(cl, sz, 5) != 5) {
	perror("falló write en el socket");
	exit(1);
    }
    if(l > 0) {
	if(write(cl, buf, l) != l) {
	    perror("falló write en el socket");
	    exit(1);
	}
    }
// fprintf(stderr, "Dwrite: %d bytes \n", l);
}

void Dclose(int cl) {
    close(cl);
}
