#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"

typedef struct
{
   SOCKET sock;
   char name[BUF_SIZE];
}Client;

typedef struct 
{
   char name[BUF_SIZE];
   int actual; /* Number of clients in the group */
   Client members[MAX_CLIENTS];
}Group;

#endif /* guard */
