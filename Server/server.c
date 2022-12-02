#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "server.h"
#include "client.h"

static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if(err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#endif
}

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

static void app(void)
{
   SOCKET sock = init_connection();
   printf ("Server Trichat started\n");  
   char buffer[BUF_SIZE];
   /* the index for the array */
   int actual = 0;
   int max = sock;
   /* an array for all clients */
   Client clients[MAX_CLIENTS];

   fd_set rdfs;

   while(1)
   {
      int i = 0;
      FD_ZERO(&rdfs);

      /* add STDIN_FILENO */
      FD_SET(STDIN_FILENO, &rdfs);

      /* add the connection socket */
      FD_SET(sock, &rdfs);

      /* add socket of each client */
      for(i = 0; i < actual; i++)
      {
         FD_SET(clients[i].sock, &rdfs);
      }

      if(select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      /* something from standard input : i.e keyboard */
      if(FD_ISSET(STDIN_FILENO, &rdfs))
      {
         /* stop process when type on keyboard */
         break;
      }
      else if(FD_ISSET(sock, &rdfs))
      {
         /* new client */
         SOCKADDR_IN csin = { 0 };
         size_t sinsize = sizeof csin;
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         if(csock == SOCKET_ERROR)
         {
            perror("accept()");
            continue;
         }

         /* after connecting the client sends its name */
         if(read_client(csock, buffer) == -1) // problem : return from the function can never be -1 => always 0 at best cf. read_client function
         {
            /* disconnected */
            continue;
         }

         /* we will use the names in only lowercase => when storing and when displaying */
         int nbchar = 0;
         while(buffer[nbchar] != '\0') {
            buffer[nbchar] = tolower(buffer[nbchar]);
            ++nbchar;
         }

         /* we can't have twice the same person logged in => check if there is already a client with this name */
         int exists = 0;
         for(int i=0; i<actual; ++i) {
            if(!strcmp(clients[i].name,buffer)) { // if the name of a client is equal to the one recieved (strcmp returns 0) then we want to disconnect the client
               exists = 1;
            } 
         }
         if(exists) { // if the name was found we just continue and send a message to the client saying he can't connect with this name
            write_client(csock, "You can't connect with this username because it is already in use");
            closesocket(csock);
            printf ("User failed trying to connect as client : %s, who is already connected \n", buffer);
            continue;
         } 
         /* what is the new maximum fd ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         Client c = { csock };
         strncpy(c.name, buffer, BUF_SIZE - 1);
         clients[actual] = c;
         actual++;

         /* we write the name of the client in the clients file if it is not present (new connection) */
         /* first we read the file and search if the name provided already exists */
         FILE* fptr;
         /* we open the file in read and append => if it doesn't exist it is created */
         if((fptr = fopen("clients", "a+")) == NULL) { 
            perror("Error : Error opening file \'clients\'\n");
            exit(EXIT_FAILURE);
         }
         /* go to the start of the file just to be sure */
         fseek(fptr, 0, SEEK_SET);

         char* line = NULL;
         size_t len = 0;
         ssize_t nread;
         int found = 0;
         /* we read each line */
         while((nread = getline(&line, &len, fptr)) != -1) {
            /* we get rid of the \n because getline keeps it */
            char* c = strchr(line, '\n');
            if(c){
               *c = '\0';
            }

            /* we compare with our name buffer */
            if(strcmp(buffer, line) == 0) {
               found = 1;
            }
         }
         /* we gave a NULL buffer and 0 size to getline so it allocated memory itself but we need to free this memory ourselves after */
         free(line);

         /* if the name doesn't exist then we append it to the file */
         if(!found) {
            fwrite(buffer, nbchar, 1, fptr);
            fputc('\n', fptr);
            write_client(csock, "Welcome to Trichat ! You can now chat with your friends !");
            printf ("User connected as new client : %s\n", c.name);
         }else{
            write_client(csock, "Welcome back to Trichat ! You can now chat with your friends !");
            printf ("User connected as known client : %s\n", c.name);
         }

         /* don't forget to close the file */
         fclose(fptr);
         
      }
      else
      {
         int i = 0;
         for(i = 0; i < actual; i++)
         {
            /* a client is talking */
            if(FD_ISSET(clients[i].sock, &rdfs))
            {
               Client client = clients[i];
               int c = read_client(clients[i].sock, buffer);
               /* client disconnected */
               if(c == 0)
               {
                  closesocket(clients[i].sock);
                  remove_client(clients, i, &actual);
                  strncpy(buffer, client.name, BUF_SIZE - 1);
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);
                  Client destinataire = clients[0];
                  send_message_to_one_client(destinataire, client, actual, buffer, 1);
                  printf ("Client : %s sent a message to client : %s.\n", client.name, destinataire.name);
                  //send_message_to_all_clients(clients, client, actual, buffer, 1);
               }
               else
               {
                  // if the client sends "@name" then send the message to the client named "name"
                  if(buffer[0] == '@')
                  {
                     char *name = buffer + 1;
                     char *message = strchr(buffer, ' ');
                     if(message == NULL)
                     {
                        continue;
                     }
                     *message = 0;
                     message++;
                     Client destinataire = get_client_by_name(clients, name, actual);
                     if(destinataire.sock != -1)
                     {
                        send_message_to_one_client(destinataire, client, actual, message, 0);
                        printf ("Client : %s sent a message to client : %s.\n", client.name, destinataire.name);
                     }
                  }
                  else if(buffer[0] == '!') // the user sends "!command" to execute command "command" => for groups
                  {
                     char *command = buffer + 1;
                     char *group = strchr(buffer, ' ');
                     if(command == NULL)
                     {
                        continue;
                     }
                     // we split the buffer in two by putting a null byte to terminate the command buffer in place of the ' ' at the begining of the group name
                     *group = 0; 
                     group++;
                     if(!strcmp(command, "create")) // user wants to create a group chat
                     {
                        /* first we need to check if the conversation doesn't already exist */
                        FILE* fptr;
                        /* we open the file in read only => if it doesn't exist, it will return NULL */
                        if((fptr = fopen(group, "r")) != NULL) { 
                           perror("Error : File doesn\'t exist\n");
                           write_client(client.sock, "This conversation already exists");
                           printf ("User : %s failed trying to create existing group : %s .\n", client.name, group);
                           /* don't forget that the file was opened if we get in the if so we need to close it */
                           fclose(fptr);
                           continue;
                        }
                        /* we don't need to close the file because if we got past the if statement it means it doesn't exists and wasn't opened */

                        /* we create a file dedicated to store all users that are part of the group */
                        /* we open the file in write only => if it doesn't exist it is created */
                        if((fptr = fopen(group, "w")) == NULL) { 
                           perror("Error : File doesn\'t exist\n");
                           // write_client(client.sock, "Error : File doesn\'t exist");
                           continue;
                        }
                        /* go to the start of the file just to be sure */
                        fseek(fptr, 0, SEEK_SET);

                        /* the only person in the group when it is created is the creator */
                        fputs(client.name, fptr);
                        fputc('\n', fptr);

                        /* don't forget to close the file */
                        fclose(fptr);

                        /* when the group is created, we have to create the file to store the message history => name = groupname_histo */
                        /* first we build the filename */
                        char filename[BUF_SIZE];
                        filename[0] = 0;
                        strncpy(filename, group, BUF_SIZE - 1);
                        strncat(filename, "_histo", sizeof filename - strlen(filename) - 1);
                        /* then we open it in read append mode to create the file */
                        if((fptr = fopen(filename, "a+")) == NULL) { 
                           perror("Error : Error opening the history file\n");
                           continue;
                        }
                        /* we write the first "message" of the group which is the group creation */
                        fputs(client.name, fptr);
                        fputs(" created the group\n", fptr);
                        /* don't forget to close the file */
                        fclose(fptr);
                        printf ("User : %s created and joined group : %s .\n", client.name, group);
                     }
                     else if(!strcmp(command, "join")) // user wants to join a group chat
                     {
                        /* first we need to check if the conversation exists */
                        FILE* fptr;
                        /* we open the file in read only => if it doesn't exist, it will return NULL */
                        if((fptr = fopen(group, "r")) == NULL) { 
                           perror("Error : File doesn\'t exist.\n");
                           write_client(client.sock, "This conversation doesn\'t exist.");
                           printf ("User : %s failed trying to join unexisting group.\n", client.name);
                           continue;
                        }
                        /* we will reopen the file in write mode so we need to close it */
                        fclose(fptr);

                        /* want to add the name of the person who joins to the file */
                        /* we open the file in read append => we know it exists but it will not work if it doesn't (just in case) */
                        if((fptr = fopen(group, "a+")) == NULL) { 
                           perror("Error : File doesn\'t exist.\n");
                           write_client(client.sock, "Error : File doesn\'t exist.");
                           continue;
                        }
                        /* go to the start of the file just to be sure */
                        fseek(fptr, 0, SEEK_SET);

                        /* add the name to the file */
                        fputs(client.name, fptr);
                        fputc('\n', fptr);

                        /* don't forget to close the file */
                        fclose(fptr);

                        /* send a message to all clients in the group to let them know someone joined */
                        send_message_to_group(clients,client,actual,group,NULL,1);
                     }
                     else
                     {
                        write_client(client.sock, "Error : unknown command.");
                     }
                  }
                  else if(buffer[0] == '#') // "#group message" will send the message "message" to group named "group"
                  {
                     char *group = buffer + 1;
                     char *message = strchr(buffer, ' ');
                     if(group == NULL)
                     {
                        continue;
                     }
                     // we split the buffer in two by putting a null byte to terminate the group buffer in place of the ' ' at the begining of the message
                     *message = 0; 
                     message++;

                     /* we first need to check if the client belongs to the group */
                     /* first we read the file and search if the name exists */
                     FILE* fptr;
                     /* we open the file in read only => tests if the group exists aswell */
                     if((fptr = fopen(group, "r")) == NULL) { 
                        perror("Error : Group doesn\'t exist\n");
                        write_client(client.sock, "This group doesn\'t exist, please create it first.");
                        printf ("User : %s failed trying to send message to unexisting group.\n", client.name);
                        continue;
                     }
                     /* go to the start of the file just to be sure */
                     fseek(fptr, 0, SEEK_SET);

                     char* line = NULL;
                     size_t len = 0;
                     ssize_t nread;
                     int found = 0;
                     /* we read each line */
                     while((nread = getline(&line, &len, fptr)) != -1) {
                        /* we get rid of the \n because getline keeps it */
                        char* c = strchr(line, '\n');
                        if(c){
                           *c = '\0';
                        }

                        /* we compare with our name */
                        if(strcmp(client.name, line) == 0) {
                           found = 1;
                        }
                     }
                     /* we gave a NULL buffer and 0 size to getline so it allocated memory itself but we need to free this memory ourselves after */
                     free(line);

                     /* if the name doesn't exist then the client can't talk to this group */
                     if(!found) {
                        write_client(client.sock, "You do not belong to this group, please join it first.");
                        printf ("User : %s failed trying to send message to a group he doen't belong to.\n", client.name);
                        /* don't forget to close the file */
                        fclose(fptr);
                        continue;
                     }

                     /* if the name exists then the client belongs to the group and we want to send the message to the whole group */
                     /* close the file and call a function that will do the enumeration */
                     fclose(fptr);
                     send_message_to_group(clients, client, actual, group, message, 0);
                     printf ("User : %s sent a message to group : %s\n", client.name, group);
                  }
                  else
                  {
                     send_message_to_all_clients(clients, client, actual, buffer, 0);
                     printf("User : %s sent to message to everyone.", client.name);
                  }
                  // send_message_to_one_client(clients, client, actual, buffer, 1);
                  // send_message_to_all_clients(clients, client, actual, buffer, 0);
               }
               break;
            }
         }
      }
   }

   clear_clients(clients, actual);
   end_connection(sock);
}

