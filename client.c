/*
Name: David Tran
Student Number: 301223841
SFU User Name: dta31
Lecture Section: CMPT 300: D100
Instructor: Brian G. Booth
TA: Scott Kristjanson
*/
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "decrypt.h"
#include "memwatch.h"
main (int argc, char **argv) {

	int sockfd, portnum;
	struct sockaddr_in serv_addr, *sa;
	char * addr; //This will hold our IPv4 Address

	portnum = atoi(argv[2]); //Convert port number (string -> int)

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		printfTime();
		printf("lyrebird.client: PID %d failed to socket.\n", getpid());
		exit(-1);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portnum); //Assign port number
	if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) { //Assign IP number
		printfTime();
		printf("lyrebird.client: PID %d failed due to invalid input IP.\n", getpid());
		exit(-1);
	}
	memset(serv_addr.sin_zero, '\0', sizeof(serv_addr.sin_zero)); //Set bits on the padding field to zero

	if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		printfTime();
		printf("lyrebird.client: PID %d failed to connect.\n", getpid());
		exit(-1);
	}

	printfTime();
	printf("lyrebird.client: PID %d connected to server %s on port %d.\n",  getpid(), inet_ntoa(serv_addr.sin_addr), (int) ntohs(serv_addr.sin_port));
	
	char fname[2049+1];
	memset(fname, 0, sizeof(fname));

	//Number of cores to create
	int numcore = sysconf(_SC_NPROCESSORS_CONF)-1;

	//Pipe setup
  int readfd[numcore][2];
  int writfd[numcore][2];
  int i;
  for (i = 0; i < numcore; i++){
    if (pipe(readfd[i]) < 0 || pipe(writfd[i]) < 0) { //Check if piping failed
      printfTime();
      printf("lyrebird.client: PID %d disconnecting from server %s due to pipe failure.\n", getpid(), inet_ntoa(serv_addr.sin_addr));
      close(sockfd);
      exit(-1);
    }
  }


  pid_t pids; //This will be the what we use to fork (Also Pid of children in Parent)
  i = 0; //The index of the child (for readfd/writfd[i])
  while (i < numcore) { //Create children equal to total # of cores -1
    pids = fork();
    if (pids <= 0){ break; } //Stops child from forking more children
    i++;
  }

  if (pids < 0) { //Fork failed and Child not created
    printfTime();
    printf("lyrebird.client: PID %d disconnecting from server %s due to fork failure.\n", getpid(), inet_ntoa(serv_addr.sin_addr));
    close(sockfd);
    exit(-1);
  }

  if (pids == 0){ //Child Process
  	close(sockfd); //Child should not have communication with Server
  	int open = childProcess(i, numcore, readfd, writfd);
  	if (open < 0){ exit(-1); } //Child had error on read
  	exit(0);
  }

  else { //Parent Process
    parentProcess(sockfd, numcore, readfd, writfd);
  }
	waitChild(numcore);
	int stat = 4;
	write(sockfd, &stat, sizeof(stat)); //Notify the server that we are disconnecting
	close(sockfd); //Close connection
	printfTime();
  printf("lyrebird.client: PID %d completed its tasks and is exiting successfully.\n", getpid());
	return;
}