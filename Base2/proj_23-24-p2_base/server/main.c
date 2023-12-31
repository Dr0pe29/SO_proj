#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"

typedef struct{
  int req_pipe;
  int resp_pipe;
}client_t;

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
  if (mkfifo(server_name, 0777) < 0) return 1;

  if((fregister = open(server_name, O_RDONLY)) < 0) {
    perror("Erro ao abrir o pipe do servidor para leitura");
    unlink(server_name);
    return 1;
  }
  //Create array to store open sessions
  client_t prod_cons_buffer[MAX_SESSION_COUNT];
  int active_sessions = 0;

  int exit = 0;
  
  while (exit == 0) {
    //TODO: Read from pipe
    char msg;
    ssize_t ret = read(fregister, &msg, sizeof(char));
    if (ret < 0) {
      perror("Erro ao ler tipo de mensagem do pipe");
      break;
    } else if (ret == 0){// ret == 0 signals EOF
      fprintf(stderr, "[INFO]: register pipe closed\n");
      break;
    }
    
    switch (msg){
      case '1': //setup request
        char req_pipe_path[40];
        char resp_pipe_path[40];
        if (read(fregister, req_pipe_path, sizeof(req_pipe_path)) < 0) {
          perror("Erro ao ler caminho do pipe de solicitacao");
          exit = 1;
          break;
        }
        
        if (read(fregister, resp_pipe_path, sizeof(resp_pipe_path)) < 0) {
          perror("Erro ao ler caminho do pipe de resposta");
          exit = 1;
          break;
        }
        //setup request pipe
        int frequest;
        if((frequest = open(req_pipe_path, O_RDONLY)) < 0) {
          perror("Erro ao abrir o pipe de requests para leitura no servidor");
          exit = 1;
          break;
        }
        prod_cons_buffer[active_sessions].req_pipe = frequest;
        //setup response pipe
        int fresponse;
        if ((fresponse = open(resp_pipe_path, O_WRONLY)) < 0) {
          perror("Erro ao abrir o pipe de requests para escrita no servidor");
          exit = 1;
          unlink(req_pipe_path);
          break;
        }
        prod_cons_buffer[active_sessions].resp_pipe = fresponse;
        
        if (write(fresponse, &active_sessions, sizeof(int)) < 0) {
          perror("Erro ao enviar a resposta para o client no servidor");
          exit = 1;
          unlink(resp_pipe_path);
          unlink(req_pipe_path);
          break;
        }
        
        active_sessions ++;
        break;
      
      default:
        exit = 1;
        break;
    }
    //TODO: Write new client to the producer-consumer buffer
  }
  //Futura função executada pelas threads
  int frequest = prod_cons_buffer[0].req_pipe;
  int fresponse = prod_cons_buffer[0].resp_pipe;
  exit = 0;
  while(!exit){
    char request;
    unsigned int event_id;
    int sid;
    size_t num_rows, num_cols, num_seats;
    ssize_t red = read(frequest, &request, sizeof(char));
    if (red < 0) {
      perror("Erro ao ler o pedido do client no servidor");
      break;
    } /*else if (red == 0){
      fprintf(stderr, "[INFO]: request pipe closed\n");
      break;
    }*/

    switch (request) {
      case '2':
        int id;
        if (read(frequest, &id, sizeof(int)) < 0) {
          perror("Erro ao ler o sessionID no servidor");
          break;
        }
        close(prod_cons_buffer[id].req_pipe);
        close(prod_cons_buffer[id].resp_pipe);
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
        ems_show(prod_cons_buffer[0].resp_pipe, event_id);
        break;
      case '6':
        ems_list_events(prod_cons_buffer[0].resp_pipe);
        break;
      default:
        break;
      }
  }

  //TODO: Close Server
  close(fregister);
  unlink(server_name);
  ems_terminate();
}