static void clear_clients(Client *clients, int actual)
{
   int i = 0;
   for(i = 0; i < actual; i++)
   {
      closesocket(clients[i].sock);
   }
}

static void remove_client(Client *clients, int to_remove, int *actual)
{
   /* we remove the client in the array */
   memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(Client));
   /* number client - 1 */
   (*actual)--;
}

static void send_message_to_group(Client* clients, Client sender, int nbClients, const char* groupname, const char* message, char from_server)
{
   /* 
   we want to :
      1) read the file that contains the names of the participants *THAT ARE CONNECTED*
      2) get their corresponding sockets
      3) send the message
   */
   SOCKET socks[100]; // we consider that we can't have more than 100 clients connected at the same time in the group => TODO : make sure it is impossible
   int countSocks = 0;

   FILE* fptr;
   /* we open the file in read only => tests if the group exists aswell (which it should) */
   if((fptr = fopen(groupname, "r")) == NULL) { 
      perror("Error : Group doesn\'t exist\n");
      exit(EXIT_FAILURE);
   }
   /* go to the start of the file just to be sure */
   fseek(fptr, 0, SEEK_SET);

   char* line = NULL;
   size_t len = 0;
   ssize_t nread;
   /* we read each line */
   while((nread = getline(&line, &len, fptr)) != -1) {
      /* we get rid of the \n because getline keeps it */
      char* c = strchr(line, '\n');
      if(c){
         *c = '\0';
      }

      /* we compare with all of the connected clients names */
      for(int i=0; i<nbClients; ++i)
      {
         /* if the name of the client corresponds we add his socket id to the list of recievers */
         /* we test for the name of the sender because the sender doesn't send to himself */
         if((strcmp(clients[i].name, line) == 0) && (strcmp(sender.name, line) != 0)) {
            socks[countSocks] = clients[i].sock;
            ++countSocks;
         }
      }
   }
   /* we gave a NULL buffer and 0 size to getline so it allocated memory itself but we need to free this memory ourselves after */
   free(line);
   /* don't forget to close the file now we have read it */
   fclose(fptr);

   /* now we have all the sockets corresponding to the clients in the group, we want to send the message to each one of them */
   char buffer[BUF_SIZE];
   buffer[0] = 0;
   /* message sent is of the form "[groupname] sender_name : message"*/
   for(int i=0; i<countSocks; ++i)
   {
      /* manages the "join" message */
      if(from_server) {
         strncpy(buffer, sender.name, BUF_SIZE - 1);
         strncat(buffer, " joined ", sizeof buffer - strlen(buffer) - 1);
         strncat(buffer, groupname, sizeof buffer - strlen(buffer) - 1);
         strncat(buffer, " !", sizeof buffer - strlen(buffer) - 1);
      } else { /* or if it is just a normal message */
         strncpy(buffer, "[", BUF_SIZE - 1);
         strncat(buffer, groupname, sizeof buffer - strlen(buffer) - 1);
         strncat(buffer, "] ", sizeof buffer - strlen(buffer) - 1);
         strncat(buffer, sender.name, sizeof buffer - strlen(buffer) - 1);
         strncat(buffer, " : ", sizeof buffer - strlen(buffer) - 1);
         strncat(buffer, message, sizeof buffer - strlen(buffer) - 1);
      }
      write_client(socks[i], buffer);
   }

   /* lastly we need to add the message to the history of the group */
   add_to_group_history(buffer, groupname);
} 

