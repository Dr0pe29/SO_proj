#include "api.h"
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int session_id;
int request_pipe;
int response_pipe;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  // Create pipes and connect to the server
  unlink(req_pipe_path);
  unlink(resp_pipe_path);
  // Create request pipe
  if (mkfifo(req_pipe_path, 0777) < 0){
    perror("Erro ao criar o pipe de solicitacao");
    return 1;
  }

  // Create response pipe
  if (mkfifo(resp_pipe_path, 0777) < 0) {
    perror("Erro ao criar o pipe de resposta");
    unlink(req_pipe_path);
    return 1;
  }
  // Connect to the server
  int fregister;
  if ((fregister = open(server_pipe_path, O_WRONLY)) < 0) {
    perror("Erro ao abrir o pipe do servidor para escrita");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    return 1;
  }
  //Register request to the server
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

  strcpy(msg, resp_pipe_path);
  if (write(fregister, msg, sizeof(msg)) < 0) {
    perror("Erro ao enviar a solicitacao para o servidor");
    close(fregister);
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(fregister);
    return 1;
  }
  //Open the request PIPE 
  if ((request_pipe = open(req_pipe_path, O_WRONLY)) < 0) {
    perror("Erro ao abrir o pipe de pedidos do client para escrita");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(fregister);
    return 1;
  }
  //Open the response PIPE 
  if ((response_pipe = open(resp_pipe_path, O_RDONLY)) < 0) {
    perror("Erro ao abrir o pipe de resposta do client para leitura");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(request_pipe);
    close(fregister);
    return 1;
  }
  if (read(response_pipe, &session_id, sizeof(int)) < 0) {
    perror("Erro ao ler o pipe de resposta do client");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    close(request_pipe);
    close(response_pipe);
    close(fregister);
    return 1;
  }
  close(fregister);

  return 0;
}

int ems_quit(void) { 
  //Send quit request to the server and close pipes
  char msg = '2';
  if (write(request_pipe , &msg, sizeof(char)) < 0) {
    perror("Erro ao escrever o OP_CODE do quit para o servidor");
    return 1;
  }
  if (write(request_pipe , &session_id, sizeof(int)) < 0) {
    perror("Erro ao escrever o session_id do quit para o servidor");
    return 1;    
  }
  close(response_pipe);
  close(request_pipe);
  return 1;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  //Send create request to the server and wait for the response 
  char msg = '3';
  if (write(request_pipe , &msg, sizeof(char)) < 0) {
    perror("Erro ao escrever o OP_CODE do create para o servidor");
    return 1;
  }
  if (write(request_pipe , &session_id, sizeof(int)) < 0) {
    perror("Erro ao escrever o session_id do create para o servidor");
    return 1;    
  }
  if (write(request_pipe , &event_id, sizeof(unsigned int)) < 0) {
    perror("Erro ao escrever o event_id do create para o servidor");
    return 1;    
  }
  if (write(request_pipe , &num_rows, sizeof(size_t)) < 0) {
    perror("Erro ao escrever o num_rows do create para o servidor");
    return 1;    
  }
  if (write(request_pipe , &num_cols, sizeof(size_t)) < 0) {
    perror("Erro ao escrever o num_cols do create para o servidor");
    return 1;    
  }
  
  int base_return;
  if (read(response_pipe, &base_return, sizeof(int)) < 0) {
    perror("Erro ao ler o retorno da base do create no client");
    return 1;  
  }
  return base_return;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  //Send reserve request to the server and wait for the response
  char msg = '4';
  if (write(request_pipe , &msg, sizeof(char)) < 0) {
    perror("Erro ao escrever o OP_CODE do reserve para o servidor");
    return 1;
  }
  if (write(request_pipe , &session_id, sizeof(int)) < 0) {
    perror("Erro ao escrever o session_id do reserve para o servidor");
    return 1;    
  }
  if (write(request_pipe , &event_id, sizeof(unsigned int)) < 0) {
    perror("Erro ao escrever o event_id do reserve para o servidor");
    return 1;    
  }
  if (write(request_pipe , &num_seats, sizeof(size_t)) < 0) {
    perror("Erro ao escrever o num_seats do reserve para o servidor");
    return 1;    
  }
  if (write(request_pipe , xs, num_seats * sizeof(size_t)) < 0) {
    perror("Erro ao escrever o xs do reserve para o servidor");
    return 1;    
  }
  if (write(request_pipe , ys, num_seats * sizeof(size_t)) < 0) {
    perror("Erro ao escrever o ys do reserve para o servidor");
    return 1;    
  }
  
  int base_return;
  if (read(response_pipe, &base_return, sizeof(int)) < 0) {
    perror("Erro ao ler o retorno da base do reserve no client");
    return 1;  
  }
  return base_return;
}

