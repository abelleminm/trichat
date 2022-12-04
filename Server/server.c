#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

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

   int nbrGroup = 0;
   Group* groups[MAX_GROUPS];

   /* 
   we want to instanciate all the existing clients and all the groups
   we will have all clients instanciated in the array event though they are not connected 
   => just add the socket number when connect and NULL it when disconnect
   TODO : modify connection process
   */

   /* we first create all our clients */
   FILE* fptr;
   /* we open the file in read mode => if it doens't exist then no need for instanciation */
   if((fptr = fopen("Data/clients", "r")) == NULL) { 
      perror("Error : Error opening file \'clients\'\n");
   } else { // if the file has opened correctly
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

         /* we create the client associated with the name we just got */
         Client cli = { };
         strncpy(cli.name, line, BUF_SIZE - 1);
         clients[actual] = cli;
         actual++;
      }
      /* we gave a NULL buffer and 0 size to getline so it allocated memory itself but we need to free this memory ourselves after */
      free(line);
      /* don't forget to close the file */
      fclose(fptr);

      printf("Clients saved in clients file are instanciated\n");
   }

   /* we then instanciate all the groups */
   /* we open the groups file and then for each line (each group) we open the associated file */
   /* we open the file in read mode => if it doens't exist then no need for instanciation */
   if((fptr = fopen("Data/groups", "r")) == NULL) { 
      perror("Error : Error opening file \'groups\'\n");
   } else
   {
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

         /* we create the group then we will add all the members */
         Group* g = (Group*) malloc(sizeof(Group));
         strncpy(g->name, line, BUF_SIZE - 1);
         g->actual = 0;

         /* we open the associated group file */
         FILE* file;
         char filename[BUF_SIZE];
         filename[0] = 0;
         strncpy(filename, "Data/", BUF_SIZE - 1);
         strncat(filename, line, sizeof filename - strlen(filename) - 1);
         strncat(filename, "/", sizeof filename - strlen(filename) - 1);
         strncat(filename, line, sizeof filename - strlen(filename) - 1);
         if((file = fopen(filename, "r")) == NULL) { 
            perror("Error : Error opening file\n");
            continue;
         }
         /* go to the start of the file just to be sure */
         fseek(file, 0, SEEK_SET);

         char* fline = NULL;
         size_t length = 0;
         ssize_t read;
         /* we read each line */
         while((read = getline(&fline, &length, file)) != -1) {
            /* we get rid of the \n because getline keeps it */
            char* car = strchr(fline, '\n');
            if(car){
               *car = '\0';
            }

            /* we seach in the clients list and we put the client in the members list */
            for(int i=0; i<actual; ++i)
            {
               /* when we find the name in the array then we add the client's pointer to our members array */
               if(!strcmp(clients[i].name, fline))
               {
                  g->members[g->actual++] = &clients[i];
               }
            }
         }
         /* we gave a NULL buffer and 0 size to getline so it allocated memory itself but we need to free this memory ourselves after */
         free(fline);
         /* don't forget to close the file */
         fclose(file);

         /* we then add the group created to the groups array and increment the number of groups */
         groups[nbrGroup++] = g;
      }
      /* we gave a NULL buffer and 0 size to getline so it allocated memory itself but we need to free this memory ourselves after */
      free(line);
      /* don't forget to close the file */
      fclose(fptr);
   }

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

         /* 
         we can't have twice the same person logged in => check if there is already a client with this name
         because of the init process we will always have a client with the same name if the person has already connected once
         we need to check the socket => if it is not NULL then the client is connected, else the client exists but isn't connected
         */
         int exists = 0;
         int index = 0;
         int connected = 0;
         for(int i=0; i<actual; ++i) {
            if(!strcmp(clients[i].name,buffer)) { // if the name of a client is equal to the one recieved (strcmp returns 0) then we want to disconnect the client
               exists = 1;
               index = i;
               if(clients[i].sock != NULL)
               {
                  connected = 1;
               }
            } 
         }
         if(connected) { // if the name was found we just continue and send a message to the client saying he can't connect with this name
            write_client(csock, "You can't connect with this username because it is already in use");
            closesocket(csock);
            continue;
         } 
         /* what is the new maximum fd ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         if(exists) // if the client already existed then we just have to assign him his socket
         {
            clients[index].sock = csock;
            printf("Known client %s connected\n", clients[index].name);           
         } else // if the client didn't already exist then we have to create the client
         {
            Client c = { csock };
            strncpy(c.name, buffer, BUF_SIZE - 1);
            clients[actual] = c;
            actual++;
            printf("New client %s connected\n",c.name);
         }

         /* we write the name of the client in the clients file if it is not present (new connection) */
         /* first we read the file and search if the name provided already exists */
         FILE* fptr;
         /* we open the file in read and append => if it doesn't exist it is created */
         if((fptr = fopen("Data/clients", "a+")) == NULL) { 
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
         }else{
            write_client(csock, "Welcome back to Trichat ! You can now chat with your friends !");
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
                  printf("Client %s sent a null byte, he was disconnected\n", client.name);
               }
               else
               {
                  // if the client sends "@name" then send the message to the client named "name"
                  if(buffer[0] == '@')
                  {
                     char *name = buffer + 1;
                     char *message = strchr(buffer, ' ');
                     if(name == NULL)
                     {
                        write_client(client.sock, "usage : @name [message]");
                        continue;
                     }

                     /* error handling : if the user types "@name " we need to print and error message */
                     if(message == NULL) // if the ' ' wasn't found strchr will return a NULL pointer
                     {
                        write_client(client.sock, "usage : @name [message]");
                        continue;
                     }

                     // we split the buffer in two by putting a null byte to terminate the command buffer in place of the ' ' at the begining of the group name
                     *message = 0;
                     message++;

                     /* error handling : if the user types "@name " we need to print and error message */
                     if(strlen(message) == 0) // if after splitting the buffer, there is nothing
                     {
                        write_client(client.sock, "usage : @name [message]");
                        continue;
                     }
                     
                     Client destinataire = get_client_by_name(clients, name, actual);
                     
                     if(destinataire.sock == -1) // get_client_by_name returns a client with sock = -1 if the client doesn't exist
                     {
                        write_client(client.sock, "This user doesn\'t exist");
                        printf("Client %s tried to send a message to a non existing client\n", client.name);
                     }
                     else
                     {
                        if(destinataire.sock == NULL) // because of the init of the data, a client can exist but not be connected => not connected means sock = NULL
                        {
                           write_client(client.sock, "This user isn\'t connected, your message will be added to their mailbox.");
                           add_to_mailbox(message,destinataire,client);
                           printf("Client %s sent a message to %s but he wasn\'t connected, the message was added to his mailbox\n", client.name, destinataire.name);
                           continue;
                        }
                        /* if the client exists and is connected we send him the message */
                        send_message_to_one_client(destinataire, client, actual, message, 0);
                        printf("Client %s sent a message to %s\n", client.name, destinataire.name);
                     }
                  }
                  else if(buffer[0] == '!') // the user sends "!command" to execute command "command" => for groups
                  {
                     char *command = buffer + 1;
                     char *group = strchr(buffer, ' ');
                     if(command == NULL)
                     {
                        write_client(client.sock, "usage : !command [group]");
                        continue;
                     }

                     /* if the user calls the quit command we just disconnect him */
                     if(!strcmp(command, "quit"))
                     {
                        closesocket(clients[i].sock);
                        remove_client(clients, i, &actual);
                        printf("Client %s disconnected successfully\n", clients[i].name);
                        continue;
                     }
                     else if(!strcmp(command, "mailbox"))
                     {
                        FILE* fptr;
                        /* we open the file in read only : if the file doesn't exist then the mailbox doesn't exist */
                        char filename[BUF_SIZE];
                        filename[0] = 0;
                        strncpy(filename, "Data/", BUF_SIZE - 1);
                        strncat(filename, client.name, BUF_SIZE - 1);
                        strncat(filename, "_mailbox", sizeof filename - strlen(filename) - 1);
                        if((fptr = fopen(filename, "r")) == NULL) { 
                           perror("Error : mailbox doesn\'t exist\n");
                           write_client(client.sock, "Your mailbox is empty");
                           continue;
                           /* no need to close the file because if we get to this point it means it wasn't opened */
                        }
                        /* go to the start of the file just to be sure */
                        fseek(fptr, 0, SEEK_SET);

                        char* line = NULL;
                        size_t len = 0;
                        ssize_t nread;
                        int countLine = 0;
                        /* we count the number of lines */
                        while((nread = getline(&line, &len, fptr)) != -1) {
                           ++countLine;
                        }
                        /* we gave a NULL buffer and 0 size to getline so it allocated memory itself but we need to free this memory ourselves after */
                        free(line);
                        
                        // if the file is empty
                        if(countLine==0){
                           write_client(client.sock, "Your mailbox is empty");
                           continue;
                        }

                        /* we then go back to the begining of the file */
                        fseek(fptr, 0, SEEK_SET);
                        /* and we read and send the messages in the mailbox */
                        write_client(client.sock, "========== Mailbox ===========");
                        line = NULL;
                        len = 0;
                        /* we read all the lines */
                        while((nread = getline(&line, &len, fptr)) != -1) {
                           write_client(client.sock, line);
                        }
                        free(line);

                        write_client(client.sock, "==============================");

                        /* don't forget to close the file */
                        fclose(fptr);
                        /* Then we want the mailbox to be cleared */
                        fclose(fopen(filename, "w"));
                        printf("Client %s read his mailbox, it was then cleaned\n", client.name);
                        continue;
                     }
                     else if(!strcmp(command, "mygroups")) // command to list all the groups the user belongs to
                     {
                        int nogroups = 1;
                        write_client(client.sock, "========= Your groups =========\n");
                        for(int i = 0; i<nbrGroup; ++i)
                        {
                           for(int j = 0; j<groups[i]->actual; ++j)
                           {
                              if(!strcmp(client.name, groups[i]->members[j]->name)) // if we find the user's name in the members list
                              {
                                 nogroups = 0;
                                 write_client(client.sock, groups[i]->name);
                                 break;
                              }
                           }
                        }
                        if(nogroups) 
                        {
                           write_client(client.sock, "You do not belong to any group, try joining some !");
                        }
                        write_client(client.sock, "===============================");
                        continue;
                     }
                     else if(!strcmp(command, "groups")) // command to list all the existing groups
                     {
                        write_client(client.sock, "========= All groups =========\n");
                        for(int i = 0; i<nbrGroup; ++i)
                        {
                           write_client(client.sock, groups[i]->name);
                        }
                        write_client(client.sock, "==============================");
                        continue;
                     }

                     /* if we are in the case where the command is not quit => then we need to have a group name after the command */
                     /* error handling : if the user types "!command" we need to print and error message */
                     if(group == NULL) // if the ' ' wasn't found strchr will return a NULL pointer
                     {
                        write_client(client.sock, "usage : !command [group]");
                        continue;
                     }

                     // we split the buffer in two by putting a null byte to terminate the command buffer in place of the ' ' at the begining of the group name
                     *group = 0; 
                     group++;

                     /* error handling : if the user types "!command " we need to print and error message */
                     if(strlen(group) == 0) // if after splitting the buffer, there is nothing
                     {
                        write_client(client.sock, "usage : !command [group]");
                        continue;
                     }

                     if(!strcmp(command, "create")) // user wants to create a group chat
                     {
                        if(group == NULL)
                        {
                           write_client(client.sock, "Usage : !create [group_name]");
                           continue;
                        }
                        /* first we need to check if the conversation doesn't already exist */
                        FILE* fptr;
                        /* first we build the filename */
                        char filename[BUF_SIZE];
                        filename[0] = 0;
                        strncpy(filename, "Data/", BUF_SIZE - 1);
                        strncat(filename, group, sizeof filename - strlen(filename) - 1);
                        strncat(filename, "/", sizeof filename - strlen(filename) - 1);
                        strncat(filename, group, sizeof filename - strlen(filename) - 1);
                        /* we open the file in write only => if it doesn't exist, it will return NULL */
                        if((fptr = fopen(filename, "r")) != NULL) { 
                           perror("Error : File doesn\'t exist\n");
                           write_client(client.sock, "This conversation already exist");
                           printf("Client %s tried to create a conversation that already exists: %s\n", client.name, group);
                           /* don't forget that the file was opened if we get in the if so we need to close it */
                           fclose(fptr);
                           continue;
                        }
                        /* we don't need to close the file because if we got past the if statement it means it doesn't exists and wasn't opened */

                        /* we will add the name of the group to the file with all the group names (we know the group doesn't exist) => create the file if it doesn't exist */
                        if((fptr = fopen("Data/groups", "a+")) == NULL) { 
                           perror("Error : Error opening file \'groups\'\n");
                           continue;
                        }
                        fputs(group, fptr);
                        fputc('\n', fptr);
                        /*don't forget to close the file */
                        fclose(fptr);
                        
                        /* when the group is created, we have to create the file to store the message history => name = groupname_histo */
                        /* but first we create the directory to put all the files */
                        /* first we build the command for making the directory */
                        char mkdir[BUF_SIZE];
                        mkdir[0] = 0;
                        strncpy(mkdir, "mkdir Data/", BUF_SIZE - 1);
                        strncat(mkdir, group, sizeof filename - strlen(filename) - 1);
                        /* we create the directory */
                        system(mkdir);
                        /* we modify the filename */
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

                        Group* g = (Group*) malloc(sizeof(Group));
                        strncpy(g->name,group,BUF_SIZE-1);
                        groups[nbrGroup++] = g;

                        /* 
                        WARNING : we can't add "client" to the members list because it's a list of pointers
                        => if we add client then we will point to value of the client doing the action and we will have a changing client in our list
                        we need to find the client in the list of all clients (that is static) and add THIS client's pointer to our list of members
                        */
                        int index;
                        for(int i = 0; i<actual; ++i)
                        {
                           if(!strcmp(client.name, clients[i].name))
                           {
                              index = i;
                              break;
                           }
                        }
                        
                        /* we add the creator of the group to the group */
                        add_client_group(&clients[index], groups, nbrGroup, group);

                        write_client(client.sock, "Group created");
                        printf("Client %s created the group %s\n", client.name, group);
                     }
                     else if(!strcmp(command, "join")) // user wants to join a group chat
                     {
                        if(group == NULL)
                        {
                           write_client(client.sock, "Usage : !join [group_name]");
                           continue;
                        }

                        int index;
                        for(int i = 0; i<actual; ++i)
                        {
                           if(!strcmp(client.name, clients[i].name))
                           {
                              index = i;
                              break;
                           }
                        }

                        /* we add the client to the group */
                        int err = add_client_group(&clients[index], groups, nbrGroup, group);

                        if(!err) // we send the messages only if the group was successfully joined
                        {
                           int gpIndex;
                           for(int i=0; i<nbrGroup; ++i)
                           {
                              if(!strcmp(groups[i]->name, group))
                              {
                                 gpIndex = i;
                                 break;
                              }
                           }
                           /* send a message to all clients in the group to let them know someone joined */
                           send_message_to_group(groups[gpIndex],client,NULL,1);

                           write_client(client.sock, "Group joined");
                           printf("User %s joined the group %s\n", client.name, group);
                        }
                     }
                     else if(!strcmp(command, "leave")) // user wants to leave a group chat
                     {
                        int index;
                        for(int i = 0; i<actual; ++i)
                        {
                           if(!strcmp(client.name, clients[i].name))
                           {
                              index = i;
                              break;
                           }
                        }

                        /* we add the client to the group */
                        int err = leave_group(&clients[index], groups, nbrGroup, group);

                        if(!err) // we send the messages only if the group was successfully left
                        {
                           int gpIndex;
                           for(int i=0; i<nbrGroup; ++i)
                           {
                              if(!strcmp(groups[i]->name, group))
                              {
                                 gpIndex = i;
                                 break;
                              }
                           }
                           /* send a message to all clients in the group to let them know someone left */
                           send_message_to_group(groups[gpIndex],client,NULL,2);

                           write_client(client.sock, "Group left");
                           printf("Client %s left the group %s\n", client.name, group);
                        }
                     }
                     else if(!strcmp(command, "histo"))
                     {
                        /* the user want's to display the message history of the group => by default we give the 10 last messages */
                        /*
                        first we check that the user belong to the group and that the group exists
                        we want to open the file corresponding to the group chat's history
                        then we count the number of lines (messages) and subtract 10 to get the number of lines to skip
                        then we read the file, skip the x first lines and just print the rest
                        */

                        if(group == NULL)
                        {
                           write_client(client.sock, "Usage : !histo [group_name]");
                           continue;
                        }

                        /* does the user belong to he group ? and does the group exist ? */
                        int found = 0;
                        int exists = 0;
                        for(int i = 0; i<nbrGroup; ++i)
                        {
                           if(!strcmp(groups[i]->name, group)) // we found the group
                           {
                              for(int j = 0; j<groups[i]->actual; ++j)
                              {
                                 if(!strcmp(groups[i]->members[j]->name, client.name)) // if the user's name is in the list of members
                                 {
                                    found = 1;
                                    break;
                                 }
                              }
                              exists = 1;
                              break;
                           }
                        }

                        /* error handling : group doesn't exist or user not part of it */
                        if(!exists)
                        {
                           write_client(client.sock, "This group doesn\'t exist");
                           printf("Client %s tried to access the history of a non existing group\n", client.name);
                           continue;
                        }
                        if(!found)
                        {
                           write_client(client.sock, "You can't view the history unless you are part of the group");
                           printf("Client %s tried to access the history of a group he's not part of\n", client.name);
                           continue;
                        }

                        FILE* fptr;
                        /* we open the file in read only : if the file doesn't exist then the group doesn't exist */
                        char filename[BUF_SIZE];
                        filename[0] = 0;
                        strncpy(filename, "Data/", BUF_SIZE - 1);
                        strncat(filename, group, sizeof filename - strlen(filename) - 1);
                        strncat(filename, "/", sizeof filename - strlen(filename) - 1);
                        strncat(filename, group, sizeof filename - strlen(filename) - 1);
                        strncat(filename, "_histo", sizeof filename - strlen(filename) - 1);
                        if((fptr = fopen(filename, "r")) == NULL) { 
                           perror("Error : Group doesn\'t exist\n");
                           write_client(client.sock, "This group doesn\'t exist");
                           continue;
                           /* no need to close the file because if we get to this point it means it wasn't opened */
                        }
                        /* go to the start of the file just to be sure */
                        fseek(fptr, 0, SEEK_SET);

                        char* line = NULL;
                        size_t len = 0;
                        ssize_t nread;
                        int countLine = 0;
                        /* we count the number of lines */
                        while((nread = getline(&line, &len, fptr)) != -1) {
                           ++countLine;
                        }
                        /* we gave a NULL buffer and 0 size to getline so it allocated memory itself but we need to free this memory ourselves after */
                        free(line);
                        
                        /* we subtract 10 from the total, if there are less than 10 messages we will get a negative number => just change it to zero */
                        countLine -= 10;
                        if(countLine < 0)
                        {
                           countLine = 0;
                        }

                        /* we then go back to the begining of the file */
                        fseek(fptr, 0, SEEK_SET);
                        /* and we read and send the 10 last lines */
                        write_client(client.sock, "======= Message History ======");
                        line = NULL;
                        len = 0;
                        int counter = 0;
                        /* we read all the lines */
                        while((nread = getline(&line, &len, fptr)) != -1) {
                           if(counter >= countLine) // we got to the last 10 lines
                           {
                              /* we send the line to the client */
                              write_client(client.sock, line);
                           }
                           ++counter;
                        }
                        free(line);

                        write_client(client.sock, "==============================");

                        /* don't forget to close the file */
                        fclose(fptr);
                        printf("Client %s viewed the history of the group %s\n", client.name, group);
                     }
                     else
                     {
                        write_client(client.sock, "Error : unknown command");
                        printf("Client %s tried to use an unknown command\n", client.name);
                     }
                  }
                  else if(buffer[0] == '#') // "#group message" will send the message "message" to group named "group"
                  {
                     char *group = buffer + 1;
                     char *message = strchr(buffer, ' ');
                     if(group == NULL)
                     {
                        write_client(client.sock, "usage : #group [message]");
                        continue;
                     }

                     /* error handling : if the user types "#group" we need to print and error message */
                     if(message == NULL) // if the ' ' wasn't found strchr will return a NULL pointer
                     {
                        write_client(client.sock, "usage : #group [message]");
                        continue;
                     }

                     // we split the buffer in two by putting a null byte to terminate the command buffer in place of the ' ' at the begining of the group name
                     *message = 0;
                     message++;

                     /* error handling : if the user types "#group " we need to print and error message */
                     if(strlen(message) == 0) // if after splitting the buffer, there is nothing
                     {
                        write_client(client.sock, "usage : #group [message]");
                        continue;
                     }

                     int exists = 0;
                     int isMember = 0;
                     int gpIndex;
                     /* Add client to the group in struct */
                     for(int i = 0; i < nbrGroup; ++i)
                     {
                        if(!strcmp(groups[i]->name, group))
                        {
                           for(int j = 0 ; j < groups[i]->actual ; ++j)
                           {
                              if(!strcmp(client.name, groups[i]->members[j]->name))
                              {
                                 isMember = 1;
                                 break;
                              }
                           }
                           gpIndex = i;
                           exists = 1;
                           break;
                        }
                     }
                     if(!exists)
                     {
                        write_client(client.sock, "This group doesn't exist");
                        printf("Client %s tried to send a message to a non existing group\n", client.name);
                        continue;
                     }
                     if(!isMember)
                     {
                        write_client(client.sock, "You do not belong to this group, please join it first");
                        printf("Client %s tried to send a message to a group he's not part of\n", client.name);
                        continue;
                     }

                     /* if the client belongs to the group, we want to send the message to the whole group */
                     send_message_to_group(groups[gpIndex], client, message, 0);
                     printf("Client %s sent a message to the group %s\n", client.name, group);
                  }
                  else
                  {
                     send_message_to_all_clients(clients, client, actual, buffer, 0);
                     printf("Client %s sent a message to all clients\n", client.name);
                  }
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
   clients[to_remove].sock = NULL;
}

