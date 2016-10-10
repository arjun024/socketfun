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
#include <pthread.h>

#define BUFF_SIZE 256
#define USERNAME_MAX_SIZE 20

static unsigned short port = 55555;
static char username[USERNAME_MAX_SIZE];

/*
* Conditional variables (@console_cv) permit us to wait until
* another thread completes an arbitrary activity.
* A mutex (@console_cv_lock) is required to protect the condition
* variable itself from race condition.
*/
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

	/*
	* Issue the prompt and wait for command,
	* process the command and
	* repeat forever
	*/
	while(1) {
		/* console prompt */
		printf("[%s]$ ", username);
		fgets(buffer, sizeof buffer, stdin);
		/* fgets also reads the \n from stdin, strip it */
		buffer[strlen(buffer) - 1] = '\0';

		if(strcmp(buffer, "") == 0)
			continue;

		if(strncmp(buffer, "exit", 4) == 0) {
			/* tell server to clean up structures for the client */
			write(sockfd, "exit", 6);
			/* clean up self and exit */
			pthread_mutex_destroy(&console_cv_lock);
			pthread_cond_destroy(&console_cv);
			_exit(EXIT_SUCCESS);
		}

		/*
		* `ls` is sent to server to get list of connected users.
		* It is written to server's socket, then using conditional wait,
		* we `wait` until the reply arrives in the receiver thread, where
		* `signal` is done immediately when the reply is read
		*/
		if(strncmp(buffer, "ls", 2) == 0) {
			/*
			* The mutex the protects the conditional has to
			* be locked before a conditional wait.
			*/
			pthread_mutex_lock(&console_cv_lock);
			write(sockfd, "ls", 2);
			/* not protected from spurious wakeups */
			/*
			* This operation unlocks the given mutex and waits until a 
			* pthread_cond_signal() happens on the same conditonal variable.
			* Then the given mutex is again unlocked
			*/
			pthread_cond_wait(&console_cv, &console_cv_lock);
			/* release the mutex */
			pthread_mutex_unlock(&console_cv_lock);
			continue;
		}

		/* `send <recipient> <msg>` sends <msg> to the given <username> */
		if(strncmp(buffer, "send ", 5) == 0) {
			/* the following is to validate the syntax */
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

			/* issue the `send` command to server */
			write(sockfd, buffer, 5 + strlen(recipient) + 1 + strlen(msg) + 1);
			continue;
		}

		error();
	}
}

/*
* write username to server
* in the syntax: register username <username>
*/
void register_username(int sockfd)
{
	char *regstring = malloc(USERNAME_MAX_SIZE + 18);
	sprintf(regstring, "register username %s", username);
	write(sockfd, regstring, strlen(regstring));
	free(regstring);
}

/*
* the stupid receiver thread
* It continuously waits for messages from the server,
* and prints it whatever it is.
*/
void *receiver(void *sfd)
{
	char buffer[BUFF_SIZE] = {0};
	int sockfd = *(int*)sfd;
	int readlen;
	/*
	* If a new msg is received when we are processing the prev msg,
	* the kernel buffers the received data for us since we use streaming sockets.
	* It waits in queue until the next read().
	* That's the reason we do not create a thread for every received msg.
	*/
	while(1) {
		memset(buffer, 0, sizeof buffer);
		readlen = read(sockfd, buffer, sizeof buffer);
		if(readlen < 1)
			continue;
		pthread_mutex_lock(&console_cv_lock);
		printf("%s\n", buffer);
		/* let the other thread stop waiting, if it is */
		pthread_cond_signal(&console_cv);
		pthread_mutex_unlock(&console_cv_lock);
	}
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

	/* just to dump the handle for the spawned thread - no use */
	pthread_t receiver_thread;

	pthread_cond_init(&console_cv, NULL);
	pthread_mutex_init(&console_cv_lock, NULL);

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
	connect(sockfd, (struct sockaddr*) &serv_addr, sizeof serv_addr);

	printf("%s\n", "Enter a username (max 20 characters, no spaces):");
	fgets(username, sizeof username, stdin);
	/* fgets also reads the \n from stdin, strip it */
	username[strlen(username) - 1] = '\0';

	register_username(sockfd);
	/* spawn a new thread that continuously listens for any msgs from server */
	pthread_create(&receiver_thread, NULL, receiver, (void*)&sockfd);
	/* get our console in action, let the user enter commands */
	console(sockfd);

	return 0;
}
