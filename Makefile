all: serv connect

serv: server.o
	gcc -o serv server.o
	
connect: client.o
	gcc -o connect client.o

server.o : ./Server/server.c
	gcc -c -Wall ./Server/server.c

client.o : ./Client/client.c
	gcc -c -Wall ./Client/client.c

clean :
	rm -f server.o
	rm -f client.o
	rm -f connect
	rm -f serv
