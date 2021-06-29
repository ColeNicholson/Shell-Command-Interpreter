/*
 * This project is code for a simple shell program modelled after Bash shell.
 * It is by no means perfect but it does replicate some of the more basic functionality
 * such as allowing the user to call different linux programs and input/output redirection.
 * I understand that there are better ways of doing what was done here and much of what was
 * written here was done for one of my classes at university.
 */

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "constants.h"
#include "parsetools.h"

void syserror(const char *s);

int main()
{
    // Buffer for reading one line of input
    char line[MAX_LINE_CHARS];
    // holds separated words based on whitespace
    char* line_words[MAX_LINE_WORDS + 1];
    pid_t pids[100]; // Holds the pid for each forked process
    //const pid_t parent = getpid(); // Shell pid, for debugging purposes
    int numPipes, numArgs; // Counts the number of arguments and the number of pipes in the command line
    int pipePos[100]; // Array which holds the position of the pipes in the arg list, but since pipes are removed it will hold the position of the next command in the arg list
    int pipeArr[100][2]; // Array which will be used for initializing pipes
    int appendFlag; // Holds the flag for each command which determines if append mode should be used or not
    char* args[MAX_LINE_WORDS + 1]; // Arg list, contains the all args for all commands, separated by a NULL value
    char* inDirect, outDirect; // pointer to name of an input or output redirect file
    int eFlag, dFlag, quoteFlag = 0; // 1 for debug mode, 0 for regular mode on dFlag
    // Loop until user hits Ctrl-D (end of input)
    // or some other input error occurs
    while( fgets(line, MAX_LINE_CHARS, stdin) )
    {
        int num_words = split_cmd_line(line, line_words);
        numPipes, numArgs, eFlag, appendFlag, quoteFlag = 0;
        inDirect = NULL;
        outDirect = NULL;
        for(int i = 0; i < MAX_LINE_WORDS + 1; i++)
        {
            args[i] = NULL;
        }
        for(int i = 0; i < 100; i++)
        {
            pipePos[i] = 0;
            pids[i] = 0;
        }
        char quote[MAX_LINE_CHARS];
        for (int i=0; i < num_words; i++)
        { // Process each word in command line
            char *test = strchr(line_words[i],'\"');
            if(strchr(line_words[i], '\"') != NULL && quoteFlag == 0)
            { // Detect opening quote and add that c-string to the 'quote'
                quoteFlag = 1;
                strcat(quote, line_words[i] + 1);
            }
            else if(strchr(line_words[i], '\"') != NULL && quoteFlag == 1)
            { // Detect enging quote and add that c-string to the quote and add the quote to the arg list
                quoteFlag = 0;
                strcat(quote, " ");
                strcat(quote, line_words[i]);
                quote[strlen(quote) - 1] = '\0';
                args[numArgs] = quote;
                numArgs++;
            }
            else if(quoteFlag == 1)
            { // In the middle of a quote, add the word to the quote
            strcat(quote," ");
            strcat(quote,line_words[i]);
            }
            else if(*line_words[i] == PIPE)
            { // Logic for setting up a pipe later
                args[numArgs] = NULL; // We have seen a pipe, so we know that the arguments for the first command end here.
                numArgs++; // Increment num args
                numPipes++; // Increment numPipes
                pipePos[numPipes] = numArgs; // Record the pipes position in an array.
            }
            else if(*line_words[i] == INPUT_REDIRECT)
            { // Grabs the redirection the jumps to the argument after the input file
                inDirect = line_words[i + 1]; // Grab the arg for the input file
                i++;
                continue;
            }
            else if(*line_words[i] == OUTPUT_REDIRECT)
            { // Grabs the redirection then jumps to the argument after the output file
                if(strcmp((char*) line_words[i], ">>") == 0)
                appendFlag = 1;
                outDirect = line_words[i + 1];
                i++;
                continue;
            }
            else
            { // Process all other words
                args[numArgs] = line_words[i];
                numArgs++;
            }
        }
        args[numArgs + 1] = NULL; // Terminate last arg chunk with a NULL pointer

        if(dFlag)
        {
            printf("inDirect = %s\n", inDirect); // Debug
            printf("outDirect = %s\n", outDirect);
            for(int i = 0; i < numArgs + 1; i++)
            { // Check args for escape characters
                printf("arg = %s\n", args[i]); // Debug
            }
            printf("numPipes = %d\n", numPipes); // Debug
            for(int i = 0; i < numPipes + 1; i++) // Debug
                printf("pipePos in command line: %d\n", pipePos[i]); // Debug
        }

        for(int i = 0; i < numPipes; i++)
            if(pipe(pipeArr[i]) == -1) // Create pipes for each pipe
                syserror("Failed to create pipe.");

        //fprintf(stderr, "pid = %d\n", parent); // Debug

        for(int i = 0; i < numPipes + 1; i++) // For each command
        {
            switch(pids[i] = fork())
            { // Child code
            case -1:
                syserror("Fork failed");
                break;
            case 0:
                if(numPipes > 0)
                { // Set up pipe redirection
                    if(i == 0) // Only do for the first command
                    {
                        if(dFlag)
                            fprintf(stderr, "First Output Pipe: i = %d | pipeArr[%d][1] = %d | Associated Pipe = %d\n", i, i, pipeArr[i][1], pipeArr[i][0]); // Debug
                        dup2(pipeArr[i][1], 1);
                        for(int j = 0; j < numPipes; j++)
                        { // Close all pipes
                            close(pipeArr[j][0]);
                            close(pipeArr[j][1]);
                        }
                    }
                    else if(i == numPipes) // Only do for the last command
                    {
                        if(dFlag)
                            fprintf(stderr, "Last Input Pipe: i - 1 = %d | pipeArr[%d][0] = %d | Associated Pipe = %d\n", i-1, i-1, pipeArr[i-1][0], pipeArr[i-1][1]); // Debug
                        dup2(pipeArr[i - 1][0], 0);
                        for(int j = 0; j < numPipes; j++)
                        { // Close all pipes
                            close(pipeArr[j][0]);
                            close(pipeArr[j][1]);
                        }
                    }
                    else // Every command in the middle
                    {
                        if(dFlag)
                        {
                            fprintf(stderr, "Intermediate Input Pipe: i - 1 = %d | pipeArr[%d][0] = %d | Associated Pipe = %d\n", i - 1, i - 1, pipeArr[i - 1][0], pipeArr[i-1][1]); // Debug
                            fprintf(stderr, "Intermediate Output Pipe: i = %d | pipeArr[%d][1] = %d | Associated Pipe = %d\n", i, i, pipeArr[i][1], pipeArr[i][0]); // Debug
                        }
                        dup2(pipeArr[i - 1][0], 0); // Read from pipe outputted by last command
                        dup2(pipeArr[i][1], 1); // Write to pipe associated with this command
                        for(int j = 0; j < numPipes; j++)
                        { // Close all pipes
                            close(pipeArr[j][0]);
                            close(pipeArr[j][1]);
                        }
                    }
                }
                if(inDirect != NULL && i == 0) // If not NULL -> filename detected
                { // Sets up the redirect for input
                    int infiledesc = open(inDirect, O_RDONLY);
                    if(dFlag)
                        fprintf(stderr, "Opening input file: %s\n", inDirect);
                    dup2(infiledesc, 0);
                    close(infiledesc);
                }
                if(outDirect != NULL && i == numPipes) // If not NULL -> filename detected
                { // Sets up the redirect for the output
                    int outfiledesc;
                    if(appendFlag == 1) // Append mode detected
                        outfiledesc = open(outDirect, O_CREAT | O_WRONLY | O_APPEND, 0666);
                    else // If not append mode, overwrite
                        outfiledesc = open(outDirect, O_CREAT | O_WRONLY | O_TRUNC, 0666);
                    if(dFlag)
                        fprintf(stderr, "Opening output file: %s\n", outDirect);
                    dup2(outfiledesc, 1);
                    close(outfiledesc);
                }
                if(dFlag)
                    fprintf(stderr, "Calling exec with %s\n", args[pipePos[i]]); // Debug
                execvp(args[pipePos[i]], &args[pipePos[i]]);
                syserror("Could not exec program");
                break;
            default:
                if(dFlag)
                fprintf(stderr, "The %d child pid is: %d\n", i, pids[i]); // debug
                break;
            }
        }

        // Deallocate the quote after the exec has been called.
        memset(quote, 0, MAX_LINE_CHARS*(sizeof(char)));

        for(int i = 0; i < numPipes; i++)
        { // Close all pipes in parent
            close(pipeArr[i][0]);
            close(pipeArr[i][1]);
        }

        while(wait(NULL) != -1)
            ; // Reap all children

        if(dFlag)
            printf("\n"); // Debug
    }
    //free((char*) inDirect);
    //free((char*) outDirect);
    return 0;
}

void syserror(const char* s)
{
    extern int errno;
    fprintf(stderr, "%s\n", s);
    fprintf(stderr, " (%s)\n", strerror(errno));
    exit(1);
}
