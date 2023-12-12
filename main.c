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
/*#include "parser.h"*/

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;

  if (argc > 1) {
    char *endptr;
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
  char *endptr;
  long int MAX_PROC = strtol(argv[3], &endptr, 10);
  if (*endptr != '\0'){
    fprintf(stderr, "Invalid process value\n");
    return 1;
  }
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
      ems_read_command(fd, outputFd);
      /*
      while (!isDone) {
        unsigned int event_id, delay;
        size_t num_rows, num_columns, num_coords;
        size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
        switch (get_next(fd)) {
          
          case CMD_CREATE:
            if (parse_create(fd, &event_id, &num_rows, &num_columns) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (ems_create(event_id, num_rows, num_columns)) {
              fprintf(stderr, "Failed to create event\n");
            }

            break;

          case CMD_RESERVE:
            num_coords = parse_reserve(fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);

            if (num_coords == 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (ems_reserve(event_id, num_coords, xs, ys)) {
              fprintf(stderr, "Failed to reserve seats\n");
            }

            break;

          case CMD_SHOW:
            if (parse_show(fd, &event_id) != 0) {
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (ems_show(event_id, outputFd)) {
              fprintf(stderr, "Failed to show event\n");
            }

            break;

          case CMD_LIST_EVENTS:
            if (ems_list_events(outputFd)) {
              fprintf(stderr, "Failed to list events\n");
            }

            break;

          case CMD_WAIT:
            if (parse_wait(fd, &delay, NULL) == -1) {  // thread_id is not implemented
              fprintf(stderr, "Invalid command. See HELP for usage\n");
              continue;
            }

            if (delay > 0) {
              printf("Waiting...\n");
              ems_wait(delay);
            }

            break;

          case CMD_INVALID:
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
            if (close(fd) == -1 || close(outputFd) == -1)
              fprintf(stderr, "Error closing file %s\n", dp->d_name);
            isDone = 1;
            break;
        }  
      }*/
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
