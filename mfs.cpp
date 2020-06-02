/*
Name: Johan Barradas
ID: 1001354711
*/
// The MIT License (MIT)
//
// Copyright (c) 2016, 2017 Trevor Bakker
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <ctype.h> //REMOVE IF NEEDED
#include <stdint.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>

std::vector<std::string> cd_token_separator(char * token) //separates tokens
{
  std::string str = token;
  //std::cout<< str <<std::endl;
  std::string buf;
  std::stringstream ss(str);
  std::vector<std::string> tokens; // Create vector to hold our words

    while (std::getline(ss,buf,'/'))
    {
      tokens.push_back(buf);
    }
    return tokens;
}
/********************************************************************/
char     BS_OEMName[8];
uint16_t BPB_BytsPerSec;
uint8_t  BPB_SecPerClus;
uint16_t BPB_RsvdSecCnt;
uint8_t  BPB_NumFATS;
uint16_t BPB_RootEntCnt;
char     BS_VolLab[11];
uint32_t BPB_FATSz32;
uint32_t RootClusAddress; //to store the root address of our filesystem.
uint32_t RootDirSectors = 0;
uint32_t FirstDataSector = 0;
uint32_t FirstSectorofCluster = 0;
/********************************************************************/
struct __attribute__((__packed__))DirectoryEntry{
  char     DIR_Name[11]; //short "filename"
  uint8_t  DIR_Attr;
  uint8_t  Unused1[8];
  uint16_t DIR_FirstClusterHigh;
  uint8_t  Unused2[4];
  uint16_t DIR_FirstClusterLow;
  uint32_t DIR_FileSize;
};

struct DirectoryEntry dir[16]; //directory entry visible for all functions
#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 10     // Mav shell only supports 10 arguments #9

#define TRUE 1
#define FALSE 0
pid_t parent;
/*
* Param: current sector number that points to a block of data
* returns: value of the address for that block of data
* Desc: finds the starting address of a block of data given the sector number corresponding to that data block
*/
int16_t NextLB( uint32_t sector, FILE* fp)
{
  uint32_t FATAddress = ( BPB_BytsPerSec * BPB_RsvdSecCnt ) + ( sector * 4);
  int16_t val;
  fseek(fp, FATAddress, SEEK_SET );
  fread( &val, 2, 1, fp);
  return val;
}