static void add_to_group_history(const char* message, const char* groupname)
{
   /* we first open the file in append mode => build the name */
   char filename[BUF_SIZE];
   filename[0] = 0;
   strncpy(filename, groupname, BUF_SIZE - 1);
   strncat(filename, "_histo", sizeof filename - strlen(filename) - 1);
   FILE* fptr;
   /* then we open it in append mode to add the message */
   if((fptr = fopen(filename, "a")) == NULL) { 
      perror("Error : Error opening the history file\n");
      exit(EXIT_FAILURE);
   }
   /* we write the message into the file + we add the timestamp to it => format of message in history : (timestamp) message\n */
   time_t timestamp = time(NULL);
   char* time = ctime(&timestamp);
   fputc('(', fptr);
   /* the string returned by ctime is terminated by a \n */
   char* c = strchr(time, '\n');
   if(c){
      *c = '\0';
   }
   fputs(time, fptr);
   fputs(") ", fptr);
   fputs(message, fptr);
   fputc('\n', fptr);
   /* don't forget to close the file */
   fclose(fptr);
}

static void send_message_to_all_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server)
{
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;
   for(i = 0; i < actual; i++)
   {
      /* we don't send message to the sender */
      if(sender.sock != clients[i].sock)
      {
         if(from_server == 0)
         {
            strncpy(message, sender.name, BUF_SIZE - 1);
            strncat(message, " (to everyone) : ", sizeof message - strlen(message) - 1);
         }
         strncat(message, buffer, sizeof message - strlen(message) - 1);
         write_client(clients[i].sock, message);
      }
   }
}

