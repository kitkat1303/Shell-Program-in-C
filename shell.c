#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h> // fork, wait 
#include <sys/wait.h>  // wait 
#include <fcntl.h>



#define MAX_LINE 80 /* The maximum length command */

int clearArgs(char** args) 
{
  if(args == NULL) {
    return -1;
  }
  int index = 0;
  while(args[index] != NULL) 
  {
      free(args[index]);
      args[index] = NULL;
  }
  return 1;
}


void resetFlags(int* waitFlag,
                int* pipeFlag,
                int* outputFlag,
                int* inputFlag,
                int* historyFlag)
{
  *waitFlag     = 0;
  *pipeFlag     = 0;
  *outputFlag   = 0;
  *inputFlag    = 0;
  *historyFlag  = 0;
  return;
}

// used for !! and exit
int checkForStartFlags(char** args, int* flag, const char* startFlagIndicator) 
{

  if(args == NULL) 
  {
      return -1;
  }

  if(args[0] == NULL) {
    return -1;
  }

  // if the flag is found
  if(!(strcmp(args[0],startFlagIndicator))) 
  {
    // if there is in appropraite use of history
    if(args[1]!= NULL) 
    {
      return -1;
    } 
    else
    {
        *flag = 1;
        return 1;
    }
  }

    return 0;
}


int setFlags(char** rawInput, 
              int maxSize,
              int* waitFlag, 
              int* pipeFlag, 
              int* outputFlag, 
              int* inputFlag) 
{

  int curChar = 0;

  const char pipe[]     = "|";
  const char wait[]     = "&";
  const char outRedir[] = ">";
  const char inRedir[]  = "<";

  // input verification
  if(rawInput == NULL) 
  {
    return -1;
  }

  while(rawInput[curChar] != NULL && curChar < maxSize)
  {

      int isOutRedir = strcmp(rawInput[curChar], outRedir);

      if(!(isOutRedir)) 
      {
        *outputFlag = curChar;
      }

      int isInRedir = strcmp(rawInput[curChar], inRedir);
      if(!(isInRedir)) 
      {
        *inputFlag = curChar;
      }

      int isWait = strcmp(rawInput[curChar], wait);
      if(!(isWait)) 
      {
        *waitFlag = curChar;
      }

      int isPipe = strcmp(rawInput[curChar], pipe);
      if(!(isPipe)) 
      {
        *pipeFlag = curChar;
      }

      curChar++;
  }

  return 1;
}


// reads user input from the shell
char* getLineFromShell() {
    char* buffer = NULL;
    size_t buffer_size = MAX_LINE * (sizeof(char));
    size_t user_input;
    user_input = getline(&buffer, &buffer_size, stdin);

    return buffer;
}

// return an int error state for ones the invalidate
// if the flag is set do not add it to the args
void fillCommandLineArguments(char* buf, char** args, int size) 
{
    // free the memory associated if its not null
    if(args[0] != NULL) {
      clearArgs(args);
    }
    char* token;
    const char delim[3] = " \n";
    token = strtok(buf,delim);
    args[0] = token;
    int index = 1;
    while(token != NULL && index < size) 
    {
      token = strtok(NULL,delim);
      args[index] = token;
      index++;  
    }
    // set the flag
}

int copyInstructionsList(char** curCommands, char** copied) 
{
  if(curCommands != NULL) 
  {
    int index = 0;

    while(curCommands[index] != NULL) 
    {    
        copied[index] = strdup(curCommands[index]);
        index++;
    }

    copied[index] = NULL;

    return 1;
  }
  return -1;
}


// we want to adjust this
// we make copies to ensure that when we modify we are not modifying the origional set
// need to free the copied
int formatPipeArguments(char** args, char** inputArgs, char** outputArgs, int* pipeFlag) 
{
  int worked = copyInstructionsList(args,inputArgs);
  
  inputArgs[*pipeFlag] = NULL;

  // going past the pipe
  int startHere = *pipeFlag + 1;
  for(int i = 0; inputArgs[startHere + i] != NULL; i++) {
    outputArgs[i] = inputArgs[startHere+i];
  }
  return 1;
}




void processPipe(char** args,int* pipeFlag) 
{
  enum {READ, WRITE};
  int fd[2];
  char* inputArgs[MAX_LINE/2 + 1] = {NULL};
  char* outputArgs[MAX_LINE/2 + 1] = {NULL};
  formatPipeArguments(args,inputArgs, outputArgs,pipeFlag);


  int pid1 = fork();
  if (pipe(fd) < 0)
  {
    perror("Error in creating pipe");
    exit(EXIT_FAILURE);
  }

  // child will execute the first and write it to the pipe
  if(pid1 == 0) 
  {
    // make grandchild to process the input to the output
    int pid2 = fork();

    if(pid2 == 0)
    {
      close(fd[READ]); 
      dup2(fd[WRITE], 1);   //stdout is now child's read pipe
      execvp(inputArgs[0], inputArgs);
    }  
    else 
    {
      int status;
      wait(&status);
      close(fd[WRITE]); 
      dup2(fd[READ], 0);
      execvp(outputArgs[0], outputArgs);
    }
  } 
  else 
  {
    int status;
    wait(&status);
  }

  exit(0);
}

