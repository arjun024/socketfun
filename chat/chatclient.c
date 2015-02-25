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
static char username[USERNAME_MAX_SIZE];

pthread_cond_t console_cv;
pthread_mutex_t console_cv_lock;

void error(void)
{
	fprintf(stderr, "%s\n", "bad command\n"
		"syntax: [command] [optional recipient] [optional msg]");
}

void console(int sockfd)
{
	char buffer[BUFF_SIZE];
	char *recipient, *msg, *tmp;
	memset(buffer, 0, sizeof buffer);
	printf("%s\n%s\n", "Welcome to chat client console. Please enter commands",
		"syntax: [command] [optional recipient] [optional msg]");
	while(1) {
		printf("[%s]$ ", username);
		fgets(buffer, sizeof buffer, stdin);
		buffer[strlen(buffer) - 1] = '\0';

		if(strcmp(buffer, "") == 0)
			continue;

		if(strncmp(buffer, "exit", 4) == 0) {
			write(sockfd, "exit", 6);
			pthread_mutex_destroy(&console_cv_lock);
			pthread_cond_destroy(&console_cv);
			_exit(EXIT_SUCCESS);
		}

		if(strncmp(buffer, "ls", 2) == 0) {
			pthread_mutex_lock(&console_cv_lock);
			write(sockfd, "ls", 2);
			/* not protected from spurious wakeups */
			pthread_cond_wait(&console_cv, &console_cv_lock);
			pthread_mutex_unlock(&console_cv_lock);
			continue;
		}

		if(strncmp(buffer, "send ", 5) == 0) {
			tmp = strchr(buffer, ' ');
			if(tmp == NULL) {
				error();
				continue;
			}
			recipient = tmp + 1;

			tmp = strchr(recipient, ' ');
			if(tmp == NULL) {
				error();
				continue;
			}
			msg = tmp + 1;

			write(sockfd, buffer, 5 + strlen(recipient) + 1 + strlen(msg) + 1);
			continue;
		}

		error();
	}
}

void register_username(int sockfd)
{
	char *regstring = malloc(USERNAME_MAX_SIZE + 18);
	sprintf(regstring, "register username %s", username);
	write(sockfd, regstring, strlen(regstring));
	free(regstring);
}

void *receiver(void *sfd)
{
	char buffer[BUFF_SIZE] = {0};
	int sockfd = *(int*)sfd;
	int readlen;
	while(1) {
		memset(buffer, 0, sizeof buffer);
		readlen = read(sockfd, buffer, sizeof buffer);
		if(readlen < 1)
			continue;
		pthread_mutex_lock(&console_cv_lock);
		printf("%s\n", buffer);
		pthread_cond_signal(&console_cv);
		pthread_mutex_unlock(&console_cv_lock);
		/* flush because previous printf doesn't end in \n */
		fflush(stdout);
	}
	return NULL;
}

int main()
{
	int sockfd;
	struct sockaddr serv_addr;
	char filler[16] = {0};
	pthread_t receiver_thread;

	pthread_cond_init(&console_cv, NULL);
	pthread_mutex_init(&console_cv_lock, NULL);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	filler[0] = (unsigned short)AF_INET;
	filler[2] = (unsigned short)htons(port);
	filler[4] = (unsigned long)htonl(INADDR_ANY);
	memcpy(&serv_addr, filler, sizeof serv_addr);

	bind(sockfd, &serv_addr, sizeof serv_addr);

	connect(sockfd, &serv_addr, sizeof serv_addr);

	printf("%s\n", "Enter a username (max 20 characters, no spaces):");
	fgets(username, sizeof username, stdin);
	username[strlen(username) - 1] = '\0';

	register_username(sockfd);
	pthread_create(&receiver_thread, NULL, receiver, (void*)&sockfd);
	console(sockfd);

	return 0;
}
