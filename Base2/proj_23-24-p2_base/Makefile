CC = gcc
SERVER = /home/pedromacedo/Desktop/SO_Proj/SO_proj-1/Base2/proj_23-24-p2_base/serverPipe
REQUEST = /home/pedromacedo/Desktop/SO_Proj/SO_proj-1/Base2/proj_23-24-p2_base/requestPipe
RESPONSE = /home/pedromacedo/Desktop/SO_Proj/SO_proj-1/Base2/proj_23-24-p2_base/responsePipe
JOBS = /home/pedromacedo/Desktop/SO_Proj/SO_proj-1/Base2/proj_23-24-p2_base/jobs/a.jobs
# Para mais informações sobre as flags de warning, consulte a informação adicional no lab_ferramentas
CFLAGS = -g -std=c17 -D_POSIX_C_SOURCE=200809L -I. \
		 -Wall -Wextra \
		 -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused \
		 -pthread
 -fsanitize=thread -fsanitize=undefined 


ifneq ($(shell uname -s),Darwin) # if not MacOS
	CFLAGS += -fmax-errors=5
endif

all: server/ems client/client

server/ems: common/io.o common/constants.h server/main.c server/operations.o server/eventlist.o
	$(CC) $(CFLAGS) $(SLEEP) -o $@ $^

client/client: common/io.o client/main.c client/api.o client/parser.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c %.h
	$(CC) $(CFLAGS) -c ${@:.o=.c} -o $@

rserver: server/ems 
	@./server/ems $(SERVER) 
clean:
	rm -f common/*.o client/*.o server/*.o server/ems client/client

format:
	@which clang-format >/dev/null 2>&1 || echo "Please install clang-format to run this command"
	clang-format -i common/*.c common/*.h client/*.c client/*.h server/*.c server/*.h
rclient:
	@./client $(REQUEST) $(RESPONSE) $(SERVER) $(JOBS)
