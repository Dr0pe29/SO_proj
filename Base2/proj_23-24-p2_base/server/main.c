#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

typedef struct{
  int req_pipe;
  int resp_pipe;
  int setup_correct;
}client_t;

//Server mutex
pthread_mutex_t prod_mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t prod_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t client_cond = PTHREAD_COND_INITIALIZER;

client_t prod_cons_buffer[MAX_SESSION_COUNT];
int session_bitmap[MAX_SESSION_COUNT] = {0};
int active_sessions = 0;
int clientptr = 0;

client_t setup_client(int fregister){
  client_t client;
  char req_pipe_path[40];
  char resp_pipe_path[40];
  client.setup_correct = 0;

  if (read(fregister, req_pipe_path, sizeof(req_pipe_path)) < 0) {
    perror("Erro ao ler caminho do pipe de solicitacao");
    client.setup_correct = -1;
    return client;
  }
  
  if (read(fregister, resp_pipe_path, sizeof(resp_pipe_path)) < 0) {
    perror("Erro ao ler caminho do pipe de resposta");
    client.setup_correct = -1;
    return client;
  }
  //setup request pipe
  int frequest;
  if((frequest = open(req_pipe_path, O_RDONLY)) < 0) {
    perror("Erro ao abrir o pipe de requests para leitura no servidor");
    client.setup_correct = -1;
    return client;
  }
  //printf("Atrribuir:%d\n", frequest);
  client.req_pipe = frequest;
  //setup response pipe
  int fresponse;
  if ((fresponse = open(resp_pipe_path, O_WRONLY)) < 0) {
    perror("Erro ao abrir o pipe de requests para escrita no servidor");
    client.setup_correct = -1;
    return client;
  }
  client.resp_pipe = fresponse;
  
  return client;
}

void read_client_commands(int frequest, int fresponse){
  int exit = 0;
  while(!exit){
    char request;
    unsigned int event_id;
    int sid;
    size_t num_rows, num_cols, num_seats;
    ssize_t red = read(frequest, &request, sizeof(char));
    if (red < 0) {
      perror("Erro ao ler o pedido do client no servidor");
      break;
    } 

    switch (request) {
      case '2':
        int id;
        if (read(frequest, &id, sizeof(int)) < 0) {
          perror("Erro ao ler o sessionID no servidor");
          break;
        }
        close(frequest);
        close(fresponse);
        pthread_mutex_lock(&prod_mut);
        session_bitmap[id] = 0;
        pthread_cond_signal(&prod_cond);
        if(ems_show_output(61)){
          perror("Erro ao escrever o evento no stdout");
        }
        if(ems_show_output(2)){
          perror("Erro ao escrever o evento no stdout");
        }
        pthread_mutex_unlock(&prod_mut);
        exit = 1;
        break;
      case '3':
        if (read(frequest, &sid, sizeof(int)) < 0) {
          perror("Erro ao ler o session id no servidor");
          break;
        }
        if (read(frequest, &event_id, sizeof(unsigned int)) < 0) {
          perror("Erro ao ler o eventID no servidor");
          break;
        }
        if (read(frequest, &num_rows, sizeof(size_t)) < 0) {
          perror("Erro ao ler o num filas no servidor");
          break;
        }
        if (read(frequest, &num_cols, sizeof(size_t)) < 0) {
          perror("Erro ao ler o num colunas no servidor");
          break;
        }
        int ret_create = ems_create(event_id, num_rows, num_cols);
        if (write(fresponse, &ret_create, sizeof(int)) < 0){
          perror("Erro ao escrever o output do ems_create");
          break;
        }
        break;
      case '4':
        size_t *xs, *ys;
        if (read(frequest, &sid, sizeof(int)) < 0) {
          perror("Erro ao ler o session id no servidor");
          break;
        }
        if (read(frequest, &event_id, sizeof(unsigned int)) < 0) {
          perror("Erro ao ler o eventID no servidor");
          break;
        }
        if (read(frequest, &num_seats, sizeof(size_t)) < 0) {
          perror("Erro ao ler o num lugares no servidor");
          break;
        }
        
        xs = (size_t *)malloc(num_seats * sizeof(size_t));
        ys = (size_t *)malloc(num_seats * sizeof(size_t));

        if (read(frequest, xs, num_seats*sizeof(size_t)) < 0) {
          perror("Erro ao ler o conteudo dos xs no servidor");
          free(xs);
          free(ys);
          break;
        }
        if (read(frequest, ys, num_seats*sizeof(size_t)) < 0) {
          perror("Erro ao ler o conteudo dos ys no servidor");
          free(xs);
          free(ys);
          break;
        }
        int ret_reserve = ems_reserve(event_id, num_seats, xs, ys);
        free(xs);
        free(ys);
        if (write(fresponse, &ret_reserve, sizeof(int)) < 0){
          perror("Erro ao escrever o output do ems_reserve no servidor");
          break;
        }
        break;
      case '5':
        if (read(frequest, &sid, sizeof(int)) < 0) {
          perror("Erro ao ler o session id no servidor");
          break;
        }
        if (read(frequest, &event_id, sizeof(unsigned int)) < 0) {
          perror("Erro ao ler o eventID no servidor");
          break;
        }
        ems_show(fresponse, event_id);
        break;
      case '6':
        ems_list_events(fresponse);
        break;
      default:
        break;
      }
  }
}

