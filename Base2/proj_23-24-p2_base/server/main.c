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
  int exit = 0;
  while (exit == 0) {
    //TODO: Read from pipe
    char msg;
    ssize_t ret = read(fregister, &msg, sizeof(char));
    printf("%lu\n", ret);
    if (ret < 0) {
      perror("Erro ao ler tipo de mensagem do pipe");
      break;
    } else if (ret == 0){// ret == 0 signals EOF
      fprintf(stderr, "[INFO]: pipe closed\n");
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
        printf("%s\n", req_pipe_path);
        if (read(fregister, resp_pipe_path, sizeof(resp_pipe_path)) < 0) {
          perror("Erro ao ler caminho do pipe de resposta");
          exit = 1;
          break;
        }
        break;
      
      default:
        exit = 1;
        break;
    }
    //TODO: Write new client to the producer-consumer buffer
  }

  //TODO: Close Server
  close(fregister);
  unlink(server_name);
  ems_terminate();
}