int LBAToOffset(int32_t sector)
{
  return ((sector - 2) * BPB_BytsPerSec) + (BPB_BytsPerSec*BPB_RsvdSecCnt) + (BPB_NumFATS*BPB_FATSz32*BPB_BytsPerSec);
}
void readDirectories(uint32_t ClusAddress,FILE* fp)
{
  fseek(fp, ClusAddress, SEEK_SET);
  int i;
  for(i = 0; i< 16; i++)
  {
    fread(&dir[i],sizeof(struct DirectoryEntry),1,fp );
    //storing the directory info in the ith place of array dir (1 means only read once)
  }
}
int searchDirectories(std::string token, int i) //
{
  char name[12]; //size 11
  memcpy(name, dir[i].DIR_Name,11);
  name[11] = '\0';
  std::string direct_name(name);
  std::string comp1, comp2;
  int x;
  for(x = 0; x < direct_name.length();x++)
  {
    direct_name[x] = std::tolower(direct_name[x]);
    if (!isspace(direct_name[x])) comp1.push_back(direct_name[x]);
  }
  //int per = 0; //dont accept more than 1 period
  for(x = 0; x < token.length();x++)
  {
    token[x] = std::tolower(token[x]);
    if (token[x] != '.')
    {
      comp2.push_back(token[x]);
      //per = 1;
    }
  }
  if(token == "..")
    comp2 = token;
  //printf("%s %s\n", token, name);
  if( comp1 == comp2)
    return 1; //return 1 if found
  return 0; //0 if not
}
void printDirectory() //shall we display ..?
{
  int i;
  for(i = 0;i< 16; i++)
  {
    if( (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20)
      && dir[i].DIR_Name[0]!= 0xffffffe5 && dir[i].DIR_Name[0]!= 0xffffff05 &&
      dir[i].DIR_Name[0]!= 0x00 && dir[i].DIR_Name[0]!= '.')
    {
      char name[12];
      memcpy(name, dir[i].DIR_Name,11);
      name[11] = '\0';
      printf("'%s' %d\n", name,LBAToOffset(dir[i].DIR_FirstClusterLow) );
    }
  }
}
//function checks for whitespace, returns 0 if the strlen == #of whitspace chars on string
int is_only_whitespace(char * string)
{
  int i;
  int empty_count = 0;
  for(i = 0; i<strlen(string);i++)
  {
    if (isspace(string[i]))
    {
      empty_count++;
    }
  }
  if(strlen(string) == empty_count)
  {
    return 0;
  }
  return 1;
}
static void handlesig( int sig ) //handle signals inputted by user
{
  if( sig == SIGINT && parent!=getpid())//check if signal does not occur on parent
  {
    //send a SIGINT signal to the current process id, not the shell
    kill(getpid(), SIGINT); //send sigint signal to kill child process
    //printf("%d\n",getpid());
  }
  else if( sig == SIGTSTP && parent !=getpid())//check if signal does not occur on parent
  {
    kill(getpid(), SIGTSTP); //send a SIGTSP to the current process id
  }
}
//#REQ14
void printpids(pid_t listp[]) //prints the list of pids from the listpids data structure
{
  int i;
  for(i=0; i < 15 && listp[i]!= 0; i++)//short circuit operator to not check listp[i] if index
  {
    printf("%d   :%d\n",i,listp[i]);
  }
}
void sendcontsig(char* Pid) //sends continue signal to paused process, have to specify pid
{
  int pid_to_cont = atoi(Pid);
  if (pid_to_cont!= 0)
  {
    kill(pid_to_cont, SIGCONT); //send continue signal
  }
}
void printhist(char** historylist) //prints history list of commands ran
{
  int i;
  for (i = 0; i < 15&& historylist[i]!= NULL; i++)
  {
    printf("%d: %s",i, historylist[i]); //chars on history include newline character
  }
}
/*                                 COLOR FOR SHELL                                       */
//---------------------------------------------------------------------------------------//
void cyan() //set printf color to cyan
{
  printf("\033[01;36m");
}
void green() //set printf color to green
{
  printf("\033[01;32m");
}
void yellow() //set printf color to yellow
{
  printf("\033[01;33m");
}
void reset() //reset printf color of buffer
{
  printf("\033[0m");
}
void red() //set printf color to red
{
  printf("\033[01;31m");
}
//---------------------------------------------------------------------------------------//
void openFileSystemFirst(){
  red();
  printf("Error: File system image must be opened first.\n\n");
  reset();
}
int main() // mavshell
{
  //FILE SYSTEM
  FILE * file = NULL;

  uint32_t CurClusAddress; //stores the current address
  char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
  char * historylist[15] = {0}; // list for history, initialize to NULL
  int status;
  parent = getpid(); //to avoid SIGTSP and SIGINT from performing on parent
  pid_t listp[15] = {0}; //list for pids
  int iterator = 0; //itearator for bookeeping of history
  int listi = 0; //iterator for bookeeping of listp
  //----------------------------------------------------------------------//
  struct sigaction act; //signal handler for SIGINT (ctrl-c) and SIGTSTP (ctrl-z)
  memset (&act, '\0', sizeof(act)); //zero out the sigaction struct
  act.sa_handler = &handlesig; //set the hanlder to use handlesignal
  if(sigaction(SIGINT, &act, NULL) < 0) //install handler
  {
    perror("sigaction: "); //check return value and throw failure error
    return 1;
  }
  if(sigaction(SIGTSTP, &act, NULL) < 0) //install handler
  {
    perror("sigaction: "); //check return value and throw failure error
    return 1;
  }
  //----------------------------------------------------------------------//
  while( TRUE )
  {
    // Print out the msh prompt
    cyan(); //switch mav shell color to blue
    printf ("mfs> ");
    reset(); //switch shell color back to default
    // Read the command from the commandline.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something since fgets returns NULL when there
    // is no input
    while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );
    /* Parse input */
    if( is_only_whitespace(cmd_str) )//Requirement#6 check if the input is only whitespace
    {
      if(iterator > 14) // if the iterator goes out of bounds for the booking arrays, reset it
      {
        iterator = 0;
      }
      if(listi > 14)
      {
        listi = 0;
      }
      if(historylist[iterator] != NULL) //replacing current position if its already filled
      {
        free(historylist[iterator]);
      }
      historylist[iterator] = strdup(cmd_str);  //saving the command strng for historylist
      //check if its !n
      if(cmd_str[0] == '!')
      {
        cmd_str[0] = '0'; //make token full of numeric
        int commandnum = atoi(cmd_str);//turn command into number, to find position in list
        if(commandnum > 15 || commandnum < -1 || historylist[commandnum] == NULL )
        {
          red();
          printf("Command not in history\n");
          reset();
          continue;
        }
        else
        {
          int siz = sizeof(historylist[commandnum]);
          memcpy(cmd_str,historylist[commandnum],siz);
        }
      }
      char *token[MAX_NUM_ARGUMENTS];

      int   token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
      int i;
      for(i = 0; cmd_str[i];i++)
        cmd_str[i] = tolower(cmd_str[i]);
      char *arg_ptr;

      char *working_str = strdup( cmd_str );
      //save history using cmd_str
    // we are going to move the working_str pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
      char *working_root = working_str;
      while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) &&
                (token_count<MAX_NUM_ARGUMENTS))
      {
        token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
        if( strlen( token[token_count] ) == 0 )
        {
          token[token_count] = NULL;
        }
          token_count++;
      }
      if (strcmp(token[0],"exit") == 0 || strcmp(token[0],"quit") == 0 )
      {
        int j;
        for(j = 0;j < 15; j++)
        {
          free(historylist[j]); //free the entire list
        }
        exit(0); //Requirement #5 COMPLETE
      }
      //printf("%s\n",token[0]);
      if(strcmp(token[0], "listpids") == 0)
      {
        printpids(listp);
      }
      else if(strcmp(token[0],"bg") == 0) //test command with a loop program
      {
        if(token[1]!= NULL)
        {
          sendcontsig(token[1]); //pass pid to background
        }
        else
        {
          int stepback = listi -1; // look a step back from current iterator position
          if(stepback == -1) //if Iterator was currently at zero, go back to 14th position
          {
            stepback = 14;
          }
          if(listp[stepback] != 0) //if there has been a pid placed there, send signal
          {
            kill(listp[stepback],SIGCONT);  //send a continue signal to listpids
          }
        }
      }
      else if(strcmp(token[0],"history") == 0)
      {
        printhist(historylist); //passing a read only history list
      }
      else if(strcmp(token[0], "cd") == 0) //TO DO, ACCEPT PATHS
      {
        if (file ==NULL)
        {
          openFileSystemFirst();
        }
        else
        {
          if (token[1] == NULL || strcmp(token[1], "/")==0) //if my specified directory is empty go to root
          {
            CurClusAddress = RootClusAddress;// go to root directory if just typing cd
            readDirectories(CurClusAddress,file);
          }
          else // else try to enter the directory
          {
            //perform linear search all current directories
            std::vector<std::string> path = cd_token_separator(token[1]);
            int i;
            for(i = 0; i<16;i++)
            {
              if(dir[i].DIR_Attr == 16 ) //dir type
              {

                for (std::string cur : path)
                {
                  int found = searchDirectories(cur, i);
                  //std::cout << cur << found << std::endl;
                  if (found)
                  {
                    //check low cluster for to the function to turn into offset.
                    uint32_t subDirOffset = LBAToOffset(dir[i].DIR_FirstClusterLow);
                    if(dir[i].DIR_FirstClusterLow == 0) //ask about
                    //root value
                    {
                      subDirOffset = RootClusAddress;
                    }
                    CurClusAddress = subDirOffset;
                    readDirectories(subDirOffset,file);
                    i = 0;
                    path.erase( path.begin() ); //delete first member of vector
                  }
                }
              }
            }
            //printf( "mfs: %s: %s: No such directory\n\n",token[0], token[1]);
          }
        }
      }
      else if(strcmp(token[0], "stat") == 0 ) //complete
      {
        if(file == NULL)
        {
          openFileSystemFirst();
        }
        else
        {
          if(token[1])
          {
            int found = 0;
            for(i = 0;i< 16; i++)
            {
              if( dir[i].DIR_Attr == 1 || dir[i].DIR_Attr == 16 || dir[i].DIR_Attr == 32)
              {
                found = searchDirectories(std::string(token[1]), i);
                if( found )
                {
                  char name[12];
                  memcpy(name, dir[i].DIR_Name,11);
                  name[11] = '\0';
                  yellow();
                  printf("Stats for '%s'\n DIR_Attr: %d \n Starting Cluster: %d\n Size: %d\n"
                    ,name,dir[i].DIR_Attr, dir[i].DIR_FirstClusterLow, dir[i].DIR_FileSize);
                  reset();
                  i = 16; //exit loop
                }
              }
            }
            if (!found)
            {
              red();
              printf("Error: File not found\n");
              reset();
            }
          }
        }
      }
      else if(strcmp(token[0], "open") == 0 ) //complete
      {
        /*OPEN FILE SYSTEM*/
        //FILE * fopen ( const char * filename, const char * mode );
        if(file == NULL && token[1]!= NULL)
        {
            file = fopen(token[1],"r");
            if(file == NULL)
            {
              red();
              printf("Error: file named '%s' was not found\n.\n", token[1]);
              reset();
            }
            else
            {
               fseek(file, 11, SEEK_SET);
               fread(&BPB_BytsPerSec, 1, 2, file); //size of 2

               fseek(file, 13, SEEK_SET);
               fread(&BPB_SecPerClus, 1, 1, file); //size of 1

               fseek(file, 14, SEEK_SET);
               fread(&BPB_RsvdSecCnt, 1, 2, file);

               fseek(file, 3, SEEK_SET);
               fread(&BS_OEMName, 1, 8, file); //size of 8

               fseek(file, 16, SEEK_SET);
               fread(&BPB_NumFATS, 1, 1, file);

               fseek(file, 17, SEEK_SET);
               fread(&BPB_RootEntCnt, 1, 2, file);

               fseek(file, 71, SEEK_SET);
               fread(&BS_VolLab, 1, 11, file); //VolLab is empty for this img

               fseek(file, 36, SEEK_SET);
               fread(&BPB_FATSz32, 1, 4, file);

               RootClusAddress = CurClusAddress =(BPB_NumFATS * BPB_FATSz32 * BPB_BytsPerSec) + (BPB_RsvdSecCnt * BPB_BytsPerSec);
               readDirectories(RootClusAddress,file);
               //FirstSectorofCluster = LBAToOffset(dir[0].DIR_FirstClusterLow); //root cluster, for some reason is not equal to
               //the RootClusAddress
             }
        }
        else
        {
          red();
          printf("Error: File system image already open.\n");
          reset();
        }
      }
      else if(strcmp(token[0], "close") == 0 ) //close complete
      {
        if(file != NULL)
        {
          fclose (file);
          file = NULL; //set the pointer to NULL
          green();
          printf("File successfully closed.\n\n");
          reset();
        }
        else
        {
          red();
          printf("Error: File system not open.\n\n");
          reset();
        }
      }
      else if(strcmp(token[0], "info") == 0 ) //info complete
      {
        if(file != NULL)
        {
          printf("BPB_BytsPerSec: %d %x\n", BPB_BytsPerSec, BPB_BytsPerSec);
          printf("BPB_SecPerClus: %d %x\n", BPB_SecPerClus, BPB_SecPerClus);
          printf("BPB_RsvdSecCnt: %d %x\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
          //printf("BS_OEMName: %s \n", BS_OEMName);
          printf("BPB_NumFATS: %d %x\n", BPB_NumFATS, BPB_NumFATS);
          //printf("BPB_RootEntCnt: %d %x\n", BPB_RootEntCnt, BPB_RootEntCnt);
          printf("BPB_FATSz32: %d %x\n", BPB_FATSz32, BPB_FATSz32);
          //printf("RootClusAddress %x\n", RootClusAddress);
        }
        else
        {
          openFileSystemFirst();
        }
      }
      else if(strcmp(token[0], "ls") == 0 ) //list directory, complete
      {
        if(file != NULL)
        {
          if(token[1] == NULL || strcmp(token[1], ".") == 0)
          {
            //print my local directory if "ls" or "ls ." is put in the console
            //printf("name: %s, token: %s\n", token[0], token[1]);
            printDirectory();

          }
          else if(strcmp(token[1], "..") == 0)
          {
            uint32_t subDirOffset = LBAToOffset(dir[1].DIR_FirstClusterLow);
            if(dir[1].DIR_FirstClusterLow == 0)
            //root value
            {
              subDirOffset = RootClusAddress;
            }

            readDirectories(subDirOffset,file);
            printDirectory(); //prints current directory loaded into dir
            readDirectories(CurClusAddress,file); //reread the directories on the cluster you are supposed to be in
          }
          fseek(file, CurClusAddress, SEEK_SET); //go back to beginning of current cluster
        }
        else
          openFileSystemFirst();
      }
      else if(strcmp(token[0], "get") == 0) // /*Test it, it should be able to get all txt files now*/
      {
        if(file == NULL)
        {
          openFileSystemFirst();
        }
        else if( file != NULL && token[1])
        //get <filename>
        {
          int j, found = 0;
          FILE * newfp;
          for(j =0; j<16;j++)
          {
            found = searchDirectories(std::string(token[1]), j);
            if (found)
            {//do reading
              newfp = fopen(token[1], "w");
              int file_size = dir[j].DIR_FileSize;
              int lowCluster = dir[j].DIR_FirstClusterLow;
              int offset = LBAToOffset(lowCluster);
              char buffer[512]; //safe buffer size, check if it is 4mb (max fat32 size)
              printf("%d, size: %d\n",offset ,dir[j].DIR_FileSize );
              if (lowCluster == 0)
                offset = RootClusAddress;
              fseek(file, offset, SEEK_SET);
              while( file_size >= 512) //only puts last set of bits into
              {
                fread(buffer,512,1,file);
                fwrite(buffer, 512, 1, newfp);
                //have to use a buffer to write to file, otherwise i will be writing the pointer
                file_size = file_size - 512;
                //find next logical block
                lowCluster = NextLB(lowCluster,file);
                if( lowCluster == -1) break;

                offset = LBAToOffset(lowCluster);
                fseek(file,offset,SEEK_SET);
              }
              printf("%d\n", file_size);
              if (file_size > 0)
              {
                //fread(newfp, file_size, 1,file);
                fread(buffer,file_size,1,file);
                fwrite(buffer, file_size,1,newfp);

              }

              fclose(newfp);
              j = 16; //once found, break
              //fseek(file, CurClusAddress, SEEK_SET);
            }
          }
          if(!found) //when you don't find the token
          {
            red();
            printf("Error: File not found\n");
            reset();
          }
        }
      }
      else if(strcmp(token[0], "read") == 0) // /*todo*/
      {
        if(file == NULL)
        {
          openFileSystemFirst();
        }
        else if( file != NULL && token[1] && token[2] && token[3])
        //read <filename> <position> <number of bytes>
        {
          int j, found = 0;
          for(j =0; j<16;j++)
          {
            found = searchDirectories(std::string(token[1]), j);
            if (found)
            {
              int file_size = dir[j].DIR_FileSize;
              int lowCluster = dir[j].DIR_FirstClusterLow;
              int start = LBAToOffset(lowCluster);
              std:: string pos(token[2]);
              int offset = stoi(pos);
              int readat = start + offset;
              char buffer[513];
              std:: string num(token[3]);
              int num_bytes = stoi(num);

              // size of hamlet 167771
              while (start < readat)
              {
                lowCluster = NextLB(lowCluster,file);
                start+= 512;
              }
              while( num_bytes >= 512) //only puts last set of bits into
              {
                fseek(file, readat, SEEK_SET); //burstint in
                fread(buffer,512,1,file);
                buffer[512] = '\0';
                //need to find my new cluster
                lowCluster = NextLB(lowCluster,file);
                if( lowCluster == -1) break;

                readat = LBAToOffset(lowCluster);

                num_bytes = num_bytes - 512;
                for(int i = 0;i< sizeof(buffer);i++)
                  printf("%x ", buffer[i]);
              }
              if (num_bytes > 0)
              {
                fseek(file,readat,SEEK_SET);
                fread(buffer,num_bytes,1,file);
                for(int i = 0;i< num_bytes;i++)
                  printf("%x ", buffer[i]);
                buffer[num_bytes] = '\0';
              }

               printf("\n");
               j = 16;
            }

          }
          if(!found) //when you don't find the token
          {
            red();
            printf("Error: File not found\n");
            reset();
          }
        }
      }
      else if(strcmp(token[0], "volume") == 0)//complete
      {
        if(file == NULL)
          openFileSystemFirst();
        else
        {
          char volname[12];
          memcpy(volname,BS_VolLab,11);
          volname[11] = '\0';
          printf("Volume name of the file is '%s '\n", volname);
        }
      }
      //Any command issued after a close, except for open, shall result in
      //“Error: File system image must be opened first.""
      else // if its not a command for parent
      {

        listp[listi] = fork(); //fork off a process
        if( listp[listi] != 0 )
        {
        /*Parent Code*/ //only needed to wait on the child exit
          waitpid(listp[listi], &status, 0);
        }
        else
        {
        /*child code*/ //
          //this way we cant run out of memory in case a large path with a token is taken in
          //else do this
          char* temp = (char*)malloc(15*sizeof(char) + sizeof(token[0]));
          if(temp != NULL)
          {
            sprintf(temp,"./%s", token[0]);
            execv(temp, token); //temp is first arg for execl token[0]
            sprintf(temp,"/usr/local/bin/%s", token[0]);
            execv(temp, token);
            sprintf(temp,"/usr/bin/%s", token[0]);
            execv(temp, token);
            sprintf(temp,"/bin/%s", token[0]);
            execv(temp, token);
          // Check if I was able to find the command
          // if not, send an error message
            if( errno == 2 )
            {
              red();
              printf("%s: Command not found\n\n",token[0]);//command not supported
              reset();
            }
            free(temp);
          }
          //stop doing this
          exit(0); //EXIT_SUCCESS out of child process
        }
        listi++;
      }
      iterator++;
      fflush(NULL);
      free( working_root );
      free( working_str  ); // strdup calls malloc in it, good practice to free it
    }
  }
  return 0;
}
