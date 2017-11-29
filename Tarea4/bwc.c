#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <fcntl.h>
#include "jsocket6.h"
#include "Data.h"

char buffer[BUFFER_LENGTH];

int main(int argc, char **argv) {
    int s;
    int n;
    int bytes, cnt;
    double t;
    struct timeval t0, t1, t2;
    char *server, *file, *file2, *port;
    int fd, fd2;
   
    if(argc == 1) {
	fprintf(stderr, "Use: bwc filename1 filename2 [servername [port]]\n");
	return 1;
    }

    if(argc == 3) {
	file = argv[1];
	file2 = argv[2];
	server = "localhost";
	port = PORT;
    }
    else if(argc == 4) {
	file = argv[1];
	file2 = argv[2];
	server = argv[3];
	port = port;
    }
    else if(argc == 5) {
	file = argv[1];
	file2 = argv[2];
	server = argv[3];
	port = argv[4];
    }
    else {
	fprintf(stderr, "Use: bwc filename1 filename2 [servername [port]]\n");
	return 1;
    }

    fd = open(file, O_RDONLY);
    if(fd < 0) {
	fprintf(stderr, "Cannot open: %s\n", file);
	perror("open");
	exit(1);
    }

    fd2 = open(file2, O_WRONLY|O_CREAT|O_TRUNC,0664);
    if(fd2 < 0) {
	fprintf(stderr, "Cannot open for writing: %s\n", file2);
	perror("open");
	exit(1);
    }

    s = Dconnect(server, port);
    if(s < 0) {
	printf("connect failed\n");
       	exit(1);
    }

    printf("conectado\n");

    gettimeofday(&t0, NULL);
    for(bytes=0;;bytes+=cnt) {
	if((cnt=read(fd, buffer, BUFFER_LENGTH)) <= 0)
	    break;
        Dwrite(s, buffer, cnt);
    }
    Dwrite(s, buffer, 0);

    gettimeofday(&t1, NULL);
    t = (t1.tv_sec*1.0+t1.tv_usec*1e-6) - (t0.tv_sec*1.0+t0.tv_usec*1e-6); 
    fprintf(stderr, "write %d bytes in %g seconds at %g Mbps\n", bytes, t, bytes*8.0/1024/1024/t);

    for(bytes=0;; bytes+=cnt) {
        cnt = Dread(s, buffer, BUFFER_LENGTH);
	if(bytes == 0) gettimeofday(&t1, NULL); /* Mejor esperar al primer read para empezar a contar */
	if(cnt <= 0)
	    break;
	write(fd2, buffer, cnt);
    }
    gettimeofday(&t2, NULL);
    Dclose(s);
    close(fd);
    close(fd2);

    t = (t2.tv_sec*1.0+t2.tv_usec*1e-6) - (t1.tv_sec*1.0+t1.tv_usec*1e-6); 
    if(bytes == 0) fprintf(stderr, "read 0 bytes!!\n");
    else
    fprintf(stderr, "read %d bytes in %g seconds at %g Mbps\n\n", bytes, t, bytes*8.0/1024/1024/t);

    t = (t2.tv_sec*1.0+t2.tv_usec*1e-6) - (t0.tv_sec*1.0+t0.tv_usec*1e-6); 
    fprintf(stderr, "throughput total: %d bytes in %g seconds at %g Mbps\n", bytes*2, t, bytes*2.0*8/1024/1024/t);

    exit(0);
}
