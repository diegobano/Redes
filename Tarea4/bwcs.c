#include "Data.h"
#include "jsocket6.4.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

char buffer_ttu[BUFFER_LENGTH + DHDR];
char buffer_utt[BUFFER_LENGTH + DHDR];
char buffer_ack[DHDR];

char window_ttu[WIN_SZ][BUFFER_LENGTH + DHDR];
int window_ttu_sizes[WIN_SZ];
int window_ttu_seqs[WIN_SZ];
int window_ttu_confirmed[WIN_SZ]; //nueva ventana para manejar acks que ya han sido recibidos
int window_utt_confirmed[MAX_SEQ]; //nueva ventana para manejar acks que ya han sido recibidos
int window_end = 0, window_init = 0, window_size = 0;
pthread_mutex_t window_mutex;
pthread_cond_t window_cond;

int s_tcp, s2_tcp, s_udp;
pthread_mutex_t mutex;
pthread_t ttu, utt;
pthread_t window_threads[WIN_SZ];
int timeout = WIN_SZ;

/*
  next_seq_num: next seq_num available to use
  expected_seq_num: seq_num of window_ttu[window_init]
*/
int next_seq_num = 0, seq_num_utt = 0, expected_seq_num = 0,
    seq_num_retransmit = 0, seq_num_last = -1;
int fast_retransmit = 0, RETRANSMIT = 0;
int retries = 0, end_reached = 0, empty_received = 0, acks_not_confirmed = 0;

int debug = 0;

typedef struct {
    int win_index;
} Args;

void udp_write_all(int fd);

//depreca3
void handler() {
  //udp_write_all(s_udp);
  timeout = TIMEOUT;
  //alarm(TIMEOUT);
}

void killed() {  
  pthread_mutex_destroy(&mutex);
  pthread_mutex_destroy(&window_mutex);
  pthread_cond_destroy(&window_cond);
  close(s2_tcp);
  close(s_tcp);
  close(s_udp);
  printf("\nKilled\n");
  exit(1);
}

int string_to_int(char *buf) {
  int res = 0;
  for (int i = 0; i < 5; i++)
    res = (res * 10) + (buf[i] - '0');
  return res;
}

void int_to_string(int seq, char *buf) {
  int res = seq;
  for (int i = 4; i >= 0; i--) {
    buf[i] = (res % 10) + '0';
    res /= 10;
  }
}

// a free block was left due to boundary conditions
void window_write(char *buf, int count, int seq) {
  pthread_mutex_lock(&window_mutex);
  while (window_size == WIN_SZ - 1) {
    pthread_cond_wait(&window_cond, &window_mutex);
  }
  memcpy(window_ttu[window_end], buf, count);
  window_ttu_sizes[window_end] = count;
  window_ttu_seqs[window_end] = seq;
  window_ttu_confirmed[window_end] = 0;
  window_utt_confirmed[seq] = 0;
  window_end = (window_end + 1) % WIN_SZ;
  window_size++;
  pthread_mutex_unlock(&window_mutex);
}

void udp_write(int fd, char *buf, int count, int *seq_num) {
  if (debug) {
    printf("TCPread: sending DATAAA seq=%i\n", *seq_num);
    if (window_size > 0)
      printf("win: (%i)\n", window_size);
    else
      printf("empty win\n");
  }
  write(fd, buf, count);
  *seq_num = (*seq_num + 1) % MAX_SEQ;
}

//manejo de timeout por paquete
void *manage_packet(void* arguments) {

  Args* args = (Args*) arguments;
  int win_index = args->win_index;
  int debug_timeout = 0, debug_retrans = 0;
  struct timeval ti, curr_time;  
  do {    
            
    gettimeofday(&ti, NULL);
    while (1) {
      gettimeofday(&curr_time, NULL);
      // si se envio "senhal" de retransmision y manejo el primer paquete
      if(RETRANSMIT && win_index == window_init){        
        RETRANSMIT = 0; debug_retrans = 1;
        break;
      }    
      if (curr_time.tv_sec - ti.tv_sec >= TIMEOUT || window_ttu_confirmed[win_index]) {
        debug_timeout = 1;
        break;
      }      
    }
    pthread_mutex_lock(&mutex);
    if(window_ttu_confirmed[win_index] == 0){ //si no ha sido confirmado, hubo timeout 
      if (debug){
        if (debug_timeout){
          debug_timeout = 0;
          printf("TIMEOUT para n°: %d\n", window_ttu_seqs[win_index]);
        } else if(debug_retrans){
          debug_retrans = 0;
          printf("Re-send DATA, seq=%i", window_ttu_seqs[win_index]);
        }
      }          
      write(s_udp, window_ttu[win_index], window_ttu_sizes[win_index]);
    }    
    pthread_mutex_unlock(&mutex);

  } while (window_ttu_confirmed[win_index] == 0);
  free(args);
  return NULL;
}