static void send_message_to_one_client(Client destinataire, Client sender, int actual, const char *buffer, char from_server)
{
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;
   if(sender.sock != destinataire.sock)
   {
      strncpy(message, sender.name, BUF_SIZE - 1);
      strncat(message, " (to you) : ", sizeof message - strlen(message) - 1);
      strncat(message, buffer, sizeof message - strlen(message) - 1);
      write_client(destinataire.sock, message);
   }
}

static int init_connection(void)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = { 0 };

   if(sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if(bind(sock,(SOCKADDR *) &sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if(listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static int read_client(SOCKET sock, char *buffer)
{
   /* we want to manipulate strings (with strcmp, strcat, ...) => null byte terminated */
   /* but we recv into a buffer that is too big and recv doesn't add null byte at the end => our string is unusable² */
   int n = 0;
   int error = 0;

   if((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disonnect the client */
      n = 0;
      error = 1;
   }

   /* ²which is why we need to manually add the null byte at the end : n is the return value of recv = number of bytes read */
   buffer[n] = 0;

   /* 
   cf call to read client in app where error was spotted : error control is made using the return of this function => if return = -1 then it's an error
   it was done thinking that if recv gets an error then n will be -1
   BUT => we modify the return of the recv function to be able to do buffer[n] even if recv gets and error (=> we can't write buffer[-1])
   therefore error handling won't happen in any case => that's why we need to add a boolean "error" that will make the function return -1 when recv gets an error
   */
   if(error) {
      return -1;
   }

   return n;
}

static void write_client(SOCKET sock, const char *buffer)
{
   if(send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
      exit(errno);
   }
}

//get client by name
static Client get_client_by_name(Client *clients, const char *name, int actual)
{
   int i = 0;
   for(i = 0; i < actual; i++)
   {
      if(strcmp(clients[i].name, name) == 0)
      {
         return clients[i];
      }
   }
   Client client = { -1 };
   return client;
}

int main(int argc, char **argv)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
