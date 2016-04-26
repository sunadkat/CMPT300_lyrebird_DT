/*
Name: David Tran
Student Number: 301223841
SFU User Name: dta31
Lecture Section: CMPT 300: D100
Instructor: Brian G. Booth
TA: Scott Kristjanson
*/

#include "decrypt.h"
#include "memwatch.h"

void disconPrint(int stat, char * c_id, FILE * fpw) {
  if (stat == 4 || stat == 5) {
    writeTime(fpw);
  }
  if (stat == 4) {
    fprintf(fpw, "The lyrebird client %s has disconnected expectedly.\n",  c_id);
  }
  else if (stat == 5) {
    fprintf(fpw, "The lyrebird client %s has disconnected unexpectedly.\n",  c_id);
  }
}

void statusPrint(int stat, char * c_id, char buffer[], int val, FILE * fpw) {
  if (stat > 0 && stat < 4) {
    writeTime(fpw);
  }
  if (stat == 1) {
    fprintf(fpw, "The lyrebird client %s has successfully decrypted %s in process %d.\n",  c_id, buffer, val);
  }
  else if (stat == 2) {
    fprintf(fpw, "The lyrebird client %s has encountered an error: Unable to open file %s in process %d.\n",  c_id, buffer, val);
  }
  else if (stat == 3) {
    fprintf(fpw, "The lyrebird client %s has encountered an error: Unable to write to file %s in process %d.\n",  c_id, buffer, val);
  }
}

int childProcess(int i, int numcore, int readfd[][2], int writfd[][2]){ //Tasks the Child runs
  char fname[2049+1]; //Use this as a buffer to hold the info the parent sends
  char frname[1024+1];
  frname[0] = '\0';
  int r = 0;
  memset(fname, 0, sizeof(fname));
  int j;
  
  for (j = 0; j < numcore; j++){ //Closing the unused end of the pipes
    close(writfd[j][1]);
    close(readfd[j][0]);
    if (j != i) {
      close(writfd[j][0]);
      close(readfd[j][1]);
    }
  }
  int stat = 0; //Status of the mission
  int val = getpid(); //We send the PID to the parent
  write(readfd[i][1], &stat, sizeof(stat));
  write(readfd[i][1], &val, sizeof(val));
  write(readfd[i][1], frname, 1024+1);
  //Open checks the status of the read (if 0 or negative, we exit)
  int open = read(writfd[i][0], fname, sizeof(fname));
  while (open > 0) {

    while (fname[r] != 32 && fname[r] != '\0') {
      frname[r] = fname[r];
      r++;
    }
    frname[r] = '\0';
    r = 0;

    stat = decryptEncryptedFile(fname);
    memset(fname, 0, sizeof(fname));

    write(readfd[i][1], &stat, sizeof(stat));
    write(readfd[i][1], &val, sizeof(val)); //Child is ready again
    write(readfd[i][1], frname, 1024+1);
    
    open = read(writfd[i][0], fname, sizeof(fname));
  }
  close(writfd[i][0]);
  close(readfd[i][1]);
  return open; //0 on success, otherwise it is a failure
}

