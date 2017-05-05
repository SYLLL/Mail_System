//Jiayao Wu, Suyi Liu
#include "sp.h"
#include "vector.h"
#include <sys/types.h>
#include "net_include.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define int32u unsigned int
#define BUF_SIZE MAX_MASS_LEN - 4*sizeof(int)
#define MAX_CLIENT_NAME_LENGTH 80

//print extreme cases where it gets weird updates
//to do: not optimized now: send mails back actually
//to do: free some memory (make sure variables are malloced and freed)
static  char    	User[80];
static  char 	        Spread_name[80];
static  char            *group = "ppap";
static  char            public_groupname[80];
 
static  char            Private_group[MAX_GROUP_NAME];
static  mailbox         Mbox;
static  int             To_exit = 0;
static  int             server_id;
static  int             ret;
//List of updates: vector array of vector of updates
static  Update_list*    updates[6]; //array of 5 pointers to Update_list
//stores the last update index of a specific server's list. If it has only 1 update in the list, store as 1
//static int             last_update_index[6];

static int              matrix[6][6];
//for example, I am server 2, server 2, 3, 4 are in the same partition, so other_members looks like[0, 0, 0, 1, 1, 5]
static int              other_members[6];
//for example, if server 2, 3, 4 are in the same partition, server 2 is in charge of 2, 5, 
//server 3 in charge of 1, 3
//server 4 in charge of 4 the array looks like[0, 3, 2, 3, 4, 5]
static int              who_is_in_charge_of_i[6];
//this is for indexing mail created on this server
static int              timestamp = 0;
//this is for indexing updates
static int              timeindex = 1;
//list all maillists: vector of pointers to MailList
static Mail_lists       *mls;

#define MAX_MESSLEN     1300   //102400
#define MAX_VSSETS      10
#define MAX_MEMBERS     10

//main fuction
int run_server(int server_id);
//wraps a mail into a mail node
Mail_node* form_mail_node(Mail *m);
//add a mail node into a mail list
void add_Mail_node_to_list(Mail_list* ml, Mail_node* mn);
//get specific mail from a list
Mail_node* get_Mail_node_from_list(Mail_list* ml, int stamp, int server);
//delete a mail node fron ml
int delete_Mail_node_from_list(Mail_list* ml, int stamp, int server);
//add a mail list to mls
void add_Mail_list_to_lists(Mail_list* ml); 
//get mail list from mls
Mail_list* get_Mail_list_from_lists(char *c_name);
//form a update node
Update_node* form_update_node(Update *u);
//insert node to list
void add_Update_node_to_update_list(int server_n, Update_node* un);
//broad its updates if it is in charge of it
void send_updates_in_range(int server_n, int begin);
//delete not necessary update nodes from list 
void shrink_update_list(int server_n, int smallest);
void Bye();

int main(int argc, char const *argv[]) {
    //initialize updates
    for (int i=0; i<6; i++) {
        updates[i] = malloc(sizeof(Update_list));
        updates[i]->head = NULL;
        updates[i]->tail = NULL;
        updates[i]->count = 0;
    }

    for (int p=0; p<6; p++) {
        for (int q=0; q<6; q++) {
            matrix[p][q] = 0;
        }
    }

    for (int member_index = 0; member_index < 6; member_index++) {
        other_members[member_index] = 0;
    }

    for (int charge_index = 0; charge_index < 6; charge_index++) {
        who_is_in_charge_of_i[charge_index] = 0;
    }

    sprintf( User, "yeah");
    sprintf( Spread_name, "10330");

    if (argc != 2) {
        printf("Usage: server <server_id>\n");
        return 1;
    }

    server_id = (int)strtol(argv[1],(char **)NULL, 10);    

    if (server_id < 1 || server_id > 5) {
         printf("Usage: server <server_id>\n");
        return 1;
    }

    run_server(server_id);
//main
}