int sizeOfCommands(char**args) {
  int count = 0;
  while(args[count] != NULL) {
    count++;
  }
  return count+1;
}

// assumes that args in not null at this point
void processInputRedirect(char** args, int* inputFlag) 
{
  int size = sizeOfCommands(args);

  // possibly make this copy above and then pass the copy so args is preserved
  char** toProcess = malloc(size*sizeof(char*));
  // possibly make this copy above and then pass the copy so args is preserved
  int pid = fork();
  if(pid == 0) {
    copyInstructionsList(args,toProcess);
    //assumes args in not null
    // grab the file right after the redirect
    const char* fileName = toProcess[*inputFlag + 1];
    toProcess[*inputFlag] = NULL;

    //creates but appends incase it exists
    int toInput = open(fileName,O_CREAT|O_RDWR,0777);
    dup2(toInput,STDIN_FILENO);
    execvp(toProcess[0],toProcess);
  } else {
    int status;
    wait(&status);
    clearArgs(toProcess);
    free(toProcess);
    exit(0);
  }
}



void processOutputRedirect(char** args, int* outputFlag) 
{
  int size = sizeOfCommands(args);

  // possibly make this copy above and then pass the copy so args is preserved
  char** toProcess = malloc(size*sizeof(char*));
  int pid = fork();
  if(pid == 0) 
  {
    copyInstructionsList(args,toProcess);
    printf("\n%d\n",*outputFlag);
    //assumes args in not nullOUT
    // grab the file right after the redirect
    const char* fileName = toProcess[*outputFlag + 1];
    toProcess[*outputFlag] = NULL;

    //creates but appends incase it exists
    int toOutput = open(fileName,O_CREAT|O_RDWR, 0777);
    dup2(toOutput,STDOUT_FILENO);
    execvp(toProcess[0],toProcess);
  } else {
    int status;
    wait(&status);
    clearArgs(toProcess);
    free(toProcess);
    exit(0);
  }
  

}



void processCommand(char** args,int* pipeFlag, int* outputFlag, int* inputFlag, int* historyFlag, int* waitFlag) 
{
  resetFlags(waitFlag,pipeFlag,outputFlag,inputFlag,historyFlag);
  setFlags(args,MAX_LINE/2 + 1, waitFlag,pipeFlag,outputFlag,inputFlag);

  int pid = fork();
  if(*waitFlag) {
    args[*waitFlag] = NULL;
  }
  if(pid == 0) 
  {
    if((*inputFlag)) {
      //printf("\nInputRedir\n");
      processInputRedirect(args,inputFlag);
    } else if((*outputFlag)) {
            //printf("\n%d\n",*outputFlag);
      processOutputRedirect(args,outputFlag);
    } else if((*pipeFlag)) {
      //printf("\nPipe\n");
      processPipe(args,pipeFlag);
    } else {
      //printf("\nReg\n");
      execvp(args[0],args);
    }
  } else {
    if(!(*waitFlag)) {
      int status;
      wait(&status);
      //printf("\nWaiting\n");
    } else {
      //printf("\nNot Waiting\n");
      return;
    }
  }
  
  return;
}








int main(void)
{
  char *args[MAX_LINE/2 + 1] = {NULL}; /* command line arguments */
  char *prev[MAX_LINE/2 + 1] = {NULL}; /* command line arguments */
  //possibly adjust to 1 then 0
  int initializeW     = 0;
  int initializeP     = 0;
  int initializeO     = 0;
  int initializeI     = 0;
  int initializeE     = 0;
  int initializeH     = 0;
  int* waitFlag       = &initializeW;
  int* pipeFlag       = &initializeP;
  int* outputFlag     = &initializeO;
  int* inputFlag      = &initializeI;
  int* exitFlag       = &initializeE;
  int* historyFlag    = &initializeH;

  const char exitSymb[] = "exit";
  const char histSymb[] = "!!";
  
  while (!(*exitFlag)) {

    printf("osh> ");
    fflush(stdout);
    //CLEAR FLAGS
    char* buffer = getLineFromShell();
    fillCommandLineArguments(buffer,args,MAX_LINE/2 + 1);

    int exitStatus = checkForStartFlags(args,exitFlag,exitSymb);

    if(exitStatus == 1) {
      break;
    } 
    else if(exitStatus == -1) 
    {
      printf("\nInvalid Exit Coniditon\n");
    } 
    else 
    {
      
      int histStatus = checkForStartFlags(args,historyFlag,histSymb);
      if(histStatus == 1) {
        if(prev[0] == NULL) {
            printf("\nNo History To Process\n");
        } else {
          processCommand(prev,pipeFlag,outputFlag,inputFlag,historyFlag,waitFlag);
        }
      } else if(histStatus == -1) {
          printf("\nInvalid History Attempt\n");
      } else {
        // store the current command for previous
          clearArgs(prev);
          copyInstructionsList(args,prev);
          processCommand(args,pipeFlag,outputFlag,inputFlag,historyFlag,waitFlag);
      }
    }
  }

  exit(0);
  
  return 0;
}