void parentProcess(int sockfd, int numcore, int readfd[][2], int writfd[][2]){ //Tasks the Parent runs
  //The message we will send to the child
  char fname[2049+1];
  memset(fname, 0, sizeof(fname));
  //This will hold the filename/location child will read from
  char frname[1024+1];
  memset(frname, 0, sizeof(frname));

  int val = 0; //Will hold the pid of child (to send to server)
  int stat = 0; //This will be used as a status flag to send to the server
  int j; //Will be used as a secondary index next to i, incase we need both of them

  for (j = 0; j < numcore; j++){ //Close unused sides of pipes
    close(writfd[j][0]);
    close(readfd[j][1]);
  }

  //Setting variables for select (set is the set of file descriptors that are ready)
  fd_set set;
  int sel;
  //To make sure only one child gets the line to decrypt (for FCFS)
  int done = 0;

  //Initialize dummy values to allow our process to check if any children are ready
  frname[0] = '~';
  fname[0] = 'a';
  int open = 1; //This will hold the value is the socket is open or closed
  while ((strcmp(fname, "~") != 0) && open > 0){ //'~' is our EXIT message

    FD_ZERO(&set);
    for (j = 0; j < numcore; j++) {
      FD_SET(readfd[j][0], &set);
    }
    sel = select(FD_SETSIZE, &set, NULL, NULL, NULL);

    if (sel) {
      for (j = 0; j < numcore; j++) {
        if (FD_ISSET(readfd[j][0], &set) && done == 0){
          
          write(sockfd, &stat, sizeof(stat));
          write(sockfd, &val, sizeof(val));
          write(sockfd, frname, strlen(frname)+1);
          open = read(sockfd, fname, sizeof(fname));
          if (strcmp(fname, "~") != 0) {//If it is our exit message we leave

            read(readfd[j][0], &stat, sizeof(stat));
            read(readfd[j][0], &val, sizeof(val));
            read(readfd[j][0], &frname, sizeof(frname));

            write(writfd[j][1], fname, 2049+1);
          }
          done = 1;
        }
      }
    }
    done = 0;
  }
  
  for (j = 0; j < numcore; j++) { //Read all leftover child data
    read(readfd[j][0], &stat, sizeof(stat));
    read(readfd[j][0], &val, sizeof(val));
    read(readfd[j][0], &frname, sizeof(frname));
    
    if (stat > 0 && open > 0) {
      write(sockfd, &stat, sizeof(stat));
      write(sockfd, &val, sizeof(val));
      write(sockfd, frname, strlen(frname)+1);

      //The value on fname should be irrelevant since the server is done sending actual filenames
      //Done so that client does not put too many messages in the socket
      open = read(sockfd, fname, sizeof(fname));
    }
  }
  
  for (j = 0; j < numcore; j++) { //Parent closes rest of Pipes
    close(writfd[j][1]);
    close(readfd[j][0]);
  }
}

void waitChild(int numcore){ //Waits for children to finish
  int status;
  pid_t pid;
  while (numcore > 0) { //Parent waits for ALL children to finish
    pid = wait(&status);
    if (status != 0) { //If it is a status other than 0 it is a failed process
      printfTime();
      printf("lyrebird.client: child process ID #%d did not terminate successfully.\n", pid);
    }
    numcore--;
  }
}

//Writes out the time on file
void writeTime(FILE * fpw){
  time_t t;
  time(&t);
  char * tim = ctime(&t);
  tim[24] = '\0';
  fprintf(fpw, "[%s] ", tim);
}

//Prints out the time
void printfTime(){
  time_t t;
  time(&t);
  char * tim = ctime(&t);
  tim[24] = '\0';
  printf("[%s] ", tim);
}

