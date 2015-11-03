/*
* Author:  Arjun Sreedharan
* License: GPL version 2 or higher http://www.gnu.org/licenses/gpl.html
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

	/*
	* struct sockaddr defines a socket address.
	* A socket address is a combination of address family,
	* ip address and port.
	* For IP sockets, we may use struct sockaddr_in which is
	* just a wrapper around struct sockaddr.
	* Funtions like bind() etc are only aware of struct sockaddr.
	*/
	struct sockaddr_in serv_addr;

	/*
	* creates a socket of family Internet sockets (AF_INET) and
	* of type stream. 0 indicates to system to choose appropriate
	* protocol (eg: TCP)
	*/
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	/*
	* Socket adddress represented by struct sockaddr:
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

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	/*
	* The above achieves what could be done using the following
	* on a little endian machine.
	* This breaks if the structure has padding
		char filler[16] = {0};
		filler[0] = AF_INET & 0xFF;
		filler[1] = AF_INET >> 8 & 0xFF;
		filler[2] = htons(port) & 0xFF;
		filler[3] = htons(port) >> 8 & 0xFF;
		filler[4] = htonl(INADDR_ANY) & 0xFF;
		filler[5] = htonl(INADDR_ANY) >> 8 & 0xFF;
		filler[6] = htonl(INADDR_ANY) >> 16 & 0xFF;
		filler[7] = htonl(INADDR_ANY) >> 24 & 0xFF;
		memcpy(&serv_addr, filler, sizeof(serv_addr));
	*/

	/*
	* Note that we do not bind() our socket to any socket adddress here.
	* This is because on the client side, you would only use bind() if you want
	* to use a particular client side port to connect to the server.
	* When you do not bind(), the kernel will pick a port for you.
	* Read here how kernel gets you a port: https://idea.popcount.org/2014-04-03-bind-before-connect
	* There are a few protocols in the Unix world that expect clients to connect from a particular port.
	* Create a new socket address definition and bind it to socket in such cases:
		struct sockaddr_in client_addr;
		client_addr.sin_family = AF_INET;
		client_addr.sin_port = htons(CLIENT_PORT);
		client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		bind(sockfd, (struct sockaddr*) &client_addr, sizeof client_addr);
	*/

	/* makes connection per the socket address */
	connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));

	interact_with_server(sockfd);

	/* close socket since we are done */
	close(sockfd);

	return 0;
}
