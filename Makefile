CC = gcc
CFLAGS = -D_REENTRANT
LDFLAGS = -lpthread -pthread

web_server: server.c
	${CC} -Wall -o web_server server.c util.o ${LDFLAGS} -no-pie


clean:
	rm web_server webserver_log

t1:
# 	make -i clean
# 	make
	./web_server 4567 testing 100 100 0 100 0
  