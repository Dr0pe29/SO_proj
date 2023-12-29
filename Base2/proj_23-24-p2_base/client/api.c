#include "api.h"
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

int session_id;
int request_pipe;
int response_pipe;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  //TODO: create pipes and connect to the server
  // Criar request pipe
  unlink(req_pipe_path);
  unlink(resp_pipe_path);

  if (mkfifo(req_pipe_path, 0777) < 0){
    perror("Erro ao criar o pipe de solicitacao");
    return 1;
  }

  // Criar response pipe
  if (mkfifo(resp_pipe_path, 0777) < 0) {
    perror("Erro ao criar o pipe de resposta");
    unlink(req_pipe_path);
    return 1;
  }
  // Conectar ao servidor
  int fregister;
  if ((fregister = open(server_pipe_path, O_WRONLY)) < 0) {
    perror("Erro ao abrir o pipe do servidor para escrita");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    return 1;
  }
  //Register request to the server
  // Send the request string to the server
  char msg[40];
  strcpy(msg, "1");
  if (write(fregister, msg, sizeof(char)) < 0) {
    perror("Erro ao enviar a solicitacao para o servidor");
    close(fregister);
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(fregister);
    return 1;
  }
  strcpy(msg, req_pipe_path);
  if (write(fregister, msg, sizeof(msg)) < 0) {
    perror("Erro ao enviar a solicitacao para o servidor");
    close(fregister);
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(fregister);
    return 1;
  }
  //printf("%s, %ld\n", msg, sizeof(msg));
  strcpy(msg, resp_pipe_path);
  if (write(fregister, msg, sizeof(msg)) < 0) {
    perror("Erro ao enviar a solicitacao para o servidor");
    close(fregister);
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(fregister);
    return 1;
  }
  //Abrir o request pipe para escrita
  if ((request_pipe = open(req_pipe_path, O_WRONLY)) < 0) {
    perror("Erro ao abrir o pipe de pedidos do client para escrita");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(fregister);
    return 1;
  }
  //Abrir o response pipe para leitura
  if ((response_pipe = open(resp_pipe_path, O_RDONLY)) < 0) {
    perror("Erro ao abrir o pipe de resposta do client para leitura");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(req_pipe_path);
    close(fregister);
    return 1;
  }
  if (read(response_pipe, &session_id, sizeof(int)) < 0) {
    perror("Erro ao ler o pipe de resposta do client");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(req_pipe_path);
    close(resp_pipe_path);
    close(fregister);
    return 1;
  }
  
  //printf("%s, %d\n", msg, sizeof(msg));
  close(fregister);

  return 0;
}

int ems_quit(void) { 
  char msg = '2';
  if (write(request_pipe , msg, sizeof(char)) < 0) {
    perror("Erro ao escrever o OP_CODE do quit para o servidor");
    return 1;
  }
  if (write(request_pipe , &session_id, sizeof(char)) < 0) {
    perror("Erro ao escrever o session_id para o servidor");
    return 1;    
  }
  close(response_pipe);
  close(request_pipe);
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
