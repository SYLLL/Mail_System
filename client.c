//Jiayao Wu, Suyi Liu
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sp.h"
#include "net_include.h"
#define	MAX_MEMBERS 10
static char	User[80];
static char	Spread_name[80];
static char	username[80];
static char	server_group_name[80];
static int	server = -1;
static int	temp_server;
static int	delete_mail; //d 10
static int 	read_mail; //r 10
static char     Private_group[MAX_GROUP_NAME];
static mailbox Mbox;
static  void    Print_menu();
static int	To_exit = 0;
static void 	Bye();
static void	User_command();
static void 	Read_message();
char            choice;
static int	currupdatesize;
static Update	*currupdate;
static Mail	*currmail;
static int 	currmailsize;
static int	service_type;
static int	sender[MAX_GROUP_NAME];
static char	target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
static int	num_groups;
static int	mess_type;
static int	endian_mismatch;
//static Mail_list *mail_list;

static Update_l_p *update_l_p;
static Update_r_d *update_r_d;
static Update_add *update_add;
static Update_k   *m_info;

Mail_node* form_mail_node(Mail *m);
void add_Mail_node_to_list(Mail_list* ml, Mail_node* mn);

int main( int argc, char *argv[])
{
    int ret;
    m_info = malloc(sizeof(Update_k));
    //client connect to a private group
    sprintf(User, "heirenwenhao123");    
    sprintf(Spread_name, "10330");
    ret = SP_connect(Spread_name, User, 0 ,1, &Mbox, Private_group);
    if (ret != ACCEPT_SESSION)
    {
	SP_error(ret);
	Bye();
    }
    printf("User: connect to %s with private group %s\n",Spread_name,Private_group);

    E_init();

    E_attach_fd( 0, READ_FD, User_command, 0, NULL, LOW_PRIORITY );

    Print_menu();

    fflush(stdout);

    E_handle_events();

    return (0);
}