void *tcp_to_udp() {
  int cnt;
  write(s_udp, NULL, 0);
  //alarm(WIN_SZ);
  for (;;) {
    int_to_string(next_seq_num, buffer_ttu + DSEQ); // seq num header ttu

    cnt = Dread(s2_tcp, buffer_ttu + DHDR, BUFFER_LENGTH);

    if (debug)
      printf("TCPread: %i bytes\n", cnt);
    if (cnt == 0)
      seq_num_last = next_seq_num;
    if (cnt <= 0){
      break;
    }

    Args *args = (Args *) malloc(sizeof(Args)); args->win_index = window_end;
    window_write(buffer_ttu, cnt + DHDR, next_seq_num);  
    udp_write(s_udp, buffer_ttu, cnt + DHDR, &next_seq_num);

    if (pthread_create(&window_threads[args->win_index], NULL, manage_packet, (void *) args) < 0) {
      perror("pthread_create");
      exit(1);
    }
    
  }
  // size 0 write
  Args *args = (Args *) malloc(sizeof(Args)); args->win_index = window_end;
  window_write(buffer_ttu, DHDR, next_seq_num);
  udp_write(s_udp, buffer_ttu, DHDR, &next_seq_num);

  if (pthread_create(&window_threads[args->win_index], NULL, manage_packet, (void *) args) < 0) {
      perror("pthread_create");
      exit(1);
    }
  printf("TCPread: recibo EOF desde TCP\n");
  return NULL;
}

void *close_phase() {
  int cnt;
  // in order to solve last ack missing by bwss
  fd_set fds;
  struct timeval tv;
  tv.tv_sec = 3 * TIMEOUT;
  tv.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(s_udp, &fds);

  // 0 size package to close tcp
  Dwrite(s2_tcp, NULL, 0);
  for(;;) {
    if (select(s_udp + 1, &fds, NULL, NULL, &tv)) {
      printf("UDPread: select \n");
      cnt = read(s_udp, buffer_utt, BUFFER_LENGTH + DHDR);
    } else {
      if (debug)
        printf("nada más que leer y conexion cerrada, chao!\n");
      break;
    }

    int seq_num = string_to_int(buffer_utt + DSEQ);

    if (buffer_utt[0] == 'D') { //si recibimos datos
      if (cnt - DHDR < 0){
        printf("UDPread: ACK cnt - HDR = %i\n",cnt - DHDR);
        break;
      } else{
          if (debug)
            printf("UDPread: Enviando ACK seq=%i\n", seq_num);

          int_to_string(seq_num, buffer_ack + DSEQ); // seq num header ack
          write(s_udp, buffer_ack, DHDR); //enviar ack de confirmacion a udp                  
      }
    }
  }
  return NULL;
}