static void send_message_to_group(Group* group, Client sender, const char* message, int from_server)
{
   /* we build the message */
   char buffer[BUF_SIZE];
   buffer[0] = 0;
   /* manages the "join" message */
   if(from_server == 1) {
      strncpy(buffer, sender.name, BUF_SIZE - 1);
      strncat(buffer, " joined ", sizeof buffer - strlen(buffer) - 1);
      strncat(buffer, group->name, sizeof buffer - strlen(buffer) - 1);
      strncat(buffer, " !", sizeof buffer - strlen(buffer) - 1);
   }else if(from_server == 2)
   {
      strncpy(buffer, sender.name, BUF_SIZE - 1);
      strncat(buffer, " left ", sizeof buffer - strlen(buffer) - 1);
      strncat(buffer, group->name, sizeof buffer - strlen(buffer) - 1);
      strncat(buffer, " !", sizeof buffer - strlen(buffer) - 1);
   } else { /* or if it is just a normal message */
      strncpy(buffer, "[", BUF_SIZE - 1);
      strncat(buffer, group->name, sizeof buffer - strlen(buffer) - 1);
      strncat(buffer, "] ", sizeof buffer - strlen(buffer) - 1);
      strncat(buffer, sender.name, sizeof buffer - strlen(buffer) - 1);
      strncat(buffer, " : ", sizeof buffer - strlen(buffer) - 1);
      strncat(buffer, message, sizeof buffer - strlen(buffer) - 1);
   }
   /* message sent is of the form "[groupname] sender_name : message"*/
   
   for(int i=0; i<group->actual; ++i)
   {
      /* we send the message to everyone in the group except the sender */
      if(strcmp(group->members[i]->name, sender.name))
      {
         write_client(group->members[i]->sock, buffer);
      }
   }

   /* lastly we need to add the message to the history of the group */
   add_to_group_history(buffer, group);
} 

