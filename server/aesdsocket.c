/*
** server.c -- a stream socket server demo
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/queue.h>
#include <sys/time.h>

#include "../aesd-char-driver/aesd_ioctl.h"

#define LISTEN_PORT "9000"  
#define MAX_BUF 1024
#define MAX_PACKET_BUF 65000

#define USE_AESD_CHAR_DEVICE 1

#ifdef USE_AESD_CHAR_DEVICE
#define SAVE_FILE "/dev/aesdchar"
#else
#define SAVE_FILE "/var/tmp/aesdsocketdata"
#endif


#define BACKLOG 10	 // how many pending connections queue will hold

typedef struct slist_data_s slist_date_t;

struct slist_data_s {
	int fd_client;
	char *addr;
    pthread_t thread_id;
	bool thread_complete;
	bool thread_joined;
	SLIST_ENTRY(slist_data_s) entries;
};

// set linked list
SLIST_HEAD(slisthead, slist_data_s) head; 

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// file descriptor
int frw = -1; //, fw = -1, fr = -1;

// control threads
bool exit_triggered = false;

// thread function
void *session_handler(void *);
//
void clean_up(); // free resources

/* Signal Handlers */

// signal handler

static void exit_signal_handler (int signo) 
{
	syslog(LOG_DEBUG, "Caught signal, exiting");

	// clean up
	//if (fr != -1) close(fr);
	//if (fw != -1) close(fw);
	if (frw != -1) close(frw);

	remove(SAVE_FILE); // delete the file

	//
	if (signo == SIGINT) 
		printf("Exit by SIGINT\n");
	else if (signo == SIGTERM)
		printf("Exit by SIGTERM\n");

	// signal thread to stop
	exit_triggered = true;

	// 
	clean_up(); // pthread_join here

	//
	exit (EXIT_SUCCESS);
}

// register signal handlers 

void catch_signals() 
{
	// SIGINT, SIGTERM handler
	if (signal(SIGINT, exit_signal_handler) == SIG_ERR) {
		fprintf(stderr, "Cannot handle SIGINIT!\n");
		exit(-1);
	}

	if (signal(SIGTERM, exit_signal_handler) == SIG_ERR) {
		fprintf(stderr, "Cannot handle SIGTERM!\n");
		exit(-1);
	}
}

/* Socket Utilities */

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

bool get_address_infos (struct addrinfo **infos)
{
	struct addrinfo hints;
	int rv;

	// reset hints
	memset(&hints, 0, sizeof(struct addrinfo));

	// get addresses for binding a socket to and accept connections
	//   node == NULL, hits.ai_flags == AI_PASSIVE
    // if node != NULL, AI_PASSIVE is ignored

	hints.ai_family   = AF_UNSPEC;   // any of IF_INET, AF_INET6, etc
	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_flags    = AI_PASSIVE;  // use my ip

    // lookup and make binary address structure   
    rv = getaddrinfo(NULL, LISTEN_PORT, &hints, infos);

	if (rv != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return false;
	} else {
		return true;
	}
}

bool bind_socket(int* fdp) {

	struct addrinfo *infos, *info;
	int yes = 1;
	int fd = -1;

    // get address infos
    if(!get_address_infos(&infos)) return false;

	// loop through all the results and bind to the first we can

	for(info = infos; info != NULL; info = info->ai_next) {

		// open socket 
		fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);

		if (fd == -1) {
			perror("server: socket");
			continue;
		}

		// set socket behaviour
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("setsockopt");
			exit(-1);
		} 

		// bind
		if (bind(fd, info->ai_addr, info->ai_addrlen) == -1) { 
			close(fd); 
			perror("server: bind"); 
			continue; 
		} 

		(*fdp) = fd;

		break; 
	}

	freeaddrinfo(infos); // all done with this structure 

	if (info == NULL) 
		return false;
	else 
		return true;
}