//Performs steps 1-4 of decryption
//Returns 0 on success and -1 on failure
int decryptEncryptedFile(char fname[]){
  //Here we take the given a line, this splits into a read (frname) and write (fwname) name
  char * frname = (char *)(malloc(sizeof(char)*1024+1));
  char * fwname = (char *)(malloc(sizeof(char)*1024+1));
  memset(frname, 0, sizeof(frname));
  int f = 0; int r = 0; int w = 0; int flag = 0;
  while (fname[f] != '\0'){
    if (flag == 0){
      if (fname[f] != 32){
        frname[r] = fname[f];
        r++; f++;
      }
      else {flag = 1; frname[r] = '\0'; f++;}
    }
    else {
      fwname[w] = fname[f];
      w++; f++;
    }
  }
  fwname[w] = '\0';

  //fpr = file pointer for reading
  FILE *fpr;
  fpr = fopen(frname, "r");
  //Check for incorrect file name (read)
  if (fpr == NULL) {
    free(frname);
    free(fwname);
    return 2;
  }
  //fpw = file pointer for writing
  FILE *fpw;
  fpw = fopen(fwname, "w");
  //Check for incorrect file name (write)
  if (fpw == NULL) {
    free(frname);
    free(fwname);
    fclose(fpr);
    return 3;
  }

  //INITIALIZING SOME SPACE/VARIABLES
  //Holds up to 1 line of tweets (Can be modified to hold more)
  char * ptweet = (char *)malloc(sizeof(char)*175+1);
  //Holds the 6 indices that will be used in calculation from base 41 -> decimal
  int * psix = (int *)malloc(sizeof(int)*6);
  //Holds decimal values from ptweet
  long long * pconvert = (long long *)malloc(sizeof(long long)*30);
  //Holds the final characters to be printed
  char * pfinalchar = (char *)malloc(sizeof(char)*145+1);
  //Checks next character
  int c = fgetc(fpr);
  //Used for counting tweets stored in ptweet
  int linenum = 0;
  //Index and also is the length of the characters in ptweet
  int i = 0;
  //j will be used to store the number of values in pconvert, k will store for pfinalchar
  int j, k;

  //Used read the tweets and perform the encryption tasks for each line
  while (c != EOF){
    while (c != EOF && linenum != 1){ //Holds 1 tweets (Can modify to hold more)
      if (c == 10) {
        linenum++;
      }
      if (c != 13) { // For rare cases when a text editor adds ASCII value 13 to end of line
        ptweet[i] = c;
        i++;
      }
      c = fgetc(fpr);
    }
    remove_eight(ptweet, i); //Step 1: Removes the 8th character
    j = count_six(ptweet, i); //To find how many values are stored in pconvert
    k = (j*6) + linenum; //This is how many chars will be stored in pfinalchar
    j = j + linenum; //Also need to include newlines
    convert_todecimal(ptweet, psix, pconvert, j); //Step 2: Gets chars in groups of 6 and converts them
    convert_modexp(pconvert, j); //Step 3: Convert using the formula M=C^d modn on all values in pconvert
    convert_tobase(pfinalchar, pconvert, j); //Step 4: Inverse of step 2, converting back to base 41
    fwrite(pfinalchar, sizeof(char), k, fpw); //Writes answer to file
    i = 0;
    linenum = 0;
  }
  //Free and close all functions
  free(pfinalchar);
  free(pconvert);
  free(psix);
  free(ptweet);
  fclose(fpw);
  fclose(fpr);
  free(frname);
  free(fwname);
  return 1;
}

