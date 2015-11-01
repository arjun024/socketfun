/*
* Copyright (C) 2015  Arjun Sreedharan
* License: GPL version 2 or higher http://www.gnu.org/licenses/gpl.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUFF_SIZE 256
#define USERNAME_MAX_SIZE 20

static unsigned short port = 55555;

/* client_node abstracts a connected client */
struct client_node {
	int sockfd;
	char username[USERNAME_MAX_SIZE];
	struct client_node *next;
};

/*
* a list of `client_node`s which serves as our
* connected client list
*/
struct client_node *client_list = NULL;

/*
* any operation on the client list is to be performed
* only after getting a lock on this mutex
*/
pthread_mutex_t client_list_lock;

/* add to the linked list - no rocket science */
struct client_node *add_client(int cfd)
{
	struct client_node *p = client_list;
	struct client_node *c = malloc(sizeof(struct client_node));
	c->sockfd = cfd;
	c->username[0] = '\0';
	c->next = NULL;
	/* always get a lock before you mess with list */
	pthread_mutex_lock(&client_list_lock);
	while(p && p->next)
		p = p->next;
	if(p == NULL)
		client_list = c;
	else
		p->next = c;
	/* release the lock when we are done with */
	pthread_mutex_unlock(&client_list_lock);
	return c;
}

/*
* return NULL if no node is present with username @recipient,
* else return pointer to the node
*/
struct client_node *search_client_list(char *recipient)
{
	struct client_node *p = client_list;
	if(recipient == NULL || *recipient == '\0')
		return NULL;
	/* I am a law abiding citizen, wait until I get a lock */
	pthread_mutex_lock(&client_list_lock);
	while(p != NULL) {
		if(strcmp(p->username, recipient) == 0) {
			/* never forget to release the lock, don't you like freedom */
			pthread_mutex_unlock(&client_list_lock);
			return p;
		}
		p = p->next;
	}
	pthread_mutex_unlock(&client_list_lock);
	return NULL;
}

/* remove the client fromt he list of clients */
void remove_client(struct client_node *c)
{
	struct client_node *p, *prev;
	p = client_list;
	prev = NULL;
	/* get a lock and only then touch the list */
	pthread_mutex_lock(&client_list_lock);
	if(p == c) {
		client_list = p->next;
		pthread_mutex_unlock(&client_list_lock);
		return;
	}
	while(p != c && p != NULL) {
		prev = p;
		p = p->next;
	}
	if(prev && p)
		prev->next = p->next;
	pthread_mutex_unlock(&client_list_lock);
}

char *get_username(struct client_node *cnode)
{
	char *str = malloc(38);
	read(cnode->sockfd, str, 38);
	/*
	* gives pointer to the character after the second space,
	* assuming str to be "register username <username>"
	*/
	return strrchr(str, ' ') + 1;
}

void *handle_client(void* c)
{
	char buffer[BUFF_SIZE] = {0};
	struct client_node *cnode, *targetnode, *tmpnode;
	char *recipient, *msg, *tmp, *formatted_msg;
	/* cast the void* pointer back to its original type */
	cnode = (struct client_node *)c;

	/*
	* expecting client program's register_username() to send
	* a msg of syntax "register username <username>"
	* This parsed by get_username() to retrieve username,
	* and then copied to the client's node.
	*/
	strcpy(cnode->username, get_username(cnode));
	/* logging in the server */
	printf("user: %s, socket: %d, thread:%lu\n",
		cnode->username, cnode->sockfd, (unsigned long)pthread_self());

	/*
	* read each `instruction` from the client,
	* take approprite action, and then come back to beginning
	* of the loop to wait for further instructions
	*/
	while(1) {
		memset(buffer, 0, sizeof buffer);
		/* read() call blocks till receipt of a msg */
		read(cnode->sockfd, buffer, sizeof buffer);

		if(strncmp(buffer, "exit", 4) == 0) {
			/* clean up when a client quits */
			remove_client(cnode);
			close(cnode->sockfd);
			free(cnode);
		}

		/*
		* `ls` lists all clients' usernames that are
		* currently connected
		*/
		if(strncmp(buffer, "ls", 2) == 0) {
			memset(buffer, 0, sizeof buffer);
			tmpnode = client_list;
			while(tmpnode) {
				/* concatenate each username to the buffer */
				strcat(buffer, tmpnode->username);
				strcat(buffer, "\n");
				tmpnode = tmpnode->next;
			}
			/* write the buffer to cient's socket */
			write(cnode->sockfd, buffer, strlen(buffer));
		}

		/* `send <recipient> <msg>` sends <msg> to the given <username> */
		if(strncmp(buffer, "send ", 5) == 0) {
			/* parse buffer to separate recipient and msg */
			tmp = strchr(buffer, ' ');
			if(tmp == NULL)
				continue;
			recipient = tmp + 1;

			tmp = strchr(recipient, ' ');
			if(tmp == NULL)
				continue;
			*tmp = '\0';
			msg = tmp + 1;

			/* search for the recipient in the cient list */
			targetnode = search_client_list(recipient);

			/* on invalid recipient, do nothing */
			if(targetnode == NULL)
				continue;

			formatted_msg = malloc(BUFF_SIZE);
			/* sprint is notorious for buffer overflow */
			if(BUFF_SIZE < strlen(cnode->username) + strlen(msg) + 2)
				continue;
			/* create a string of syntax `<sender>: <msg>` to send to recipient */
			sprintf(formatted_msg, "%s: %s", cnode->username, msg);
			/* logging in the server */
			printf("%s sent msg to %s\n", cnode->username, targetnode->username);
			/* Hey target client, You've got message ;) */
			write(targetnode->sockfd, formatted_msg, strlen(formatted_msg) + 1);
			free(formatted_msg);
		}
	}

	/*
	* clean up
	* Let me tell you a secret, this never happens
	* Oh, wait !!
	*/
	pthread_mutex_destroy(&client_list_lock);
	close(cnode->sockfd);
	return NULL;
}

