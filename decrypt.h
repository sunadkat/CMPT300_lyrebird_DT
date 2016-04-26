/*
Name: David Tran
Student Number: 301223841
SFU User Name: dta31
Lecture Section: CMPT 300: D100
Instructor: Brian G. Booth
TA: Scott Kristjanson
*/

#ifndef _DECRYPT_H_
#define _DECRYPT_H_

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <math.h>
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
#include "memwatch.h"

//Prints out the status based on status flag
void disconPrint(int stat, char * c_id, FILE * fpw);

//Prints out the status based on status flag
void statusPrint(int stat, char * c_id, char buffer[], int val, FILE * fpw);

//Runs the tasks the Child Process must do
int childProcess(int i, int numcore, int readfd[][2], int writfd[][2]);

//Runs the tasks the Parent Process must do
void parentProcess(int sockfd, int numcore, int readfd[][2], int writfd[][2]);

//Function that makes the Parent Process wait for the Children to finish
void waitChild(int numcore);

//Writes out time to file
void writeTime(FILE * fpw);

//Prints the time
void printfTime();

//Performs Steps 1-4 (For child process to run)
//Returns 1 on success and 2 or 3 on failure
int decryptEncryptedFile(char fname[]);

//Step 1: Replaces 8th character with 'A' since it is not used in the table
//Resets when there is a newline
void remove_eight(char * ptweet, int len);

//Returns the number of sets of 6 valid characters
//Also is the number of values that will be stored in pconvert (stores step 3 values)
int count_six(char * ptweet, int len);

//Step 2: Performs action of obtaining groups of 6 and converts to decimal
//Calls base_todecimal() for each group of six valid characters from array
//Stores returned values in a long long array
//If a newline ASCII is found, stores -1 instead
void convert_todecimal(char * ptweet, int * psix, long long * pconvert, int len);
//Returns the conversion of base 41 to decimal
long long base_todecimal(char * ptweet, int * psix);

//Step 3: Calls the mod_exp() for each value in array, returned value replaces old value
//Uses constants d = 1921821779, m = 4294434817
void convert_modexp(long long * pconvert, int len);
//Returns the formula M = c^d mod m
long long mod_exp(long long c,long long d,long long m);

//Step 4: Inverse of step 2, reverting back to base 41
//Calls decimal_tobase() for each valid value
//ONLY performs storing newline ASCII to char array if value in long long array = -1
void convert_tobase(char * pfinalchar, long long * pconvert, int len);
//Calls decrypt() for individual conversions
//Stores return value to char array
void decimal_tobase(char * pfinalchar, long long l, int j);
//Returns conversions from decimal to base 41
int decrypt(long long l);

#endif
