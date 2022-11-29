all: server connect

server: server.o
	gcc -o server server.o
	
connect: client.o
	gcc -o connect client.o

server.o : ./Serveur/server.c
	gcc -c -Wall ./Serveur/server.c

client.o : ./Client/client.c
	gcc -c -Wall ./Client/client.c

clean :
	rm -f server.o
	rm -f client.o