//Step 1: Replaces 8th character with 'A' since it is unused
void remove_eight(char * ptweet, int len){
  int i = 0;
  //"count" checks for when the function reaches the 8th character
  int count = 1;
  while (i < len){
    if (ptweet[i] == 10){ //Resets the counter when there is a new line/tweet
      count = 1;
      i++;
    }
    else {
      if (count%8 == 0){ //When it reaches the 8th character replace with 'A'
        ptweet[i] = 'A';
        count = 0;
      }
      count++;
      i++;
    }
  }
}
//Counts the groups of 6 characters in the array that holds the chars (ptweet)
int count_six(char * ptweet, int len) {
  //Counter, counts every valid character
  int check = 0;
  int i;
  for (i = 0; i <len; i++) {
    if (ptweet[i] != 10 && ptweet[i] != 65) {//As long as it is not '/n' or 'A' we add to counter
      check++;
    }
  }
  //Divide by 6 since a valid input is always a multiple of 6
  return (check/6);
}
//Step 2: Converting sets of 6 chars to decimal values
void convert_todecimal(char * ptweet, int * psix, long long * pconvert, int len){
  int i;
  int j = 0;
  //Counter for sets of 6
  int count = 0;
  for (i = 0; i < len; i++) { //Loops for the entire char array (ptweet)
    if (ptweet[j] != 10 && ptweet[j] != 65){
      while (count != 6) { //Used to store indices of 6 valid chars (in psix)
        if (ptweet[j] != 10 && ptweet[j] != 65){
          psix[count] = j;
          count++;
        }
        j++;
      }
      //Perfoms conversion to set of 6 valid chars
      pconvert[i] = base_todecimal(ptweet, psix);
    }
    //Inserts the newline
    else if (ptweet[j] == 10) {
      j = j + 1;
      pconvert[i] = -1;
    }
    //Skips the 8th character
    else if (ptweet[j] == 65) {
      j++;
      i--;
    }
    count = 0;
  }
}
//Associated with function base_todecimal() to perform conversion to decimal
//Utilizes comparison to ASCII
long long base_todecimal(char * ptweet, int * psix){
  const int base = 41;
  long long total = 0;
  int i, k;
  for (i = 0; i < 6; i++){
    k = ptweet[psix[i]];
    if (k >= 97 && k <= 122){ //If character is between a-z (1-26)
      total = total + ((long long)(k) - 96) * pow(base, 5-i);
    }
    else if (k == 32){ //If character is space (0)
      total = total + 0;
    }
    else if (k == 35){ //If character is # (27)
      total = total + 27 * pow(base, 5-i);
    }
    else if (k == 46){ //If character is . (28)
      total = total + 28 * pow(base, 5-i);
    }
    else if (k == 44){ //If character is , (29)
      total = total + 29 * pow(base, 5-i);
    }
    else if (k == 39){ //If character is ' (30)
      total = total + 30 * pow(base, 5-i);
    }
    else if (k == 33){ //If character is ! (31)
      total = total + 31 * pow(base, 5-i);
    }
    else if (k == 63){ //If character is ? (32)
      total = total + 32 * pow(base, 5-i);
    }
    else if (k == 40 || k == 41){ //If character is () (33, 34)
      total = total + ((long long)(k) - 7) * pow(base, 5-i);
    }
    else if (k == 45){ //If character is - (35)
      total = total + 35 * pow(base, 5-i);
    }
    else if (k == 58){ //If character is : (36)
      total = total + 36 * pow(base, 5-i);
    }
    else if (k == 36){ //If character is $ (37)
      total = total + 37 * pow(base, 5-i);
    }
    else if (k == 47){ //If character is / (38)
      total = total + 38 * pow(base, 5-i);
    }
    else if (k == 38){ //If character is & (39)
      total = total + 39 * pow(base, 5-i);
    }
    else if (k == 92){ //If character is \ (40)
      total = total + 40 * pow(base, 5-i);
    }
    else {
      return (-1);
    }
  }
  return total;
}
//Step 3: Convert all given values using M=C^d modn
void convert_modexp(long long * pconvert, int len) {
  //d and m are the given constants
  const long long d = 1921821779;
  const long long m = 4294434817;
  //Will hold the final value
  long long c;
  int i;
  for (i = 0; i < len; i++) {
    if (pconvert[i] != -1) {
      c = pconvert[i];
      pconvert[i] = mod_exp(c,d,m);
    }
  }
}
//Used to perform quick modular exponentiation
long long mod_exp(long long c,long long d,long long m){
    unsigned long long r = 1;
    unsigned long long mod = c % m;
    while(d>0){
        if(d%2 == 1) {
            r = (r*mod)%m;
        }
        mod = (mod*mod)%m;
        d = d/2;
    }
    long long ans = r;
    return ans;
}
//Step 4: Converts all values to base 41
void convert_tobase(char * pfinalchar, long long * pconvert, int len){
  const int base = 41;
  int i;
  int j = 0;
  //k = newline character
  char k = 10;
  long long l;
  for (i = 0; i < len; i++) {
    if (pconvert[i] == -1) { //Adds a newline
      pfinalchar[j] = k;
      j++;
    }
    else {
      l = pconvert[i];
      decimal_tobase(pfinalchar, l, j);
      j = j + 6; //Increase index by 6 as 6 values were stored during function
    }
  }
}
//Associated with convert_tobase(), performs actions in groups of 6
void decimal_tobase(char * pfinalchar, long long l, int j){
  const long long base = 41;
  int i;
  int k_val;
  long long div_val = l;
  long long mod;
  char k;
  for (i = 0; i < 6; i++) { //Divides 6 times to obtain value for 6 chars
    mod = mod_exp(div_val, 1, base);
    k_val = decrypt(mod);
    k = k_val;
    pfinalchar[j+5-i] = k;
    div_val = div_val/base;
  }
}
//Associated with decimal_tobase(), performs conversion of decimal to base 41
int decrypt(long long l){
  int asc; // ASCII value
  if (l >= 1 && l <= 26){
    asc = (int)l + 96;}
  else if (l == 0){
    asc = 32;}
  else if (l == 27){
    asc = 35;}
  else if (l == 28){
    asc = 46;}
  else if (l == 29){
    asc = 44;}
  else if (l == 30){
    asc = 39;}
  else if (l == 31){
    asc = 33;}
  else if (l == 32){
    asc = 63;}
  else if (l == 33 || l == 34){
    asc = (int)l + 7;}
  else if (l == 35){
    asc = 45;}
  else if (l == 36){
    asc = 58;}
  else if (l == 37){
    asc = 36;}
  else if (l == 38){
    asc = 47;}
  else if (l == 39){
    asc = 38;}
  else {
    asc = 92;}
  return asc;
}
