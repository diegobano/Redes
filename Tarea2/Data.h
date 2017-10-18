// #define BUFFER_LENGTH 99000
#define BUFFER_LENGTH 1400
#define PORT "2000"

/* Header UDP */
#define DTYPE 0
#define DSEQ  1
#define DHDR  6

#define MAX_SEQ 100000

int Dconnect(char *hostname, char *port);
void Dbind(void* (*f)(void *), char *port);

int Dread(int cl, char *buf, int l);
void Dwrite(int cl, char *buf, int l);
void Dclose(int cl);