int run_server(int server_id) {

    char              mess[MAX_MESS_LEN];
    int               currentMemberNum = 0;
    int               ret;
    //mail info temporary storage, for accessing the mail list
    char              mail_receiver[80];
    //update info temporary storage
    int               update_type;
    char              update_client[80];
    int               update_server_index;
    int               update_server_stamp;
    int               update_index;
    int               update_server;
    //malloc every time because of possible override
    int               service_type;
    int               mess_type;
    membership_info   memb_info;
    int               endian_mismatch;
    int               num_groups;
    char              target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
    char              sender[MAX_GROUP_NAME];
    int               mver, miver, pver;
    //used during partition
    Update_k          *uk;  
    Update_k          *uk_receive;
 
    //store itself's index into the 0 slot
    matrix[server_id][0] = server_id;  
    mls = malloc(sizeof(Mail_lists)); 
    mls->head = NULL;
    mls->tail = NULL;
    mls->count = 0; 

    ret = SP_connect(Spread_name, User, 0, 1, &Mbox, Private_group);
    if (ret < 0) {
        SP_error(ret);
        exit(1);
    }

    //public group    
    sprintf(public_groupname, "server_%d", server_id);

    //join its own public group
    ret = SP_join(Mbox, public_groupname);
    if( ret < 0 ) SP_error( ret );
    printf("Joined its own public group%s\n", public_groupname);

    //ppap is the public group
    ret = SP_join(Mbox, group);
    if( ret < 0 ) SP_error( ret );
    printf("Joined group of all servers%s\n", group);
    
    //do real job
    for(;;) {

        service_type = 0;
        ret = SP_receive(Mbox, &service_type, sender, MAX_MEMBERS, &num_groups, target_groups, &mess_type, &endian_mismatch, sizeof(mess), mess);
        if (ret < 0) {
            printf("Just received something\n");
            SP_error(ret);
            exit(1);
        }
      
        if (Is_membership_mess (service_type)) {
            //ignores self joining 
            if(strcmp(public_groupname, sender) == 0) {
                continue;
            }
	    printf("NEW ROUND For Parition\n");
  	    //refresh who is in charge of what
            for (int charge_index = 0; charge_index < 6; charge_index++) {
                who_is_in_charge_of_i[charge_index] = 0;
            }
      
            //refresh the membership infomation
            for (int member_index = 0; member_index < 6; member_index++) {
                other_members[member_index] = 0;
            }
            currentMemberNum = 0;
 
            //get how many servers are in the current partition
            ret = SP_get_memb_info( mess, service_type, &memb_info);
            if (ret < 0) {
                printf("membership message body invalid.\n");
                SP_error(ret);
                exit(1);
            }
            if (Is_reg_memb_mess(service_type)) {
                printf("Now %s has %d members, where I am member: %d\n", sender, num_groups, mess_type);
                //how many servers are in this partition
                currentMemberNum = num_groups;                                                                                                             
            }
            
            //multicast its own knowledge
            uk = malloc(sizeof(Update_k));
            uk->type = 9;
            for(int inde = 0; inde < 6; inde++) {
                uk->array[inde] = matrix[server_id][inde];
            }

            //how to tell the received server who I am??? store the info into arr[0]
            ret = SP_multicast( Mbox, AGREED_MESS, group, 0, sizeof(Update_k), uk);
            if( ret < 0 ) {
                printf("ret < 0\n");
                printf("Matrix multicast failure\n");
                SP_error( ret );
                Bye();
            }

                                   
            //collect all other arrays sent from other members
            int collected = 0;
            while(collected < currentMemberNum) {
                ret = SP_receive(Mbox, &service_type, sender, MAX_MEMBERS, &num_groups, target_groups, &mess_type, &endian_mismatch, sizeof(mess), mess);
	        //update the matrix
                Update *um = (Update *)mess;
                //is an Update_k
                if (um->type == 9) {
             
                    int slot = uk_receive->array[0];
                    //prevents receiving duplicates
		    if (other_members[slot] == 1) {
		        continue;
                    }
              
                    collected++;
                    uk_receive = (Update_k *)um;
                 
             	    //printf("Collected now is: %d, from %d, and sender %s\n", collected, uk_receive->array[0],sender);
		    //copying others info
                    other_members[slot] = 1;
                    for (int indexx = 0; indexx < 6; indexx++) {
                        matrix[slot][indexx] = uk_receive->array[indexx];
                    }
                }
            }
       
 	    printf("!!!Here is the fresh info of other members\n");
	    for (int theindex = 1; theindex < 6; theindex++) {
		printf("%d ", other_members[theindex]);
	    }            
            printf("\n");
                
       
 	    printf("!!!Here is the fresh info for the matrix\n");
            for (int thein = 1; thein < 6; thein++) {
                printf("%d:", thein);
	        for (int theindex = 0; theindex < 6; theindex++) {
	            printf("%d ", matrix[thein][theindex]);
	        }       
                printf("\n");
            }     
            printf("\n");
 
            

            //vertically compare: if there is a tie, the server with lower index sends update: >
            //other_members[server_id] = 1;    //temporarily setting this just for comparison

            int max_knowledge[6];
            for (int k_index = 0; k_index < 6; k_index++) {
                max_knowledge[k_index] = -1;
            }           
            
            //stores share min knowledge ABOUT server m_index 
            int min_knowledge[6];
            for (int m_index = 0; m_index < 6; m_index++) {
                min_knowledge[m_index] = INT_MAX;
            }

            //stores share min knowledge ABOUT server m_index 
            int min_knowledge_of_five[6];
            for (int m_index = 0; m_index < 6; m_index++) {
                min_knowledge_of_five[m_index] = INT_MAX;
            }

            //vertical comparison to decide who knows most
            for (int possible_member = 1; possible_member < 6; possible_member++) {
                //only join comparison if the server is in the partition
                if (other_members[possible_member] == 1) {
                    //traverse through knowledge vector to see any knowledge supasses the max of that slot
                    for (int theindex = 1; theindex < 6; theindex++) {
                        if (matrix[possible_member][theindex] >= max_knowledge[theindex]) {
                            max_knowledge[theindex] = matrix[possible_member][theindex];
                            who_is_in_charge_of_i[theindex] = possible_member;
                        }
                        if (matrix[possible_member][theindex] < min_knowledge[theindex]) {
                            min_knowledge[theindex] = matrix[possible_member][theindex];
                        }
                    }
                } 
                //for shrinking purpose
                for (int theindex = 1; theindex < 6; theindex++) {
                    if (matrix[possible_member][theindex] < min_knowledge_of_five[theindex]) {
                        min_knowledge_of_five[theindex] = matrix[possible_member][theindex];
                    }
                }		
            }

            //try to make a server to be in charge of itself, to prevent some ordering problems
            for (int possible_member = 1; possible_member < 6; possible_member++) {
                if(other_members[possible_member] == 1) {
                    who_is_in_charge_of_i[possible_member] = possible_member;
                }
            }

	    printf("Here is my updated who is in charge of i\n");
	    for (int theindex = 1; theindex < 6; theindex++) {
		printf("%d ", who_is_in_charge_of_i[theindex]);
	    }            
            printf("\n");
                     
            //multicast the updates, update the specific knowledge
            for (int index_to_send = 1; index_to_send < 6; index_to_send++) {
                //I'm in charge of that index of updates
                if (who_is_in_charge_of_i[index_to_send] == server_id) {
                    //send the updates
                    printf("Oh that's me. I am responsible for sending!\n");
		    int begin_index = min_knowledge[index_to_send] + 1;
                    send_updates_in_range(index_to_send, begin_index);
                //if is in charge   
                }
            //index to send loop
            }

            //shrink update list
            for (int shrink_server = 1; shrink_server < 6; shrink_server++) {
                shrink_update_list(shrink_server, min_knowledge_of_five[shrink_server]);
            }

	    printf("finished the membership\n");

        //if membership message
        } else {

            Update *u_temp = (Update *)mess;
            Update_add* ua;
            Update_r_d* ur;
            Update_r_d* ud;
            Update_l_p* ulist;
            Update_l_p* up;
            Update      *u;
            Mail*       update_mail;      
 
            int instru = u_temp->type;
            if (instru < 1 || instru > 6) {continue;}
            int ope;
   	    Mail_list *ml;
            switch(instru) {
                case 1:
                          u = malloc(sizeof(Update));
                          u->type = u_temp->type;
                          strcpy(u->client, u_temp->client);
                          u->operation = u_temp->operation;
                          u->server_index = u_temp->server_index;
                          u->server_stamp = u_temp->server_stamp;
                          u->server = u_temp->server;
                          u->index = u_temp->index;
                          u->mail = u_temp->mail;

                          printf("Received an update from a SERVER: sender: %s\n",sender);
                          update_index = u->index;
                          update_server = u->server;
	
                          Update_list  *ul = updates[update_server];
   			  if (update_index != matrix[server_id][update_server] + 1) {
      			      printf("In the method: We just ignored update %d on server %d\n Because our last index is only %d\n", update_index, update_server, matrix[server_id][update_server]);
       			      free(u);
			      break;
   		          }    
                          
                          ope = u->operation;
                          strcpy(update_client, u->client);
                          update_server_index = u->server_index;
                          update_server_stamp = u->server_stamp;

			  if (update_server == server_id) {
                              printf("This is my own update, but shouldn't be possible. \n");
  
                          }

                          switch(ope) {
                              case 2:
 				  update_mail = malloc(sizeof(Mail));
                                  strcpy(update_mail->sender, u->mail.sender);
                                  strcpy(update_mail->receiver,u->mail.receiver);
                                  strcpy(update_mail->subject, u->mail.subject);
                                  strcpy(update_mail->msg,u->mail.msg);
                                  update_mail->stamp = u->mail.stamp;
				  update_mail->server = u->mail.server;
				  update_mail->read = u->mail.read;


                                  printf("add from other server\n");
                                  //get maillist
                                  //add a new message into the maillists
                                  //add update and update matrix
                                  ml = get_Mail_list_from_lists(update_client);           
                                  
                                  if(ml == NULL) {
                                      ml = malloc(sizeof(Mail_list));
                                      strcpy(ml->client_name, update_client);
                                      ml->tail = &(ml->dummyhead);
				      ml->dummyhead.next = NULL;
                                      ml->count = 0;
                                      add_Mail_list_to_lists(ml);
                                   }
                                   Mail_node* mn = form_mail_node(update_mail);
                                   add_Mail_node_to_list(ml, mn);
                                   Update_node *un = form_update_node(u);
				   add_Update_node_to_update_list(update_server, un);

                                   break;
                              case 3:
  				     printf("read from other server\n");
 				     //get the mail list
                                     ml = get_Mail_list_from_lists(update_client);           
                                  
                                     if(ml == NULL) {
                                         ml = malloc(sizeof(Mail_list));
                                         strcpy(ml->client_name, update_client);
                                         ml->tail = &(ml->dummyhead);
					 ml->dummyhead.next = NULL;
                                         ml->count = 0;
                                         add_Mail_list_to_lists(ml);
                                         //cannot break, save it for future use
                                     } 
				     Update_node *un2 = form_update_node(u);
				     add_Update_node_to_update_list(update_server, un2);
                                   
				     read_Mail_node_from_list(ml, update_server_stamp,update_server_index);   			    
				     //get the mail by server_id and server_stamp
 				     //make operation
 				     //add to updatelist
 				     //change the matrix
                                     break;
                              case 4:
				     printf("delete from other server\n");
				     //get the mail list
				     //get the mail by two identifiers
				     //make operation
				     //add to updatelist
				     //change the matrix
                                     ml = get_Mail_list_from_lists(update_client);           
                                  
                                     if(ml == NULL) {
                                         ml = malloc(sizeof(Mail_list));
                                         strcpy(ml->client_name, update_client);
                                         ml->tail = &(ml->dummyhead);
                                         ml->count = 0;
				         ml->dummyhead.next = NULL;
                                         add_Mail_list_to_lists(ml);
                                         //cannot break, save it for future use
                                     } 
				     delete_Mail_node_from_list(ml, update_server_stamp,update_server_index);   	
 				     Update_node *un3 = form_update_node(u);
				     add_Update_node_to_update_list(update_server, un3);
                                    
				     break;
                              default:
				     printf("Impossible\n");
                                     break;
                          }
                        
		          break;
                case 2:  
                          ua = (Update_add *)u_temp;
			  strcpy(update_client, ua->client);
                          update_server_index = ua->server_index;   //就是server_id
                          //update_server_stamp = u->server_stamp;
                          //update_index = u->index;
                          //need to malloc each time it receives
			  Mail *new_mail = malloc(sizeof(Mail));
                          strcpy(new_mail->sender, ua->mail.sender);
                          strcpy(new_mail->receiver,ua->mail.receiver);
                          strcpy(new_mail->subject, ua->mail.subject);
                          strcpy(new_mail->msg,ua->mail.msg);
           
                          strcpy(mail_receiver, new_mail->receiver);                          
			  //the client wants to add
			  printf("add from client\n");
			  //add to corresponding mail list
                          ml = get_Mail_list_from_lists(mail_receiver);           
                          if(ml == NULL) {
			      printf("Mail list is null at this time\n");
                              ml = malloc(sizeof(Mail_list));
                              strcpy(ml->client_name, update_client);
                              ml->tail = &(ml->dummyhead);
                              ml->count = 0;
			      ml->dummyhead.next = NULL;
                              add_Mail_list_to_lists(ml);
                          }
                       
                          new_mail->stamp = timestamp;
 	                  new_mail->server = server_id;
			  new_mail->read = 0;
                          
                          Mail_node* mn = form_mail_node(new_mail);

                          add_Mail_node_to_list(ml, mn);
                                  
			  //add to update list
			  Update *new_up = malloc(sizeof(Update));
                          new_up->type = 1;
                          strcpy(new_up->client, update_client);
			  new_up->operation = 2;
                          new_up->server = server_id;
			  new_up->server_index = server_id;
			  new_up->server_stamp = timestamp;
                          timestamp++;
			  new_up->index = timeindex;
		          timeindex++;
			  new_up->mail = *new_mail;

                          Update_node *un4 = form_update_node(new_up);
	    	 	  add_Update_node_to_update_list(server_id, un4);
 
			  //broadcast update
                          ret = SP_multicast( Mbox, AGREED_MESS, group, 0, sizeof(Update), new_up);
                          if( ret < 0 ) {
                              printf("broadcasting of adding error, ret < 0\n");
                              SP_error( ret );
                              Bye();
                          }
                           

                          break;
                case 3:
			  //the client wants to read

                          ur = (Update_r_d *)u_temp;
                          strcpy(update_client, ur->client);
                          update_server_index = ur->server_index;
                          update_server_stamp = ur->server_stamp;
                          //update_index = u->index;
			  //the client wants to add
			  printf("read from client\n");
			  //add to corresponding mail list
                          ml = get_Mail_list_from_lists(update_client);           
                                  
                          if(ml == NULL) {
                              ml = malloc(sizeof(Mail_list));
                              strcpy(ml->client_name, update_client);
                              ml->tail = &(ml->dummyhead);
			      ml->dummyhead.next = NULL;
                              ml->count = 0;
                              add_Mail_list_to_lists(ml);
                          }
                          int read_result; 
                          read_result = read_Mail_node_from_list(ml, update_server_stamp, update_server_index); 

                          Mail *to_read;
			  if (read_result == -1) {
                              to_read = malloc(sizeof(Mail));
                              to_read->stamp = -1;
                              ret = SP_multicast( Mbox, AGREED_MESS, sender, 0, sizeof(Mail), to_read);
                              if( ret < 0 ) {
                                  printf("feedback to client of empty reading error ret < 0\n");
                                  SP_error( ret );
                                  Bye();
                              }
                          } else {
                              Mail_node *to_read_node = get_Mail_node_from_list(ml, update_server_stamp, update_server_index);
                              to_read = to_read_node->mail;
                          
                       
                              ret = SP_multicast( Mbox, AGREED_MESS, sender, 0, sizeof(Mail), to_read);
                              if( ret < 0 ) {
                                  printf("feedback to client of valid reading error, ret < 0\n");
                                  SP_error( ret );
                                  Bye();
                              }
                        
                          }
                          
			  //add to update list
			  Update *new_up2 = malloc(sizeof(Update));
                          new_up2->type = 1;
                          strcpy(new_up2->client, update_client);
			  new_up2->operation = 3;
                          new_up2->server = server_id;
			  new_up2->server_index = update_server_index;
			  new_up2->server_stamp = update_server_stamp;               
			  new_up2->index = timeindex;
		          timeindex++;

                          Update_node *un5 = form_update_node(new_up2);
	    	 	  add_Update_node_to_update_list(update_server_index, un5);
 
			  //broadcast update
                          ret = SP_multicast( Mbox, AGREED_MESS, group, 0, sizeof(Update), new_up2);
                          if( ret < 0 ) {
                              printf("broadcasting of reading error, ret < 0\n");
                              SP_error( ret );
                              Bye();
                          }

                          break;
                case 4:
			  //the client wants to delete
                          ud = (Update_r_d *)u_temp;
                          strcpy(update_client, ud->client);
                          update_server_index = ud->server_index;    //in this case, update_server_index = server_id
                          update_server_stamp = ud->server_stamp;
                          //update_index = u->index;
			  //the client wants to add
			  printf("delete from client\n");
			  //add to corresponding mail list
                          ml = get_Mail_list_from_lists(update_client);           
                          if(ml == NULL) {
                              ml = malloc(sizeof(Mail_list));
                              strcpy(ml->client_name, update_client);
                              ml->tail = &(ml->dummyhead);
			      ml->dummyhead.next = NULL;
                              ml->count = 0;
                              add_Mail_list_to_lists(ml);
                          }
                           
                          int delete_result = delete_Mail_node_from_list(ml, update_server_stamp, update_server_index); 
                                  
			  //add to update list
			  Update *new_up3 = malloc(sizeof(Update));
                          new_up3->type = 1;
                          strcpy(new_up3->client, update_client);
			  new_up3->operation = 4;
                          new_up3->server = server_id;
			  new_up3->server_index = update_server_index;
			  new_up3->server_stamp = update_server_stamp;               
			  new_up3->index = timeindex;
		          timeindex++;

                          Update_node *un6 = form_update_node(new_up3);
	    	 	  add_Update_node_to_update_list(update_server_index, un6);
 
			  //broadcast update
                          ret = SP_multicast( Mbox, AGREED_MESS, group, 0, sizeof(Update), new_up3);
                          if( ret < 0 ) {
                              printf("broadcasting of deletion error, ret < 0\n");
                              SP_error( ret );
                              Bye();
                          }
                           
			  //find corresponding mail list and mail, and delete
			  //add to update list
			  //broadcast update
                          break;
                case 5:
			  //the client wants to list
			  printf("The client wants to list\n");      
			  //send the mails in the list one by one
                          ulist = (Update_l_p *)u_temp;
			  strcpy(update_client, ulist->client);
                          update_server_index = ulist->server_index;
                          printf("the client wants to see %s\n",update_client); 
                          //need to malloc each time it receives
                          ml = get_Mail_list_from_lists(update_client);           
                          if(ml == NULL) {
			      printf("Mail list is null at this time\n");
                              ml = malloc(sizeof(Mail_list));
                              strcpy(ml->client_name, update_client);
                              ml->tail = &(ml->dummyhead);
			      ml->dummyhead.next = NULL;
                              ml->count = 0;
                              add_Mail_list_to_lists(ml);
                          }
                          
                          //send mails from the list
                          Mail_node *to_send_node = &(ml->dummyhead);
                          to_send_node = to_send_node->next;

                          while (to_send_node != NULL) {    
                              ret = SP_multicast( Mbox, AGREED_MESS, sender, 0, sizeof(Mail), to_send_node->mail);
			      printf("We sent mail with title: %s\n", to_send_node->mail->subject);
                              if( ret < 0 ) {
                                  printf("sending listing error, ret < 0\n");
                                  SP_error( ret );
                                  Bye();
                              }
                              to_send_node = to_send_node->next;
                          } 
			  //send the ending signal
			  Mail *end_signal = malloc(sizeof(Mail));
                          end_signal->read = 2;
                          ret = SP_multicast( Mbox, AGREED_MESS, sender, 0, sizeof(Mail), end_signal);
                          if( ret < 0 ) {
                              printf("sending ending signal error, ret < 0\n");
                              SP_error( ret );
                              Bye();
                          }
                            
			  printf("End signal sent.\n");
                          //no need to broadcast update
                          break;
                case 6:
			  //the client wants to print
			  printf("client wants to print\n");
                          Update_k *p_member = malloc(sizeof(Update_k));
                          for(int mem = 0; mem < 6; mem++) {
                              p_member->array[mem] = other_members[mem];
                              //for testing
			      if (other_members[mem] == 1) {printf("$%d",mem);}
                          }
                          p_member->type = 8;
                          //other_members[server_id] = 1;
			  //send membership back
                          ret = SP_multicast( Mbox, AGREED_MESS, sender, 0, sizeof(Update_k), p_member);
                          if( ret < 0 ) {
                              printf("sending membership error, ret < 0\n");
                              SP_error( ret );
                              Bye();
                          }
                          break;
                default:
                          printf("Should not fall into the case of default\n");
                          break;
            //main switch
            }            
    
      //if is signal of partition 
       }
    //running forever
    }
    return 0;
//run_server
}