int main(void)
{
	int sockfd, client_sockfd;
	/*
	* this is the container for socket's address that contains
	* address family, ip address, port
	*/
	struct sockaddr serv_addr, client_addr;
	char filler[16] = {0};
	unsigned int supplied_len;
	unsigned int *ip_suppliedlen_op_storedlen;
	/* just to dump the handle for the spawned thread - no use */
	pthread_t thread;

	/*
	* initiate a mutex to protect the client list from
	* being accessed by multiple threads at the same time
	*/
	pthread_mutex_init(&client_list_lock, NULL);

	/*
	* creates a socket of family Internet sockets (AF_INET) and
	* of type stream. 0 indicates to system to choose appropriate
	* protocol (eg: TCP)
	*/
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	/*
	* Address represented by struct sockaddr:
	* first 2 bytes: Address Family,
	* next 2 bytes: port,
	* next 4 bytes: ipaddr,
	* next 8 bytes: zeroes
	*/
	/*
	* htons() and htonl() change endianness to
	* network order which is the standard for network
	* communication.
	*/
	filler[0] = AF_INET & 0xFF;
	filler[1] = AF_INET >> 8 & 0xFF;
	filler[2] = htons(port) & 0xFF;
	filler[3] = htons(port) >> 8 & 0xFF;
	filler[4] = htonl(INADDR_ANY) & 0xFF;
	filler[5] = htonl(INADDR_ANY) >> 8 & 0xFF;
	filler[6] = htonl(INADDR_ANY) >> 16 & 0xFF;
	filler[7] = htonl(INADDR_ANY) >> 24 & 0xFF;

	/*
	* The following method of memcpy-ing is a little risky.
	* It's best done using a structure sockaddr_in like:
	* struct sockaddr_in serv_addr;
	* serv_addr.sin_family = AF_INET;
	* serv_addr.sin_port = htons(port);
	* serv_addr.sin_addr.s_addr = INADDR_ANY;
	* Why didn't I use sockaddr_in?
	* * sockaddr_in is just a wrapper around sockaddr
	* * Functions like connect() do not know of any type sockaddr_in
	* This is just to demonstrate how socket address is read
	*/
	/* copy data in the filler buffer to the socket address */
	memcpy(&serv_addr, filler, sizeof(serv_addr));

	/* binds a socket to an address */
	bind(sockfd, &serv_addr, sizeof serv_addr);

	/*
	* allows the process to listen on the socket for given
	* max number of connections
	*/
	listen(sockfd, 5);

	/*
	* This ptr on input specifies the length of the supplied sockaddr,
	* and on output specifies the length of the stored address
	*/
	supplied_len = sizeof(client_addr);
	ip_suppliedlen_op_storedlen = &supplied_len;

	/*
	* Now ready to accept clients -
	* For each client, a new client_node is created and added to the client list.
	* To handle the client, a new thread is spawned handled by handle_client().
	* Then, we let the main thread come back to the beginning of the loop to wait for
	* further clients while the newly spawned thread deals with the accepted client.
	*/
	while(1) {
		struct client_node *cnode;
		/*
		* causes the process to block until a client connects to the server,
		* returns a new file descriptor to communicate with the connected client
		*/
		client_sockfd = accept(sockfd, &client_addr, ip_suppliedlen_op_storedlen);

		cnode = add_client(client_sockfd);

		/* pass a pointer to the correspond client node to the new thread's handler */ 
		pthread_create(&thread, NULL, handle_client, (void*)cnode);
	}
	return 0;
}
