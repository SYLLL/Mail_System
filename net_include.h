#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h> 
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include <vector.h>
#define PORT	     10330
#define MAX_MESS_LEN 4000
//#define WINDOW_SIZE  64
#define MAX_CLIENT_NAME_LENGTH 80 
typedef struct
{
    int type;  //equals 9
    int array[6];
} Update_k;

typedef struct
{
    int stamp;  //also used in read function: server sends mail back: -1 means null; other means there is a mail
    int server; //these two can help distinguish emails
    int read;  //0 for unread and 1 for read, 2 for end of mail list
    char sender[80];
    char receiver[80];
    char subject[80];
    char msg[1000];
} Mail;

//update: 1 means normal update, 2 means update_add, 3 means read, 4 means delete, 5 means list, 6 means print 
typedef struct
{
    int  type;      //1 means normal update, 2 means update_add, 3 means read, 4 means delete
    char client[80];
    int  operation;  //2 add, 3 read, 4 delete
    int  server_index; //where the operation is from,mail创始人
    int  server_stamp; //for safety
    int	 server;  //开头人
    int  index;    //the position of the update in the list, scan through the lsit and find the one that has specific index, instead of using for loop to traverse
    int  array[6];     //for safety, during partition
    Mail mail;
} Update;

typedef struct dummy_Update_node
{
    Update *update;
    struct dummy_Update_node *next;
} Update_node;      

typedef struct
{
    Update_node *head;
    Update_node *tail;
    int          count;
} Update_list;

typedef struct dummy_Mail_node
{
    Mail *mail;
    struct dummy_Mail_node *next;
} Mail_node;

typedef struct dummy_Mail_list
{
    char       client_name[MAX_CLIENT_NAME_LENGTH];
    Mail_node  dummyhead;
    Mail_node  *tail;
    int         count;
    struct dummy_Mail_list  *next;   
} Mail_list;

typedef struct
{
    Mail_list  *head;
    Mail_list  *tail;
    int         count;
} Mail_lists;

typedef struct
{   
    //only from client
    int  type;  //must be 2 for add 
    char client[80];
    int  server_index;
    Mail mail;
} Update_add;

typedef struct
{
    int  type; //must be 3 or 4 for read or delete
    char client[80];
    int  server_index; //the server receiving
    int  server_stamp; //unique identifier for mail
} Update_r_d;

typedef struct
{
    int  type; //must be 5 for list, 6 for print membership
    char client[80];
    int  server_index;
} Update_l_p;
