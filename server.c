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
#include <errno.h>
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
	
	FILE * fpr;
	fpr = fopen(argv[1], "r");
	if (fpr == NULL) {
		printfTime();
		printf("lyrebird.server: PID %d failed to load due to invalid config file path, EXITING.\n", getpid());
		exit(-1);
	}
	FILE * fpw;
	fpw = fopen(argv[2], "w");
	if (fpw == NULL) {
		printfTime();
		printf("lyrebird.server: PID %d failed to load due to invalid log file name, EXITING.\n", getpid());
		exit(-1);
	}
	
	int sockfd, newfd; 
	struct sockaddr_in serv_addr, c_addr, *sa; //Our serv_addr = Network Info, c_addr = Client Info
	socklen_t c_size, serv_size;
	struct ifaddrs *ifaddr, *ifa; //Create linked lists that will hold addresses w/ ifa being a pointer
	char * addr; //This will hold our IPv4 Address
  int yes = 1;

  //Variables for Select()
  fd_set master; //Master file descriptor list
  fd_set read_fds; //Temporary file descriptor for select()
  int fdmax; //max file descriptor number

	if (getifaddrs(&ifaddr) == -1) {
		printfTime();
		printf("lyrebird.server: PID %d failed to obtain addresses information, EXITING.\n", getpid());
		exit(-1);
	}	

	memset(&serv_addr, '0', sizeof(serv_addr));
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printfTime();
		printf("lyrebird.server: PID %d failed to socket, EXITING.\n", getpid());
		exit(-1);
	}
	
	//The last address of the linked list is the one we will use
	for (ifa = ifaddr; ifa; ifa = ifa -> ifa_next) { //This will help us obtain our IPv4 Address
		if (ifa -> ifa_addr->sa_family == AF_INET) {
			sa = (struct sockaddr_in *) ifa-> ifa_addr;
			addr = inet_ntoa(sa->sin_addr);
		}
	}
	freeifaddrs(ifaddr);//We are done using the linked list

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(addr); //IP Address of the host
	serv_addr.sin_port = 0;
	memset(serv_addr.sin_zero, '\0', sizeof(serv_addr.sin_zero)); //Set bits on the padding field to zero

  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  
	if ((bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {
		printfTime();
		printf("lyrebird.server: PID %d failed to bind.\n", getpid());
		exit(-1);
	}

	listen(sockfd, 10);
	serv_size = sizeof(serv_addr);
	if (getsockname(sockfd, (struct sockaddr *) &serv_addr, &serv_size) == -1) {
		printfTime();
		printf("lyrebird.server: PID %d failed to getsockname.\n", getpid());
		exit(-1);
	}
	printfTime();
	printf("lyrebird.server: PID %d on host %s, port %d\n",  getpid(), inet_ntoa(serv_addr.sin_addr), (int) ntohs(serv_addr.sin_port));

	int f = 0;
  int c = fgetc(fpr);
  int val = -1; //Will be used to hold the PID of the clients
  int stat = -1; //Is a status bit to know the status of the clients
  int flag = 0;
  int open; //Checks if the other end of the socket is closed
  int i; //index of fd
  int finish = 0; //Checks for the number of clients connected to us
  int eof_sig = 1; //A status, if 0 then we are done sending out filenames
  int loop = 1;

  char buffer[1024+1]; //If a client is done decrypting we want to store which one it is
  char fname[2049+1];
  char * frname = (char *)(malloc(sizeof(char)*1024+1));
  char * c_id;
  
  memset(fname, 0, sizeof(fname));
  memset(frname, 0, sizeof(frname));
  memset(buffer, 0, sizeof(buffer));

  FD_SET(sockfd, &master);
  fdmax = sockfd; //Should be the largest decriptor

  while (loop){
    if (finish == 0 && eof_sig == 0){ //If no one is connected to us and we are done sending information we are done
      break;
    }

    read_fds = master;
    if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
      printfTime();
      printf("lyrebird.server: PID %d on host %s, port %d, exiting due to select failure.\n",  getpid(), inet_ntoa(serv_addr.sin_addr), (int) ntohs(serv_addr.sin_port));
      exit(-1);
    }

    for (i = 0; i <= fdmax; i++) {

      if (FD_ISSET(i, &read_fds)){
        if (i == sockfd) { //We found a new connection
          c_size = sizeof c_addr;
          newfd = accept(sockfd, (struct sockaddr *) &c_addr, &c_size);

          FD_SET(newfd, &master); //Added to fd_set
          if (newfd > fdmax) { //Incase we need to increase the maximum
            fdmax = newfd;
          }
          writeTime(fpw); 
          fprintf(fpw,"Successfully connected to lyrebird client %s.\n", inet_ntoa(c_addr.sin_addr));
          finish = finish + 1;
        }
        else { //Data from client
          c_size = sizeof c_addr;
          getpeername(i, (struct sockaddr *) &c_addr, &c_size);
          c_id = inet_ntoa(c_addr.sin_addr);
          open = read(i, &stat, sizeof(stat));
          if (open <=  0 || stat == 4){ //If stat = 4, then the client is done and exited expectedly
            //In any other situation, the client closes its socket without sending stat we encountered an unexpected disconnect
            if (stat != 4 || open <= 0) {
              stat = 5;
            }
            disconPrint(stat, c_id, fpw);
            stat = 0;
            close(i); //Close Client Socket
            FD_CLR(i, &master);
            finish = finish - 1;
            if (finish == 0 && eof_sig == 0){ //We are done decrypting and all clients exited
              break;
            }
          }
          else { //Got data
            if (c != EOF) { //If we still have data to send to clients
              while ( c != EOF && c != 10) {
                if (c == 32) { flag = 1; frname[f] = '\0'; }
                if (flag == 0) { frname[f] = c; }
                if (c != 13) { fname[f] = c; f++;}
                c = fgetc(fpr);
              }
              flag = 0;
              fname[f] = '\0';
              c_id = inet_ntoa(c_addr.sin_addr);
               
              read(i, &val, sizeof(val));
              read(i, buffer, sizeof(buffer));
    
              statusPrint(stat, c_id, buffer, val, fpw);
              stat = 0;
    
              write(i, fname, strlen(fname)+1);     
              writeTime(fpw);
              fprintf(fpw, "The lyrebird client %s has been given the task of decrypting %s.\n",  inet_ntoa(c_addr.sin_addr), frname);
              memset(fname, 0, sizeof(fname));
              memset(frname, 0, sizeof(frname));
              memset(buffer, 0, sizeof(buffer));
              c = fgetc(fpr);
              f = 0;
            }
            else { //Notify the clients we are finished
              eof_sig = 0;

              fname[0] = '~'; fname[1] = '\0';
              c_id = inet_ntoa(c_addr.sin_addr);

              read(i, &val, sizeof(val));
              read(i, buffer, sizeof(buffer));
              write(i, fname, strlen(fname)+1);
              statusPrint(stat, c_id, buffer, val, fpw);
              stat = 0;
            }
          }
        }
      }
    }
  }
  free(frname);

	fclose(fpw);
	fclose(fpr);
	close(newfd);
	close(sockfd); //Close connection 

	printfTime();
	printf("lyrebird.server: PID %d completed its tasks and is exiting successfully.\n", getpid());
	return;
}