static void User_command() {
	int senderM;
        int     ret;
        int     i;
	int num_groups;
	char receiver[80];
	char subject[80];
	char content[1000];
        char command[130];


	for( i=0; i < sizeof(command); i++ ) command[i] = 0;
	if( fgets( command, 130, stdin ) == NULL ) 
            Bye();


	
        switch (command[0]) 
        {
            case 'u':
                    ret = sscanf( &command[2],"%s",username);
                    if (ret < 1)
                    {
                        printf("invalid username.\n");
                        Print_menu();
                        break;
                    } else {
                        printf("Logged in as %s.\n", username);
                        server = -1;
                    }

                    Print_menu();
                    break;
            case 'c':
                    ret = sscanf( &command[2],"%d",&temp_server);
                    if (ret < 1)
                    {
                        printf("invalid server number.\n");
                        Print_menu();
                        break;
                    } 
		    if (username[0] == '\0') {
                        printf("You have to specify username first!\n");
      	                Print_menu();
                        break;   
                    }
		    //handle cases not in 1-5
		    if (temp_server < 1 || temp_server >5) {
			printf("Invalid server index. Please try again.\n");
			Print_menu();
			break;
		    }
		    if (temp_server == server) {
			printf("You have already connected to this server!\n");
			Print_menu();
			break;
		    }
                    if (temp_server != -1)
                    {
                        server = temp_server;
                    }
		    sprintf(server_group_name,"server_%d",server);
		    printf("Connected to server: %s\n",server_group_name);

                    Print_menu();
                    break;
            case 'l':
                    if (server == -1) {
                        printf("You have to connect to a server first!\n");
			Print_menu();
                        break;
                    }
		    update_l_p = malloc(sizeof(Update_l_p));
		    update_l_p->type = 5;
		    update_l_p->server_index = server;
		    strcpy(update_l_p->client, username);		    
                    ret = SP_multicast(Mbox, AGREED_MESS, server_group_name,0,sizeof(Update_l_p),update_l_p);
		    if ( ret < 0) {
			SP_error(ret);
			Bye();
		    }


                    //no need here: initialize a fresh mail list

		    //wait to receive the msg
		    int all_received_flag = 0;
                    int index = 0;
                    printf("\n~~~User name: %s~~~\n", username);
                    printf("~~~Mail server index: %d~~~\n", server);
                    printf("~~~Listing of headers~~~\n");
 
                    currmail = malloc(sizeof(Mail));
                    while (all_received_flag == 0) {
    		        ret = SP_receive(Mbox, &service_type,sender, MAX_MEMBERS,&num_groups,target_groups,&mess_type,&endian_mismatch,sizeof(Mail), currmail);
		        if ( ret < 0) {
			    SP_error(ret);
			    Bye();
		        }
			if (currmail->read == 2) {
			    all_received_flag = 1;
                            printf("~~~End of listing~~~\n");
                            break;
                        }
                        Mail *m = malloc(sizeof(Mail));
                        //copying fields
		        m->stamp = currmail->stamp;
                        m->server = currmail->server;
  			m->read = currmail->read;
			strcpy(m->sender, currmail->sender);
		        strcpy(m->receiver, currmail->receiver);
			strcpy(m->subject, currmail->subject);
			strcpy(m->msg, currmail->msg);
                        
 	                //printing
                        index++;
                        printf("Mail %d: \n", index);
                        printf("      sender: %s", m->sender);
                        printf("  subject: %s \n", m->subject);
			if (m->read == 1) {
			    printf("      R\n \n");
			} else {
			    printf("      N\n \n");
			}
                        free(m);
                    }
		    
                    Print_menu();
                    break;
            case 'm':
                    printf("Mailing a message\n");

                    if (server == -1) {
                        printf("You have to connect to a server first!\n");
                        Print_menu();
		        break;
                    }
		    currmail = malloc(sizeof(Mail));
		    printf("to: ");
		    //fgets (&receiver,sizeof(stdin), stdin);
	            scanf("%[^\n]%*c", &receiver);
		    printf("subject: ");
		    //fgets (&subject,sizeof(stdin), stdin);
		    scanf("%[^\n]%*c",subject);
		    printf("msg: ");
		    //fgets (&content, 1000, stdin);
                    scanf("%[^\n]%*c", content);
		    currmail->read = 0;
		    currmail->server = server;
		    strcpy(currmail->sender, username);
		    strcpy(currmail->receiver,receiver);
		    strcpy(currmail->subject,subject);
		    strcpy(currmail->msg,content);
		    
		    update_add = malloc(sizeof(Update_add));
		    update_add->type = 2;
		    strcpy(update_add->client, receiver);
		    update_add->server_index = server;
		    update_add->mail = *currmail;
		    ret = SP_multicast(Mbox, AGREED_MESS, server_group_name,0,sizeof(Update_add),update_add );
		    if ( ret < 0) {
			SP_error(ret);
			Bye();
		    }
		    free(currmail);
		    
                    Print_menu();
                    break;
            case 'd':
                    if (server == -1) {
                        printf("You have to connect to a server first!\n");
			Print_menu();
                        break;
                    }
		    update_l_p = malloc(sizeof(Update_l_p));
		    update_l_p->type = 5;
		    update_l_p->server_index = server;
		    strcpy(update_l_p->client, username);		    
		    //send the request for listing (operation = 4)
                    ret = SP_multicast(Mbox, AGREED_MESS, server_group_name,0,sizeof(Update_l_p),update_l_p);
		    if ( ret < 0) {
			SP_error(ret);
			Bye();
		    }

                    //temporary mail list 
                    Mail_list *temp_mail_list = malloc(sizeof(Mail_list));
	            temp_mail_list->dummyhead.next = NULL;
                    temp_mail_list->tail = &(temp_mail_list->dummyhead);
		    temp_mail_list->count = 0;
	  

		    //wait to receive the msg
		    int all_received_flag2 = 0;
                    int index2 = 0;

                    printf("~~~User name: %s~~~\n", username);
                    printf("~~~Mail server index: %d~~~\n", server);
                    printf("~~~Listing of headers~~~\n");
 
		    currmail = malloc(sizeof(Mail));
                    while (all_received_flag2 == 0) {
    		        ret = SP_receive(Mbox, &service_type,sender, MAX_MEMBERS,&num_groups,target_groups,&mess_type,&endian_mismatch,sizeof(Mail), currmail);
		        if ( ret < 0) {
			    SP_error(ret);
			    Bye();
		        }
			if (currmail->read == 2) {
			    all_received_flag2 = 1;
                            printf("~~~End of listing~~~\n");
                    	    break;
		        }
                        Mail *m = malloc(sizeof(Mail));
                        //copying fields
		        m->stamp = currmail->stamp;
                        m->server = currmail->server;
  			m->read = currmail->read;
			strcpy(m->sender, currmail->sender);
		        strcpy(m->receiver, currmail->receiver);
			strcpy(m->subject, currmail->subject);
			strcpy(m->msg, currmail->msg);
                        //adding to local list is not needed here
                        Mail_node* mn = form_mail_node(m);
                        add_Mail_node_to_list(temp_mail_list, mn);
                        //printing
                        index2++;
                        printf("Mail %d: \n", index2);
                        printf("      sender: %s", m->sender);
                        printf("  subject: %s \n", m->subject);
                        if (m->read == 1) {
			    printf("      R\n \n");
			} else {
			    printf("      N\n \n");
			}

		    }
		    printf("Please select which message you want to delete: ");
		    char delete_mail_s[80];
		    scanf("%s", delete_mail_s);
		    delete_mail = atoi(delete_mail_s);

		    if (delete_mail < 1) {
			printf("Index should be greater than or equal to 1\n");

                        Print_menu();
			break;
		    }

		    Mail_node *temp = &(temp_mail_list->dummyhead);
		    int breakflag2 = 0;
		    for (int i = 0; i < delete_mail;i++) {
			if (temp->next != NULL) {
			    temp = temp->next;
			} else {
			   printf("invalid number. \n");
			   breakflag2 = 1;
			   break;
			}
		    } 
		    
		    if (breakflag2 == 1) {		      
                        Print_menu();
			break;
		    }
		    update_r_d = malloc(sizeof(Update_r_d));
		    update_r_d->type = 4;
		    strcpy(update_r_d->client, username);
		    update_r_d->server_index = temp->mail->server;
		    update_r_d->server_stamp = temp->mail->stamp;
		    
		    ret = SP_multicast(Mbox, AGREED_MESS, server_group_name,0,sizeof(Update_r_d), update_r_d);
		    if ( ret < 0) {
			SP_error(ret);
			Bye();
		    }
		    printf("Deleting a mail\n");
                    
                    free(temp_mail_list);
		    free(currmail);

                    Print_menu();
                    break;
            case 'r':
                    if (server == -1) {
                        printf("You have to connect to a server first!\n");
			Print_menu();
                        break;
                    }
		    update_l_p = malloc(sizeof(Update_l_p));
		    update_l_p->type = 5;
		    update_l_p->server_index = server;
		    strcpy(update_l_p->client, username);		    
		    //send the request for reading (operation = 5)
                    ret = SP_multicast(Mbox, AGREED_MESS, server_group_name,0,sizeof(Update_l_p),update_l_p);
		    if ( ret < 0) {
			SP_error(ret);
			Bye();
		    }

                    //temporary mail list 
                    Mail_list *temp_mail_list2 = malloc(sizeof(Mail_list));
		    temp_mail_list2->dummyhead.next = NULL;
                    temp_mail_list2->tail = &(temp_mail_list2->dummyhead);
		    temp_mail_list2->count = 0;
		    //wait to receive the msg
		    int all_received_flag3 = 0;
                    int index3 = 0;
                    printf("~~~User name: %s~~~\n", username);
                    printf("~~~Mail server index: %d~~~\n", server);
                    printf("~~~Listing of headers~~~\n");
		    currmail = malloc(sizeof(Mail));
                    while (all_received_flag3 == 0) {
    		        ret = SP_receive(Mbox, &service_type,sender, MAX_MEMBERS,&num_groups,target_groups,&mess_type,&endian_mismatch,sizeof(Mail), currmail);
		        if ( ret < 0) {
			    SP_error(ret);
			    Bye();
		        }
			if (currmail->read == 2) {
			    all_received_flag3 = 1;
 			    printf("~~~End of listing~~~\n");
			    break;
                        }
                        Mail *m = malloc(sizeof(Mail));
                        //copying fields
		        m->stamp = currmail->stamp;
                        m->server = currmail->server;
  			m->read = currmail->read;
			strcpy(m->sender, currmail->sender);
		        strcpy(m->receiver, currmail->receiver);
			strcpy(m->subject, currmail->subject);
			strcpy(m->msg, currmail->msg);
                        //adding to local list is not needed here
 			
                        Mail_node* mn = form_mail_node(m);
                        add_Mail_node_to_list(temp_mail_list2, mn);
                        //printing
                        index3++;
                        printf("Mail %d: \n", index3);
                        printf("      sender: %s", m->sender);
                        printf("  subject: %s \n", m->subject);
                        if (m->read == 1) {
			    printf("      R\n \n");
			} else {
			    printf("      N\n \n");
			}

		    }
		    printf("Please select which message you want to read: ");
		    char read_mail_s[80];
		    scanf("%s", read_mail_s);
		    read_mail = atoi(read_mail_s);

		    if (read_mail < 1) {
			printf("Index should be greater than or equal to 1\n");
                        
                        Print_menu();
			break;
		    }

		    //printf("%d",read_mail);
		    //read_mail = command[0];	
		    Mail_node *temp2 = &(temp_mail_list2->dummyhead);
                    int breakflag = 0;
		    for (int i = 0; i < read_mail;i++) {
			if (temp2->next != NULL) {
			    temp2 = temp2->next;
			} else {
			    printf("invalid number \n");
			    breakflag = 1;
			    break;
			}
		    } 

                    if (breakflag == 1) {

                        Print_menu();
			break;
                    }
		    update_r_d = malloc(sizeof(Update_r_d));
		    update_r_d->type = 3;
		    strcpy(update_r_d->client, username);
		    update_r_d->server_index = temp2->mail->server;
		    update_r_d->server_stamp = temp2->mail->stamp;
		    ret = SP_multicast(Mbox, AGREED_MESS, server_group_name,0,sizeof(Update_r_d), update_r_d);
		    if ( ret < 0) {
			SP_error(ret);
			Bye();
		    }

		    //SP_receive() 等待
		    printf("Here is the mail:\n\n");
                    ret = SP_receive(Mbox, &service_type,sender, MAX_MEMBERS,&num_groups,target_groups,&mess_type,&endian_mismatch,sizeof(Mail),currmail);
		    if ( ret < 0) {
			printf("sp_error when recieve msg for read (mail)\n");
			SP_error(ret);
			Bye();
		    }

                    if (currmail->stamp == -1) {
			printf("Sorry, the mail has already been deleted, or it is out of the range\n");
			
                        Print_menu();
                        break;
                    }
 		    printf("sender: %s\n", currmail->sender);
		    printf("receiver: %s\n",currmail->receiver);
		    printf("subject: %s\n", currmail->subject);
		    printf("msg: %s\n", currmail->msg);
                    free(currmail);
                    //free(temp_mail_list2);

                    Print_menu();
                    break;
            case 'v':
                    if (server == -1) {
                        printf("You have to connect to a server first!\n");
			Print_menu();
                        break;
                    }
		    update_l_p = malloc(sizeof(Update_l_p));
		    update_l_p->type = 6;
		    update_l_p->server_index = server;
		    strcpy(update_l_p->client, username);		    
		    //send the request for printing membership (operation = 6)
                    ret = SP_multicast(Mbox, AGREED_MESS, server_group_name,0,sizeof(Update_l_p),update_l_p);
		    if ( ret < 0) {
			SP_error(ret);
			Bye();
		    }
		    //SP_receive to wait
		    printf("here is the membership info: \n");

                    ret = SP_receive(Mbox, &service_type,sender, MAX_MEMBERS,&num_groups,target_groups,&mess_type,&endian_mismatch,sizeof(Update_k),m_info);
		    if ( ret < 0) {
			printf("sp_error when recieve msg for membership info\n");
			SP_error(ret);
			Bye();
		    }

                    if (m_info->type != 8) {
			printf("Weird, should be 8, but it is %d instead\n", m_info->type);
                    }
                    
 		    for (int member = 0; member < 6; member++) {
                        if (m_info->array[member] == 1) {
                            printf("server: %d\n",member);
                        }
                    }
                    printf("\n");

                    Print_menu();
                    break;
            case 'q':
                    Bye();
                    break;
            default:
                    printf("\nUnknown commnad\n");
		
                    Print_menu();
                    break;

        }

}

static void Print_menu()
{
    printf("\n");
    printf("==========\n");
    printf("Client Menu:\n");
    printf("\tu <name> -- login with a user name\n");
    printf("\tc <number> -- connect with a mail server\n");
    printf("\tl -- list the headers of received mail\n");
    printf("\tm -- mail a message to a user\n");
    printf("\td <number> -- delete a mail message\n");
    printf("\tr <number> -- read a received message\n");
    printf("\tv -- print the membership\n");
    printf("\tq -- quit\n");
    printf("User > ");
    fflush(stdout);
}

static  void    Bye()
{
    To_exit = 1;
    printf("\nBye.\n");
    SP_disconnect( Mbox );
    exit( 0 );
}

Mail_node* form_mail_node(Mail *m) {
    Mail_node* mn = malloc(sizeof(Mail_node));
    mn->mail = m;
    mn->next = NULL;
    return mn;
}

void add_Mail_node_to_list(Mail_list* ml, Mail_node* mn) {

    if (ml->count == 0) {
        ml->dummyhead.next = mn;
        ml->tail = mn;
        ml->tail->next = NULL;
        ml->count++;
    } else {
        ml->tail->next = mn;
        ml->tail = mn;
        ml->count++;
    }
}