//helper methods
Mail_node* form_mail_node(Mail *m) {
    Mail_node* mn = malloc(sizeof(Mail_node));
    mn->mail = m;
    mn->next = NULL;
    return mn;
}

void add_Mail_node_to_list(Mail_list* ml, Mail_node* mn) {

    if (ml->count == 0) {

        //empty mail list
        ml->dummyhead.next = mn;
        ml->tail = mn;
        ml->tail->next = NULL;
        ml->count++;
    } else {
        //implementing sorting here 
        //first sort by server, second sort by index       

        //the list has something in it
        Mail_node *temp;
        temp = &(ml->dummyhead);

        //traverse to the correct place ready to insert a mail
        //impossible to be the same I guess(because updates are unique)
        //insert it when it sees its next element's server index > itself's or same server index but larger stamp
        while (temp->next != NULL && ( temp->next->mail->server < mn->mail->server || (temp->next->mail->server == mn->mail->server && temp->next->mail->stamp < mn->mail->stamp) )) {
            if  (temp->next->mail->server == mn->mail->server && temp->next->mail->stamp == mn->mail->stamp) {
                printf("Stamp and server overlap???!!!\n");
            }
            temp = temp->next;
        }
        
        mn->next = temp->next;
        temp->next = mn;            
        ml->count++;     

        if (mn->next == NULL) {
            //need to modify the tail pointer
            ml->tail = mn;
        }
        

    }

    printf("$$$ Added mail with title: %s to the list of client: %s\n", mn->mail->subject, ml->client_name);
}


