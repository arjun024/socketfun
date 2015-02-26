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

#define BUFF_SIZE 256

static unsigned short port = 55555;

void serve_client(int cfd)
{
	char buffer[BUFF_SIZE] = {0};
	write(cfd, "You are now connected.\nEnter username:", 38);
	/* wait for username */
	read(cfd, buffer, BUFF_SIZE);

	if(strcmp(buffer, "arjun024")) {
		write(cfd, "Authentication failed", 21);
		printf("A user tried to login using: %s\n", buffer);
		return;
	}
	write(cfd, "Authentication success", 22);
	printf("%s has logged in.\n", buffer);
	return;
}

int main(void)
{
	int sockfd, client_sockfd;
	pid_t pid;
	char filler[16] = {0};
	/*
	* this is the container for socket's address that contains
	* address family, ip address, port
	*/
	struct sockaddr serv_addr, client_addr;
	unsigned int supplied_len;
	unsigned int *ip_suppliedlen_op_storedlen;

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
	* next 2 bytes: ipaddr,
	* next 8 bytes: zeroes
	*/
	/*
	* htons() and htonl() change endianness to
	* network order which is the standard for network
	* communication.
	*/
	filler[0] = (unsigned short)AF_INET;
	filler[2] = (unsigned short)htons(port);
	filler[4] = (unsigned long)htonl(INADDR_ANY);

	/* copy data in the filler buffer to the socket address */
	memcpy(&serv_addr, filler, sizeof(serv_addr));

	/* binds a socket to an address */
	bind(sockfd, &serv_addr, sizeof(serv_addr));

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
	* ready to accept multiple clients -
	* for each client, we will fork and let the parent come back to
	* the beginning of the loop to wait for further clients
	* while the child deals with the accepted client.
	*/
	while(1) {
		/*
		* causes the process to block until a client connects to the server,
		* returns a new file descriptor to communicate with the connected client
		*/
		client_sockfd = accept(sockfd, &client_addr, ip_suppliedlen_op_storedlen);

		pid = fork();

		if(pid > 0) {
			/* parent */
			close(client_sockfd);
			continue;
		}

		if(pid == 0) {
			/* child */
			close(sockfd);
			serve_client(client_sockfd);
			break;
		}
	}
	return 0;
}
