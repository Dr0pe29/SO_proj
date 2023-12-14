#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h> 
#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

pthread_mutex_t trinco = PTHREAD_MUTEX_INITIALIZER;
int isFileClosed = 0;
unsigned int* waitlist;

typedef struct {
    int input_fd;
    int output_fd;
    int tid;
    unsigned int* wait;
} command_args_t;


void *ems_read_command(void *arg){
  command_args_t const *args = (command_args_t const *)arg;
  
  isFileClosed = 0; //ESTA LINHA É DO DEMONIO, NAO SEI COMO EVITAR DATA RACES
 
  while (!isFileClosed) {
    unsigned int event_id, delay, thread_id = 0;
    size_t num_rows, num_columns, num_coords;
    size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];

    pthread_mutex_lock(&trinco);
    if (isFileClosed) {
        pthread_mutex_unlock(&trinco);
        break;
    }
    //printf("sou %u vou esperar %u\n", args->tid,args->wait[args->tid]);
    if(args->wait[args->tid] != 0){
      printf("entrei e sou %u vou esperar %u\n", args->tid,args->wait[args->tid]);
      ems_wait(args->wait[args->tid]);
      args->wait[args->tid] = 0;
    }
    pthread_mutex_unlock(&trinco);
    pthread_mutex_lock(&trinco);
    enum Command next_command = get_next(args->input_fd);
    pthread_mutex_unlock(&trinco);
    
    
    switch (next_command){
      case CMD_CREATE:
        pthread_mutex_lock(&trinco);
        if (parse_create(args->input_fd, &event_id, &num_rows, &num_columns) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (ems_create(event_id, num_rows, num_columns)) {
          fprintf(stderr, "Failed to create event\n");
        }
        pthread_mutex_unlock(&trinco);
        break;

      case CMD_RESERVE:
        pthread_mutex_lock(&trinco);
        num_coords = parse_reserve(args->input_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);

        if (num_coords == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (ems_reserve(event_id, num_coords, xs, ys)) {
          fprintf(stderr, "Failed to reserve seats\n");
        }
        pthread_mutex_unlock(&trinco);
        break;

      case CMD_SHOW:
        pthread_mutex_lock(&trinco);
        if (parse_show(args->input_fd, &event_id) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (ems_show(event_id, args->output_fd)) {
          fprintf(stderr, "Failed to show event\n");
        }
        pthread_mutex_unlock(&trinco);
        break;

      case CMD_LIST_EVENTS:
        pthread_mutex_lock(&trinco);
        if (ems_list_events(args->output_fd)) {
          fprintf(stderr, "Failed to list events\n");
        }
        pthread_mutex_unlock(&trinco);
        break;

      case CMD_WAIT:
        pthread_mutex_lock(&trinco);
        if (parse_wait(args->input_fd, &delay, &thread_id) == -1) {  // thread_id is not implemented
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        
        if(thread_id > 0){
          args->wait[thread_id] = delay;
          printf("vou esperar %u na proxima %u\n", args->wait[thread_id], thread_id);
          pthread_mutex_unlock(&trinco);
          break;
        }
        if (delay > 0) {
          printf("Waiting...\n");
          ems_wait(delay);
        }
        pthread_mutex_unlock(&trinco);
        break;

      case CMD_INVALID:
        printf("%d", next_command);
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_HELP:
        printf(
            "Available commands:\n"
            "  CREATE <event_id> <num_rows> <num_columns>\n"
            "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
            "  SHOW <event_id>\n"
            "  LIST\n"
            "  WAIT <delay_ms> [thread_id]\n"  // thread_id is not implemented
            "  BARRIER\n"                      // Not implemented
            "  HELP\n");

        break;

      case CMD_BARRIER:  // Not implemented
      case CMD_EMPTY:
        break;

      case EOC:
        pthread_mutex_lock(&trinco);
        if(isFileClosed){
          break;
        } 
        if (close(args->input_fd) == -1 || close(args->output_fd) == -1)
          fprintf(stderr, "Error closing file %lu \n", pthread_self());
        isFileClosed = 1;
        pthread_mutex_unlock(&trinco);
        break;
    }  
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  char *endptr;
  if (argc > 1) {
    unsigned long int delay = strtoul(argv[1], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }
  
  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  long int MAX_PROC = strtol(argv[3], &endptr, 10);

  long int MAX_THREADS = strtol(argv[4], &endptr, 10);
  long unsigned int MAX_THREADS_INT = strtoul(argv[4], &endptr, 10);

  int active_process = 0;

  DIR *dirp;
  struct dirent *dp ;
  dirp = opendir(argv[2]);
  if (dirp == NULL) {
      fprintf(stderr,"Opendir failed\n");
      return 1;
  }
  while((dp = readdir(dirp)) != NULL){
    int fd, outputFd, openFlags/*, isDone = 0;*/;
    mode_t filePerms;
    openFlags = O_CREAT | O_WRONLY | O_TRUNC;
    filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH; 
    pid_t pid;
    pthread_t tid[MAX_THREADS];

    if (strstr(dp->d_name, ".job") == NULL) continue;

    char* jobsPath = (char *)malloc(strlen(argv[2]) + strlen(dp->d_name) + 3);
    strcpy(jobsPath, argv[2]);
    strcat(jobsPath, "/");
    strcat(jobsPath ,dp->d_name);

    pid = fork();
    if (pid == -1){
      fprintf(stderr,"Error to fork \n");
      free(jobsPath);
      return 1;
    }
    if (pid == 0){
      fd = open(jobsPath, O_RDONLY); //open input file

      strcpy(jobsPath + strlen(jobsPath) - strlen(".jobs"), ".out");

      outputFd = open(jobsPath, openFlags, filePerms);

      if (fd == -1){
        fprintf(stderr, "Error opening file %s\n", dp->d_name);
      }

      command_args_t args[MAX_THREADS];

      for(int i = 0; i < MAX_THREADS; i++){
        waitlist = (unsigned int*)calloc(MAX_THREADS_INT+1,sizeof(unsigned int));
        command_args_t command_args = {.input_fd = fd, .output_fd = outputFd,.tid = i+1, .wait = waitlist};
        args[i] = command_args;
      }
      //waitlist = (unsigned int*)calloc(MAX_THREADS_INT+1,sizeof(unsigned int));
      
      //command_args_t command_args = {.input_fd = fd, .output_fd = outputFd, .wait = waitlist};
      
      for (int i = 0; i < MAX_THREADS; i++){
        if(pthread_create(&tid[i], NULL,ems_read_command,(void *)&args[i]) != 0){
          fprintf(stderr, "failed to create thread");
          exit(EXIT_FAILURE);
        }
      }

      for (int i = 0; i < MAX_THREADS; ++i){
        pthread_join(tid[i], NULL);
      }
      free(waitlist);
      exit(EXIT_SUCCESS);
    } 
    else {
      active_process ++;
      if (active_process >= MAX_PROC){
        int s;
        wait(&s);
        if (WIFEXITED(s)) {
            printf("Child process exited normally with status: %d\n", WEXITSTATUS(s));
        } else {
            printf("Child process did not exit normally.\n");
        }
        active_process --;
      }
    }
    free(jobsPath);
  }
  /*não percebi pk que se tem que fazer isto:*/
  while (active_process > 0){
    int s;
    wait(&s);
    if (WIFEXITED(s)) {
            printf("Child process exited normally with status: %d\n", WEXITSTATUS(s));
    } else {
            printf("Child process did not exit normally.\n");
    }
    active_process --;
  }
  
  closedir(dirp);
  ems_terminate();
  return 0;
}
