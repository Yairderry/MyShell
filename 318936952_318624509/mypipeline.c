#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGUMENTS 256

void error(char *errorMessage);

int main(int argc, char const *argv[])
{
    int pipeFileDescriptor[2];
    pid_t pid1, pid2;
    char *const arguments[MAX_ARGUMENTS];
    const char *command;

    if (pipe(pipeFileDescriptor) < 0)
        error("Pipe Error");

    fprintf(stderr, "(parent_process>forking…)\n");
    if ((pid1 = fork()) < 0)
        error("Fork Error");

    if (pid1 > 0)
    {
        fprintf(stderr, "(parent_process>created process with id: %d)\n", pid1);
        fprintf(stderr, "(parent_process>closing the write end of the pipe…)\n");
        close(pipeFileDescriptor[1]);
    }

    if (pid1 == 0)
    {
        fprintf(stderr, "(child1>redirecting stdout to the write end of the pipe…)\n");
        close(STDOUT_FILENO);
        dup(pipeFileDescriptor[1]);
        close(pipeFileDescriptor[1]);

        fprintf(stderr, "(child1>going to execute cmd: …)\n");
        command = "ls";
        ((char **)arguments)[0] = "ls";
        ((char **)arguments)[1] = "-l";
        ((char **)arguments)[2] = NULL;

        if (execvp(command, arguments) < 0)
            error("Execution Error");
    }

    if (pid1 > 0 && (pid2 = fork()) < 0)
        error("Fork Error");

    if (pid1 > 0 && pid2 > 0)
    {
        fprintf(stderr, "(parent_process>closing the read end of the pipe…)\n");
        close(pipeFileDescriptor[0]);
        fprintf(stderr, "(parent_process>waiting for child processes to terminate…)\n");
        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);
    }
    if (pid1 > 0 && pid2 == 0)
    {
        fprintf(stderr, "(child2>redirecting stdin to the read end of the pipe…)\n");
        close(STDIN_FILENO);
        dup(pipeFileDescriptor[0]);
        close(pipeFileDescriptor[0]);

        fprintf(stderr, "(child2>going to execute cmd: …)\n");
        command = "tail";
        ((char **)arguments)[0] = "tail";
        ((char **)arguments)[1] = "-n";
        ((char **)arguments)[2] = "2";
        ((char **)arguments)[3] = NULL;

        if (execvp(command, arguments) < 0)
            error("Execution Error");
    }

    if (pid1 > 0 || pid2 > 0)
        fprintf(stderr, "(parent_process>exiting…)\n");

    return 0;
}

void error(char *errorMessage)
{
    perror(errorMessage);
    exit(EXIT_FAILURE);
}