void *ems_client(){
  while(1){
    client_t client;
    pthread_mutex_lock(&prod_mut);
    while (active_sessions == 0) pthread_cond_wait(&client_cond, &prod_mut);
    client = prod_cons_buffer[clientptr];
    clientptr ++;
    if (clientptr == MAX_SESSION_COUNT) clientptr = 0; 
    int frequest = client.req_pipe;
    int fresponse = client.resp_pipe;
    active_sessions --;
    pthread_mutex_unlock(&prod_mut);
    read_client_commands(frequest, fresponse);
  }
}

void *ems_host(void *arg){
  int fregister = *((int *)arg);
  int prodptr = 0;
  while (1) {
    //TODO: Read from pipe
    char msg;
    ssize_t ret = read(fregister, &msg, sizeof(char));
    if (ret < 0) {
      perror("Erro ao ler tipo de mensagem do pipe");
      break;
    } else if (ret == 0){// ret == 0 signals EOF
      continue;
    }
    if(msg != '1'){ 
      fprintf(stderr, "Erro, OP-CODE do register incorreto\n");
      break;
    }

    client_t client = setup_client(fregister);
    if (client.setup_correct == -1) break;

    pthread_mutex_lock(&prod_mut);
    while (active_sessions == MAX_SESSION_COUNT) pthread_cond_wait(&prod_cond, &prod_mut);
    prod_cons_buffer[prodptr] = client;
    prodptr ++;
    if (prodptr == MAX_SESSION_COUNT) prodptr = 0;
    active_sessions++;
    
    //get session id
    int session_id = -1;
    for(int i= 0; i < MAX_SESSION_COUNT; i++){
      if(session_bitmap[i] == 0){
        session_id = i;
        session_bitmap[i] = 1;
        break;
      }
    }
    if (write(client.resp_pipe, &session_id, sizeof(int)) < 0) {
      perror("Erro ao enviar a resposta para o client no servidor");
      break;
    }
    pthread_cond_signal(&client_cond);
    pthread_mutex_unlock(&prod_mut);
    //TODO: Write new client to the producer-consumer buffer
  }
  return NULL;
}

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }
  char* server_name = argv[1];
  
  
  int fregister;
  unlink(server_name);
  //TODO: Intialize server, create worker threads
  pthread_t producer_tid, worker_tid[MAX_SESSION_COUNT];
  if (mkfifo(server_name, 0777) < 0) return 1;

 if((fregister = open(server_name, O_RDONLY)) < 0) {
    perror("Erro ao abrir o pipe do servidor para leitura");
    unlink(server_name);
    return 1;
  }
  
  //Create host thread
  if(pthread_create(&producer_tid, NULL, ems_host, (void*)&fregister) != 0){
    perror("Erro ao criar a host thread");
    close(fregister);
    unlink(server_name);
    return 1;
  }
  for(int i = 0; i < MAX_SESSION_COUNT; i++){
    if(pthread_create(&worker_tid[i], NULL, ems_client, NULL) != 0){
      perror("Erro ao criar worker threads");
      close(fregister);
      unlink(server_name);
      return 1;
    }
  }
  for(int i = 0; i < MAX_SESSION_COUNT; i++){
    if(pthread_join(worker_tid[i], NULL) != 0){
      perror("Erro ao aguardar a conclusão da thread\n");
      close(fregister);
      unlink(server_name);
      return 1;
    }
  }

  if(pthread_join(producer_tid, NULL) != 0){
    perror("Erro ao dechar a host thread");
    close(fregister);
    unlink(server_name);
    return 1;
  }

  //TODO: Close Server
  close(fregister);
  unlink(server_name);
  ems_terminate();
}