int ems_show(int out_fd, unsigned int event_id) {
  //Send show request to the server, wait for the response and write the seats
  //of event_id in out_fd
  char msg = '5';
  if (write(request_pipe , &msg, sizeof(char)) < 0) {
    perror("Erro ao escrever o OP_CODE do show para o servidor");
    return 1;
  }
  if (write(request_pipe , &session_id, sizeof(int)) < 0) {
    perror("Erro ao escrever o session_id do show para o servidor");
    return 1;    
  }
  if (write(request_pipe , &event_id, sizeof(unsigned int)) < 0) {
    perror("Erro ao escrever o event_id do show para o servidor");
    return 1;    
  }
  int base_return;
  if (read(response_pipe, &base_return, sizeof(int)) < 0) {
    perror("Erro ao ler o retorno da base do show no client");
    return 1;  
  }
  if (base_return) return 1;

  size_t row, col;
  if (read(response_pipe, &row, sizeof(size_t)) < 0) {
    perror("Erro ao ler o row do show no client");
    return 1;  
  }
  if (read(response_pipe, &col, sizeof(size_t)) < 0) {
    perror("Erro ao ler o col do show no client");
    return 1;  
  }

  unsigned int seats[row*col];
  if (read(response_pipe, seats, sizeof(unsigned int) * row * col) < 0) {
    perror("Erro ao ler o output do show");
    return 1;  
  }
  size_t count = 0;
  for (size_t i = 0; i < (row*col); i++){
    char buffer[16];
    sprintf(buffer, "%u", seats[i]);
    if (write(out_fd, buffer, strlen(buffer) ) < 0) {
      perror("Erro ao escrever o conteudo do show no output do client");
      return 1;  
    }
    count++;
    if(count < col){
      char space = ' ';
      if (write(out_fd, &space, sizeof(char) ) < 0) {
        perror("Erro ao escrever os espacos do show no output do client");
        return 1;  
      }
    }
    else{
      char new_line = '\n';
      if (write(out_fd, &new_line, sizeof(char) ) < 0) {
        perror("Erro ao escrever os new line do show no output do client");
        return 1;  
      }
      count = 0;
    }
  }
  return 0;
}

int ems_list_events(int out_fd) {
  //Send list request to the server, wait for the response and write a list of
  //events in out_fd
  char msg = '6';
  if (write(request_pipe , &msg, sizeof(char)) < 0) {
    perror("Erro ao escrever o OP_CODE do list_events para o servidor");
    return 1;
  }
  int base_return;
  if (read(response_pipe, &base_return, sizeof(int)) < 0) {
    perror("Erro ao ler o retorno da base do list_events no client");
    return 1;  
  }
  if(base_return) return 1;
  size_t num_events;
  if (read(response_pipe, &num_events, sizeof(size_t)) < 0) {
    perror("Erro ao ler o num_events do list no client");
    return 1;  
  }
  unsigned int list[num_events];
  if (read(response_pipe, list, sizeof(unsigned int) * num_events) < 0) {
    perror("Erro ao ler o output do list no client");
    return 1;  
  }
  for (size_t i = 0; i < num_events; i++){
    char buff[] = "Event: ";
    if (write(out_fd, buff, strlen(buff) ) < 0) {
      perror("Erro ao escrever Event:  do list no output do client");
      return 1;  
    }
    char id[16];
    sprintf(id, "%u\n", list[i]);
    if (write(out_fd, id, strlen(id)) < 0) {
      perror("Erro ao escrever o id_event do list no output do client");
      return 1;  
    }
  }
  return 0;
}
