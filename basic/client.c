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

void interact_with_server(int sfd)
{
	char buffer[BUFF_SIZE] = {0};
	/* Wait til receiving "You are now connected.\nEnter username:" */
	read(sfd, buffer, sizeof buffer);
	/* clear buffer */
	memset(buffer, 0, sizeof buffer);

	printf("%s\n", "Enter your username:");
	/* read username */
	fgets(buffer, sizeof buffer, stdin);
	buffer[strlen(buffer) - 1] = '\0';

	/* send username to server*/
	write(sfd, buffer, strlen(buffer));

	/* clear buffer */
	memset(buffer, 0, sizeof buffer);
	/* Wait til receiving server's response to username */
	read(sfd, buffer, sizeof buffer);
	printf("%s\n", buffer);
	return;
}

int main(void)
{
	int sockfd;
	char filler[16] = {0};
	/*
	* this is the container for socket's address that contains
	* address family, ip address, port
	*/
	struct sockaddr serv_addr;

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

	/* makes connection per the socket address */
	connect(sockfd, &serv_addr, sizeof(serv_addr));

	interact_with_server(sockfd);
	return 0;
}
