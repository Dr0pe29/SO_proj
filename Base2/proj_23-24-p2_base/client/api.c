#include "api.h"
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  //TODO: create pipes and connect to the server
  // Criar request pipe
  unlink(req_pipe_path);
  unlink(resp_pipe_path);

  if (mkfifo(req_pipe_path, 0666) < 0){
    perror("Erro ao criar o pipe de solicitacao");
    return 1;
  }

  // Criar response pipe
  if (mkfifo(resp_pipe_path, 0666) < 0) {
    perror("Erro ao criar o pipe de resposta");
    unlink(req_pipe_path);
    return 1;
  }
  // Conectar ao servidor
  int frequests;
  if ((frequests = open(server_pipe_path, O_WRONLY)) < 0) {
    perror("Erro ao abrir o pipe do servidor para escrita");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    return 1;
  }
  //Register request to the server

  char msg[82];
  msg[0] = '\0'; 
  strcat(msg, "1");
  strcat(msg, req_pipe_path);
  strcat(msg, resp_pipe_path);
  // Send the request string to the server
  if (write(frequests, msg, strlen(msg) + 1) < 0) {
    perror("Erro ao enviar a solicitacao para o servidor");
    close(frequests);
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    return 1;
  }

  close(frequests);

  return 0;
}

int ems_quit(void) { 
  //TODO: close pipes
  return 1;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //TODO: send create request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //TODO: send reserve request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_show(int out_fd, unsigned int event_id) {
  //TODO: send show request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}

int ems_list_events(int out_fd) {
  //TODO: send list request to the server (through the request pipe) and wait for the response (through the response pipe)
  return 1;
}