/*
  seq_num: numero de secuencias recibido
  expected_seq_num: numero de secuencia esperado
  next_seq_num: siguiente numero de secuencia a asignar
*/
void *udp_to_tcp() {
  int cnt, packets_received = 0, acks_received = 0;
  
  for (;;) {

    if (end_reached) {      
      return close_phase();
    } 
    else {      
      cnt = read(s_udp, buffer_utt, BUFFER_LENGTH + DHDR);
    }
    
    //se leyo
    if (debug)
      printf("UDPread: recv largo=%i\n", cnt);
    if (cnt <= 0) {
      printf("UDPread: LEL cnt = %i\n",cnt);
      break;
    }

    int seq_num = string_to_int(buffer_utt + DSEQ);

    /* ACKNOWLEDGEMENT*/
    if (buffer_utt[0] == 'A') { //acknowledgment
      if (cnt - DHDR < 0){
        printf("UDPread: ACK cnt - HDR = %i\n",cnt - DHDR);
        break;
      } else {
        if (debug)
          printf("UDPread: recv ACK seq=%i, expected_ack=%i\n", seq_num, expected_seq_num);

        // si ack es mayor o igual al esperado 
        // o si el siguiente a ack a asignar este entre el recibido y el esperado
        if (window_init <= seq_num && seq_num <= window_end) {                    

          if (!window_ttu_confirmed[seq_num % WIN_SZ]) {
            window_ttu_confirmed[seq_num % WIN_SZ] = 1;
            acks_received++;
            if(debug) printf("UDPread: Reception of packet %i confirmed\n", seq_num);              
          }
          
          
          if (seq_num == expected_seq_num){ //se puede mover la ventana c:
            int diff = 0;
            //se busca siguiente paquete aun no confirmado
            while(window_ttu_confirmed[(window_init + diff) % WIN_SZ] != 0) {
              pthread_join(window_threads[(window_init + diff) % WIN_SZ], NULL);
              diff++;
            }

            pthread_mutex_lock(&window_mutex);
            window_init = (window_init + diff) % WIN_SZ; //se actualiza inicio de ventana
            window_size -= diff; //se achica tamaño de ventana
            pthread_cond_broadcast(&window_cond);
            pthread_mutex_unlock(&window_mutex);

            if (debug) {
              if (window_size > 0)
                printf("win: (%i)\n", window_size);
              else
                printf("empty win\n");
            }
            expected_seq_num = (expected_seq_num + diff) % MAX_SEQ; //se actualiza nuevo ack esperado
          }

        } /* seq_num >= expected_seq_num || (seq_num < next_seq_num && next_seq_num < expected_seq_num) */
        else {
          /* condiciones para ver fast retransmit */
          if (seq_num == (MAX_SEQ + expected_seq_num - 1) % MAX_SEQ && expected_seq_num != seq_num_retransmit) {
            seq_num_retransmit = expected_seq_num;
            fast_retransmit = 1;
            retries = 0;
          } else if (++fast_retransmit == 3) {
              if (debug)
                printf("Fast Retransmit\n");
              // max number of acks for this retransmit
              fast_retransmit = 0;
              RETRANSMIT = 1; //se retransmite solo primer paquete
          }
        }
      }
    } /* buffer_utt[0] == 'A' */
    else if (buffer_utt[0] == 'D') { //si recibimos datos
      if (cnt - DHDR < 0){
        printf("UDPread: ACK cnt - HDR = %i\n",cnt - DHDR);
        break;
      } else{
        if (debug)
          printf("UDPread: DATA: seq=%i, expected_seq=%i\n", seq_num, seq_num_utt);

        if (seq_num_utt <= seq_num) { //si numero recibido es numero esperado por socket tcp
          if (debug)
            printf("UDPread: Enviando ACK seq=%i\n", seq_num_utt);

          int_to_string(seq_num, buffer_ack + DSEQ); // seq num header ack

          if(seq_num_utt == seq_num)
            seq_num_utt = (seq_num_utt + 1) % MAX_SEQ;
          write(s_udp, buffer_ack, DHDR); //enviar ack de confirmacion a udp

          if (cnt - DHDR == 0 && !empty_received){            
            if (debug)
              printf("UDPread: empty packet received\n");
            empty_received = 1;
          } else if(!window_utt_confirmed[seq_num]) { //si paquete no habia sido recibido antes
              pthread_mutex_lock(&mutex);
              window_utt_confirmed[seq_num] = 1;
              packets_received++;
              pthread_mutex_unlock(&mutex);
              printf("Reception of packet %i >= %i confirmed, packets: %i/%i\n", 
                seq_num, seq_num_utt - 1, packets_received, seq_num_last);
              Dwrite(s2_tcp, buffer_utt + DHDR, cnt - DHDR); //escribir en tcp
            }                      
        } else {
          int_to_string(seq_num, buffer_ack + DSEQ); // seq num header ack
          if (debug)
            printf("UDPread: DATA fuera de rango, envío ACK para %i\n", seq_num);
          write(s_udp, buffer_ack, DHDR);
          
        }      
        if (empty_received && seq_num_last == packets_received && seq_num_last == acks_received - 1){
          if (debug)
              printf("UDPread: end reached\n");  
          end_reached = 1;
        }
      }      
    }
    if (debug)
      printf("Estado parcial: end_reached=%i, empty_received=%i, packets_received=%i, acks_received=%i\n",
        end_reached, empty_received, packets_received, acks_received);
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  ttu = pthread_self();
  signal(SIGALRM, handler);
  signal(SIGINT, killed);
  char *server, *port_tcp, *port_udp;
  int pos[3];
  int flag_count = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      debug = 1;
      flag_count++;
    } else
      pos[i - flag_count - 1] = i;
  }
  if (argc != 4 + flag_count) {
    fprintf(stderr, "Use: bwcs [-d] servername portin portout\n");
    return 1;
  }

  server = argv[pos[0]];
  port_tcp = argv[pos[1]];
  port_udp = argv[pos[2]];

  buffer_ttu[DTYPE] = 'D';
  buffer_ack[DTYPE] = 'A';

  // base case for package 0 lost
  int_to_string(MAX_SEQ - 1, buffer_ack + DSEQ);

  if (pthread_mutex_init(&mutex, NULL) != 0) {
    fprintf(stderr, "Mutex init failed\n");
    return 1;
  }

  if (pthread_mutex_init(&window_mutex, NULL) != 0) {
    fprintf(stderr, "Mutex init failed\n");
    return 1;
  }

  if (pthread_cond_init(&window_cond, NULL) != 0) {
    fprintf(stderr, "Condition init failed\n");
    return 1;
  }

  // UDP
  s_udp = j_socket_udp_connect(server, port_udp);
  if (s_udp < 0) {
    printf("connect failed\n");
    exit(1);
  }

  // TCP
  s_tcp = j_socket_tcp_bind(port_tcp);
  if (s_tcp < 0) {
    fprintf(stderr, "bind failed\n");
    exit(1);
  }
  s2_tcp = j_accept(s_tcp);

  printf("conectado\n");

  // disble alarm for this thread
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGALRM);
  pthread_sigmask(SIG_BLOCK, &set, NULL);

  if (pthread_create(&utt, NULL, udp_to_tcp, NULL) < 0) {
    perror("pthread_create");
    exit(1);
  }

  tcp_to_udp();

  pthread_join(utt, NULL);
  printf("Murió hijo\n");

  pthread_mutex_destroy(&mutex);

  pthread_mutex_destroy(&window_mutex);
  pthread_cond_destroy(&window_cond);

  close(s2_tcp);
  close(s_tcp);
  close(s_udp);
  return 0;
}
