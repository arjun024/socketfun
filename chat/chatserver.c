/*
* Author:  Arjun Sreedharan
* License: GPL version 2 or higher
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

struct client_node {
	int sockfd;
	char username[USERNAME_MAX_SIZE];
	struct client_node *next;
};

struct client_node *client_list = NULL;

pthread_mutex_t client_list_lock;

struct client_node *add_client(int cfd)
{
	struct client_node *p = client_list;
	struct client_node *c = malloc(sizeof(struct client_node));
	c->sockfd = cfd;
	c->username[0] = '\0';
	c->next = NULL;
	while(p && p->next)
		p = p->next;
	if(p == NULL)
		client_list = c;
	else
		p->next = c;
	return c;
}

struct client_node *search_client_list(char *recipient)
{
	struct client_node *p = client_list;
	if(recipient == NULL || *recipient == '\0')
		return NULL;
	pthread_mutex_lock(&client_list_lock);
	while(p != NULL) {
		if(strcmp(p->username, recipient) == 0) {
			pthread_mutex_unlock(&client_list_lock);
			return p;
		}
		p = p->next;
	}
	pthread_mutex_unlock(&client_list_lock);
	return NULL;
}

void remove_client(struct client_node *c)
{
	struct client_node *p, *prev;
	p = client_list;
	prev = NULL;
	pthread_mutex_lock(&client_list_lock);
	if(p == c) {
		client_list = p->next;
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
	return strrchr(str, ' ') + 1;
}

void *handle_client(void* c)
{
	char buffer[BUFF_SIZE] = {0};
	struct client_node *cnode, *targetnode, *tmpnode;
	char *recipient, *msg, *tmp, *formatted_msg;
	cnode = (struct client_node *)c;
	strcpy(cnode->username, get_username(cnode));
	printf("user: %s, socket: %d, thread:%lu\n", 
		cnode->username, cnode->sockfd, pthread_self());

	while(1) {
		memset(buffer, 0, sizeof buffer);
		read(cnode->sockfd, buffer, sizeof buffer);

		if(strncmp(buffer, "exit", 4) == 0) {
			remove_client(cnode);
			close(cnode->sockfd);
			free(cnode);
		}

		if(strncmp(buffer, "ls", 2) == 0) {
			memset(buffer, 0, sizeof buffer);
			tmpnode = client_list;
			while(tmpnode) {
				strcat(buffer, tmpnode->username);
				strcat(buffer, "\n");
				tmpnode = tmpnode->next;
			}
			write(cnode->sockfd, buffer, strlen(buffer));
		}

		if(strncmp(buffer, "send ", 5) == 0) {
			tmp = strchr(buffer, ' ');
			if(tmp == NULL)
				continue;
			recipient = tmp + 1;

			tmp = strchr(recipient, ' ');
			if(tmp == NULL)
				continue;
			*tmp = '\0';
			msg = tmp + 1;
			targetnode = search_client_list(recipient);
			if(targetnode == NULL)
				continue;
			formatted_msg = malloc(BUFF_SIZE);
			if(BUFF_SIZE < strlen(cnode->username) + strlen(msg))
				continue;
			sprintf(formatted_msg, "%s: %s", cnode->username, msg);
			printf("%s sent msg to %s\n", cnode->username, targetnode->username);
			write(targetnode->sockfd, formatted_msg, strlen(formatted_msg) + 1);
			free(formatted_msg);
		}
	}

	/* exit */
	pthread_mutex_destroy(&client_list_lock);
	close(cnode->sockfd);
	return NULL;
}

int main(void)
{
	int sockfd, client_sockfd;
	struct sockaddr serv_addr, client_addr;
	char filler[16] = {0};
	unsigned int supplied_len;
	unsigned int *ip_suppliedlen_op_storedlen;
	pthread_t thread;

	pthread_mutex_init(&client_list_lock, NULL);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	memset(&serv_addr, 0, sizeof serv_addr);
	filler[0] = (unsigned short)AF_INET;
	filler[2] = (unsigned short)htons(port);
	filler[4] = (unsigned long)htonl(INADDR_ANY);
	memcpy(&serv_addr, filler, sizeof(serv_addr));

	bind(sockfd, &serv_addr, sizeof serv_addr);

	listen(sockfd, 5);

	supplied_len = sizeof(client_addr);
	ip_suppliedlen_op_storedlen = &supplied_len;

	while(1) {
		struct client_node *cnode;
		client_sockfd = accept(sockfd, &client_addr, ip_suppliedlen_op_storedlen);

		pthread_mutex_lock(&client_list_lock);
		cnode = add_client(client_sockfd);
		pthread_mutex_unlock(&client_list_lock);

		pthread_create(&thread, NULL, handle_client, (void*)cnode);
	}
	return 0;
}