static void add_to_group_history(const char* message, Group* group)
{
   /* we first open the file in append mode => build the name */
   char filename[BUF_SIZE];
   filename[0] = 0;
   strncpy(filename, "Data/", BUF_SIZE - 1);
   strncat(filename, group->name, sizeof filename - strlen(filename) - 1);
   strncat(filename, "/", sizeof filename - strlen(filename) - 1);
   strncat(filename, group->name, sizeof filename - strlen(filename) - 1);
   strncat(filename, "_histo", sizeof filename - strlen(filename) - 1);
   FILE* fptr;
   /* then we open it in append mode to add the message */
   if((fptr = fopen(filename, "a")) == NULL) { 
      perror("Error : Error opening the history file\n");
      return;
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

static void add_to_mailbox(const char* message, Client dest, Client sender)
{
   /* we first open the file in append mode => build the name */
   char filename[BUF_SIZE];
   filename[0] = 0;
   strncpy(filename, "Data/", BUF_SIZE - 1);
   strncat(filename, dest.name, BUF_SIZE - 1);
   strncat(filename, "_mailbox", sizeof filename - strlen(filename) - 1);
   FILE* fptr;
   /* then we open it in append mode to add the message */
   if((fptr = fopen(filename, "a")) == NULL) { 
      perror("Error : Error opening the mailbox file\n");
      return;
   }
   /* we write the message into the file + we add the timestamp to it => format of message in mailbox : (timestamp) message\n */
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
   fputs(sender.name, fptr);
   fputs(" : ", fptr);
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
      if((sender.sock != clients[i].sock) && (clients[i].sock != NULL))
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
   if(sock != NULL)
   {
      if(send(sock, buffer, strlen(buffer), 0) < 0)
      {
         perror("send()");
         exit(errno);
      }
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

//add client in a group
/* function returns 0 for success, 1 if the person couldn't join the group for any reason */
static int add_client_group(Client* client, Group** groups, int nbrGroup, const char *group)
{
   int error = 1;
   /* Add client to the group in struct */
   for(int i = 0 ; i < nbrGroup ; ++i)
   {
      if(!strcmp(groups[i]->name, group))
      {
         if(groups[i]->actual == MAX_CLIENTS)
         {
            write_client(client->sock, "Can't join the group : group is full");
            return 1;
         }
         for(int j = 0 ; j < groups[i]->actual ; ++j)
         {
            if(!strcmp(client->name, groups[i]->members[j]->name))
            {
               write_client(client->sock, "You're already in the group!");
               return 1;
            }
         }
         /* add the client and increment the size */
         groups[i]->members[groups[i]->actual++] = client;
         error = 0;
         break;
      }
   }
   if(error)
   {
      write_client(client->sock, "This group doesn't exist");
      return 1;
   }

   FILE* fptr;
   /* want to add the name of the person who joins to the file */
   char filename[BUF_SIZE];
   filename[0] = 0;
   strncpy(filename, "Data/", BUF_SIZE - 1);
   strncat(filename, group, sizeof filename - strlen(filename) - 1);
   strncat(filename, "/", sizeof filename - strlen(filename) - 1);
   strncat(filename, group, sizeof filename - strlen(filename) - 1);
   /* we open the file in read append => we know it exists but it will not work if it doesn't (just in case) */
   if((fptr = fopen(filename, "a+")) == NULL) { 
      perror("Error : File doesn\'t exist\n");
      write_client(client->sock, "Error : File doesn\'t exist");
      return 1;
   }
   /* go to the start of the file just to be sure */
   fseek(fptr, 0, SEEK_SET);

   /* add the name to the file */
   fputs(client->name, fptr);
   fputc('\n', fptr);

   /* don't forget to close the file */
   fclose(fptr);

   return 0;
}

//leave a group
/* function returns 0 for success, 1 if the person couldn't leave the group for any reason */
static int leave_group(Client* client, Group** groups, int nbrGroup, const char *group)
{
   int error = 1;
   /* Search the group in struct */
   for(int i = 0 ; i < nbrGroup ; ++i)
   {
      if(!strcmp(groups[i]->name, group))
      {
         /* Search for the client */
         for(int j = 0 ; j < groups[i]->actual ; ++j)
         {
            if(!strcmp(client->name, groups[i]->members[j]->name))
            {
               /* case when it is not the end of the list */
               if(j != groups[i]->actual-1)
               {
                  groups[i]->members[j] = groups[i]->members[groups[i]->actual-1];
               }
               groups[i]->members[groups[i]->actual-1] = NULL;
               groups[i]->actual--;
               error = 0;
               break;               
            }
         }
         if(error)
         {
            write_client(client->sock, "You are not in this group!");
            return 1;
         }
      }
   }
   if(error)
   {
      write_client(client->sock, "This group doesn't exist");
      return 1;
   }

   FILE* fptr;
   FILE* ftmp;
   /* want to delete the name of the person who left */
   char filename[BUF_SIZE];
   filename[0] = 0;
   strncpy(filename, "Data/", BUF_SIZE - 1);
   strncat(filename, group, sizeof filename - strlen(filename) - 1);
   strncat(filename, "/", sizeof filename - strlen(filename) - 1);
   strncat(filename, group, sizeof filename - strlen(filename) - 1);
   /* we open the file in read append => we know it exists but it will not work if it doesn't (just in case) */
   if((fptr = fopen(filename, "a+")) == NULL) { 
      perror("Error : File doesn\'t exist\n");
      write_client(client->sock, "Error : File doesn\'t exist");
      return 1;
   }

   char tmp[BUF_SIZE];
   tmp[0] = 0;
   strncpy(tmp, "Data/", BUF_SIZE - 1);
   strncat(tmp, group, sizeof filename - strlen(filename) - 1);
   strncat(tmp, "/tmp", sizeof filename - strlen(filename) - 1);
   ftmp = fopen(tmp, "w");

   /* go to the start of the file just to be sure */
   fseek(fptr, 0, SEEK_SET);

   char* line = NULL;
   size_t len = 0;
   ssize_t nread;
   /* we read each line */
   while((nread = getline(&line, &len, fptr)) != -1) 
   {
      /* we get rid of the \n because getline keeps it */
      char* c = strchr(line, '\n');
      if(c){
         *c = '\0';
      }
      if(strcmp(line,client->name))
      {
         fputs(line, ftmp);
         fputs("\n", ftmp);		
      }
   }

   /* don't forget to close the files */
   fclose(fptr);
   fclose(ftmp);
   free(line);

   remove(filename);
   rename(tmp, filename);

   return 0;
}

int main(int argc, char **argv)
{
   /* creating */
   if ( mkdir("Data", 755) != 0 )
   {
      if (errno == EACCES)
      {
         printf("Can't create the data folder, you don't have the rights.\n");
         exit(EXIT_FAILURE);
      }
   }

   init();

   app();

   end();

   return EXIT_SUCCESS;
}