bool save_to_file(char* packet, int size) {
	int rc;
	struct aesd_seekto cmd_arg;
	ssize_t nr;
	int route;

    if (frw == -1) { 
#ifdef USE_AESD_CHAR_DEVICE
		frw = open(SAVE_FILE, O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
#else
		frw = open(SAVE_FILE, O_CREAT | O_RDWR | O_APPEND, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
	}

	if (frw == -1) {
		perror("open");
	}

	printf("<< %s\n", packet);

	if (strncmp(packet, "AESDCHAR_IOCSEEKTO:", 19) == 0) {
		route = 0;

		// driver to ioctl
		char *not_used = strtok(packet, ":");

		printf("***IOCTL SEEK < %s\n", not_used);

		cmd_arg.write_cmd = atoi(strtok(NULL, ","));
		cmd_arg.write_cmd_offset = atoi(strtok(NULL, ","));

		rc = ioctl(frw, AESDCHAR_IOCSEEKTO, &cmd_arg);
	} else {
		route = 1;
		nr = write(frw, packet, size);
		lseek(frw, 0, SEEK_SET);
	}

	//close(frw);

	//frw = -1; // reset -- for signal handler

	if (route == 0 && rc < 0) {
		perror("ioctl");
		return false;
	}
	else if (route ==1 && nr == -1) {
		perror("write");
		return false;
	}
	return true;
}

// listening module

void accept_loop(int fd_server)
{
    int fd_client, rc;
	struct sockaddr_storage client_addr; 
	socklen_t sin_size = sizeof(struct sockaddr_storage);;
	char s[INET6_ADDRSTRLEN];

	pthread_t thread_id;
	struct slist_data_s *datap = NULL;


#ifdef USE_AESD_CHAR_DEVICE
	printf("server 2.0: waiting for connections (save to dev I/O)...\n");
#else
	printf("server: waiting for connections (save to tmp file)...\n");
#endif

	while(!exit_triggered) {  
		// accept a connection
		fd_client = accept(fd_server, (struct sockaddr *)&client_addr, &sin_size); 

		if (fd_client == -1) { 
			// can be caused by alarm (interval timer which interrupt 'accept'
			continue;
		} 

		printf("new connection: %d\n", fd_client);

		// address
		inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s); 

		// create linked list item before creating a thread
		datap = malloc(sizeof(struct slist_data_s));
		datap->addr = s;
		datap->fd_client = fd_client;
		datap->thread_complete = false;
		datap->thread_joined = false;
		
		// create thread
		rc = pthread_create(&thread_id, NULL, session_handler, (void *) datap);

		if (rc < 0) {
			perror("Could not create thread");
			free(datap); // release the allocatied memory
			break; // someting wrong -> exit
		}

		// log
		printf("server: got connection from %s\n", s); 
		syslog(LOG_DEBUG, "Accepted connection from %s", s);

		// set thread ID
		datap->thread_id = thread_id;

		// insert datap to the linked list
		SLIST_INSERT_HEAD(&head, datap, entries); // set the new item as the first

		// join if thread is complete but not joined yet
		// The allocation is not deleted here as SLIT_FOREACH_SAFE is not defined
		// deleting the list item here might not be safe

		SLIST_FOREACH(datap, &head, entries) {
			if (!datap->thread_joined && datap->thread_complete) {
			    pthread_join(datap->thread_id, NULL);
				datap->thread_joined = true;
			}
		}
	} 

	//
	clean_up();
}

void clean_up() 
{
	struct slist_data_s *datap = NULL;

	// release memory allocations 
	while(!SLIST_EMPTY(&head)) { // (&head)->slh_first == NULL
		// list item
		datap = SLIST_FIRST(&head);  
		// make sure it's the thread was joined
		if(!datap->thread_joined) {
			pthread_join(datap->thread_id, NULL);
			datap->thread_joined = true;
		}
		// remote from the linked list
		SLIST_REMOVE_HEAD(&head, entries); 
		// free the allocation
		free(datap);
	}
}

void* session_handler(void* dp)
{
	char packet_buf[MAX_PACKET_BUF], recv_buf[MAX_BUF];
	int n_packet = 0, n_recv;
	int i, pos_newline, rc, n_read;
	int fd_client = -1;

	struct slist_data_s *datap = (struct slist_data_s *) dp;
	fd_client = datap->fd_client;

	while(!exit_triggered) {
		n_recv = recv(fd_client, recv_buf, MAX_BUF, 0);

		// socket closed
		if (n_recv == 0) {
			printf("server: closed connection from %s\n", datap->addr); 
			syslog(LOG_DEBUG, "Closed connection from %s", datap->addr);
			break;
		}

		// check buffer full
		if (n_packet + n_recv > MAX_PACKET_BUF) {
			printf("Buffer full. Packet is discarded\n");
			n_packet = 0;
			continue;
		} 

		// copy to master buffer
		memcpy(packet_buf + n_packet, recv_buf, n_recv);
		n_packet += n_recv;

		printf("%d characters received, total = %d\n", n_recv, n_packet);

		// check packet ending
		pos_newline = -1;

		for(i = 0; i < n_packet; i++) {

		    //printf("[%d]%c", i, packet_buf[i]);

			if (packet_buf[i] == '\n') {
				pos_newline = i;
				break;
			}
		}

		// packet completed
		if(pos_newline >= 0) {
			// save
			pthread_mutex_lock(&lock); // protect critical section
			save_to_file(packet_buf, pos_newline+1); // include newline
			pthread_mutex_unlock(&lock); // release mutex

			if (n_packet > pos_newline+1) {
				memcpy(packet_buf, packet_buf + pos_newline+1, n_packet - pos_newline - 1);
				n_packet = n_packet - pos_newline - 1;
			} else {
				n_packet = 0;
			}

			// feedback
			pthread_mutex_lock(&lock);

			if (frw == -1) {
				frw = open(SAVE_FILE, O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
			    // fr = open(SAVE_FILE, O_RDONLY);
			}

			//if (fr== -1) {
			if (frw == -1) {
			    perror("open - rd");
				exit(-1);
			}
				
			while(1) {
				//n_read = read(fr, recv_buf, MAX_BUF);
				n_read = read(frw, recv_buf, MAX_BUF);

				if (n_read == -1) {
					perror("read");
					break;
				}

				if (n_read > 0) {
					rc = send(fd_client, recv_buf, n_read, 0);
					if (rc == -1) {
						perror("send");
						break;
					}
				} else {
					break;
				}
			}
			//close(fr);

			//fr = -1; // for signal handler

			pthread_mutex_unlock(&lock);
		} // if
	} // while

	datap->thread_complete = true;

	close(fd_client);  // parent doesn't need this 

	return NULL;
}

/* Timer */

void timer_proc (int signum) 
{
	time_t timer;
	char buf[100];
	struct tm* tm_info;

	timer = time(NULL);
	tm_info = localtime(&timer);

	sprintf(buf, "timestamp:");
	//strftime(buf, 30,"%a, %d %b %Y %T %z", tm_info);
	strftime(buf+10, 30,"%d %b %Y %T", tm_info);
	strcat(buf, "\n");
	//printf("%s", buf);

	pthread_mutex_lock(&lock); // protect critical section
	save_to_file(buf, strlen(buf)); 
	pthread_mutex_unlock(&lock); // release mutex
}

int main(int argc, char *argv[])
{
	int fd;
	pid_t pid;
	//struct itimerval itv;
	//struct sigaction sa;

	// syslog
	openlog("Assignment9", LOG_NDELAY, LOG_USER);

	// bind socket
	if(!bind_socket(&fd)) {
		fprintf(stderr, "server: failed to bind\n"); 
		exit(-1); 
	}

	// make it a daemon
	if (argc == 2 && strcmp(argv[1], "-d") == 0) {

		printf("Daemon mode\n");

		pid = fork();

		if (pid < 0) {
			// parent exit with failure
			perror("fork");
			exit(EXIT_FAILURE);
		}
		if (pid > 0) {
			// kill the parent
			exit(EXIT_SUCCESS);
		}
		// pid == 0, child process continues to run
	}

	// listen
	if (listen(fd, BACKLOG) == -1) { 
		perror("listen"); 
		exit(-1); 
	} 

	// init linked list
	SLIST_INIT(&head); // head points to NULL

	/*
	// timer handler
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &timer_proc;
	sigaction(SIGALRM, &sa, NULL); // for ITIMER_REAL

	// timer setting -- ever 10 seconds do something
	itv.it_value.tv_sec = 10;
	itv.it_value.tv_usec = 0;
	itv.it_interval.tv_sec = 10;
	itv.it_interval.tv_usec = 0;
	*/

	//setitimer(ITIMER_REAL, &itv, NULL);

	//
	catch_signals();

	// accept loop
	accept_loop(fd);

	return 0;
}