Mail_node* get_Mail_node_from_list(Mail_list* ml, int stamp, int server) {
    Mail_node *temp = &(ml->dummyhead);
    while (temp->next != NULL) {
        if (temp->next->mail->stamp == stamp && temp->next->mail->server == server) {
            return temp->next;
        }
        temp = temp->next;
    }
    return NULL;
}


int delete_Mail_node_from_list(Mail_list* ml, int stamp, int server) {
    Mail_node *temp = &(ml->dummyhead);
    Mail_node *pelement;
    while (temp->next != NULL) {
        if (temp->next->mail->stamp == stamp && temp->next->mail->server == server) {
            pelement = temp->next;
            temp->next = pelement->next;
            
	    //if after deletion, temp becomes last
            if (temp->next == NULL) {
	        ml->tail = temp;
            }

            free(pelement);
            ml->count--; 
            return 0;
        }
        temp = temp->next;
    }
    
    return -1;
}

int read_Mail_node_from_list(Mail_list* ml, int stamp, int server) {
    Mail_node *temp = &(ml->dummyhead);
    while (temp->next != NULL) {
        if (temp->next->mail->stamp == stamp && temp->next->mail->server == server) {
            temp->next->mail->read = 1; //for read
	    return 0;
        }
        temp = temp->next;
    }
    return -1;
}


void add_Mail_list_to_lists(Mail_list* ml) {
    if (mls->count == 0) {
        mls->head = ml;
        mls->tail = ml;
        mls->tail->next = NULL;
        mls->count++;
    } else {
        
        mls->tail->next = ml;
        mls->tail = ml;
        ml->next = NULL;
        mls->count++;
    }
}

Mail_list* get_Mail_list_from_lists(char *c_name) {
    Mail_list* temp = mls->head;
    while(temp != NULL) {
        if(strcmp(temp->client_name, c_name) == 0) {
            return temp;
        }
        temp = temp->next;
    }
    return NULL;
}

Update_node* form_update_node(Update *u) {
    Update_node* un = malloc(sizeof(Update_node));
    un->update = u;
    un->next = NULL;
    return un;
}

void add_Update_node_to_update_list(int server_n, Update_node* un) {
    //ignores the update which is not the next update
    Update_list  *ul = updates[server_n];
    if (un->update->index != matrix[server_id][server_n] + 1) {
        printf("We just ignored update %d on server %d\n Because our last index is only %d\n Not likely to fall into this case but just in case\n", un->update->index, server_n, matrix[server_id][server_n]);
        return;
    }    


    if (ul->count == 0) {
        if (ul->head == NULL) {
            printf("Case count is 0, head BEFORE is nothing\n");
        }
        ul->head = un;
        ul->tail = un;
        ul->tail->next = NULL;
        ul->count++;
        printf("Case count is 0, head AFTER is: %d\n", ul->head->update->index);
    } else {
        printf("Case count is not 0, head BEFORE is: %d\n", ul->head->update->index);   
        ul->tail->next = un;
        ul->tail = un;
        un->next = NULL;
        ul->count++;
        printf("Case count is not 0, head AFTER is: %d\n", ul->head->update->index);
    }
    matrix[server_id][server_n]++; 
    
    printf("Added new knowledge to server: %d, updated to %d\n", server_n, matrix[server_id][server_n]);
}

//reflection: we do not need end index here
void send_updates_in_range(int server_n, int begin) {
    Update_list  *ul = updates[server_n];
    if (ul->head == NULL || ul->head->update->index > begin) {
        if (ul->head == NULL) {
	    printf("@U@ Head is NULLLLLL\n");
        } else {
            printf("@.@ Head index is %d", ul->head->update->index);
	    printf("@v@ Begin index is %d", begin);
        }
        printf("Nothing to send\n");
        return;
    }

    Update_node* temp = ul->head;
    while (temp != NULL) {
        if (temp->update->index == begin) {
            break;
        }

        temp = temp->next;
    }    


    if (temp == NULL) { printf("OMG This is impossible!");}
 

    //send till the end
    while (temp != NULL) {
        printf("### For debugging: the sent update's index is: %d\n", temp->update->index);
        //multicast
        ret = SP_multicast( Mbox, AGREED_MESS, group, 0, sizeof(Update), temp->update);
        if( ret < 0 ) {
            printf("ret < 0\n");
            SP_error( ret );
        }
        temp = temp->next;
    }
}

void shrink_update_list(int server_n, int smallest) {
    printf("Shrinking of server: %d called until %d\n", server_n, smallest);

    Update_list  *ul = updates[server_n];
    if (ul->head == NULL) {
        printf("head is null, unable to shrink\n");
        return;
    }
    
    Update_node* temp = ul->head;
    while (temp != NULL && temp->update->index <= smallest) {
        //deletable
        Update_node *pelement = temp;
        temp = temp->next;
        printf("Freed update No: %d\n",pelement->update->index);
        free(pelement);
        ul->count--;
    }

    ul->head = temp;

    if (ul->head == NULL) {
        ul->tail = NULL;
    }
    printf("Shrinking over\n \n");
}

void Bye() {
    To_exit = 1;
    printf("\nBye. \n");
    SP_disconnect( Mbox );
    exit( 0 );
}
