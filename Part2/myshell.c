#include "linux/limits.h"
#include "unistd.h"
#include "stdio.h"
#include "stdlib.h"
#include "sys/wait.h"
#include "LineParser.h"
#include "string.h"
#include "sys/types.h"
#include "signal.h"
#include "sys/stat.h"
#include "fcntl.h"

void execute(cmdLine *cmdLine, int debug);
void error(char *errorMessage);
int signalProcess(char *pid, int signal);
int cd(char *path);
int pipeCommands(cmdLine *cmdLine1, cmdLine *cmdLine2);
void inputRedirect(char const *path);
void outputRedirect(char const *path);

int main(int argc, char const *argv[])
{
    char cwd[PATH_MAX];
    char line[2048];
    cmdLine *cmdLine;
    int debug = 0;

    // Debug mode
    for (int i = 1; i < argc; i++)
        if (argv[i][0] == '-' && argv[i][1] == 'd')
            debug = 1;

    while (1)
    {
        getcwd(cwd, PATH_MAX);
        printf("%s ", cwd);
        if (fgets(line, sizeof(line), stdin) == NULL)
            error("Line Reading Error");

        if (strncmp(line, "quit\n", 5) == 0)
            exit(EXIT_SUCCESS);

        cmdLine = parseCmdLines(line);

        if (cmdLine == NULL)
            error("Parsing Error");

        execute(cmdLine, debug);
        freeCmdLines(cmdLine);
    }
    return 0;
}

void execute(cmdLine *cmdLine, int debug)
{
    int isBasicCommand = 0;

    if (strcmp(cmdLine->arguments[0], "cd") == 0)
        isBasicCommand = cd(cmdLine->arguments[1]);
    else if (strcmp(cmdLine->arguments[0], "kill") == 0)
        isBasicCommand = signalProcess(cmdLine->arguments[1], SIGINT);
    else if (strcmp(cmdLine->arguments[0], "wake") == 0)
        isBasicCommand = signalProcess(cmdLine->arguments[1], SIGCONT);
    else if (strcmp(cmdLine->arguments[0], "suspend") == 0)
        isBasicCommand = signalProcess(cmdLine->arguments[1], SIGTSTP);
    else if (cmdLine->next)
        isBasicCommand = pipeCommands(cmdLine, cmdLine->next);

    if (isBasicCommand)
        return;

    pid_t pid = fork();

    // Couldn't fork for any reason
    if (pid < 0)
        error("Fork Error");

    // Parent process
    if (pid > 0 && debug)
        fprintf(stderr, "PID: %d\nExecuting command: %s\n", pid, cmdLine->arguments[0]);
    if (pid > 0 && cmdLine->blocking)
        waitpid(pid, NULL, 0);

    // Child process
    redirectAndExecute(cmdLine);
}

void error(char *errorMessage)
{
    perror(errorMessage);
    exit(EXIT_FAILURE);
}

int signalProcess(char *pid, int signal)
{
    int PID = atoi(pid);
    if (kill(PID, signal) < 0)
        error("Waking Process Error");

    return 1;
}

int cd(char *path)
{
    if (chdir(path) < 0)
        error("Changing Directories Error");

    return 1;
}

void inputRedirect(char const *path)
{
    int fileDescriptor = open(path, O_RDONLY);

    if (fileDescriptor < 0)
        error("Redirect Error");

    close(STDIN_FILENO);

    if (dup(fileDescriptor) < 0)
        error("Duplication Error");

    close(fileDescriptor);
}

void outputRedirect(char const *path)
{
    int fileDescriptor = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fileDescriptor < 0)
        error("Redirect Error");

    close(STDOUT_FILENO);

    if (dup(fileDescriptor) < 0)
        error("Duplication Error");

    close(fileDescriptor);
}

int pipeCommands(cmdLine *cmdLine1, cmdLine *cmdLine2)
{
    if (cmdLine1->outputRedirect || cmdLine2->inputRedirect)
    {
        fprintf(stderr, "Illegal Redirecting Error");
        exit(EXIT_FAILURE);
    }

    // Create pipe
    int pipeFileDescriptor[2];
    pid_t pid1, pid2;

    if (pipe(pipeFileDescriptor) < 0)
        error("Pipe Error");

    // Fork first child process
    if ((pid1 = fork()) < 0)
        error("Fork Error");

    if (pid1 > 0)
        close(pipeFileDescriptor[1]);

    if (pid1 == 0)
    {
        close(STDOUT_FILENO);
        dup(pipeFileDescriptor[1]);
        close(pipeFileDescriptor[1]);

        redirectAndExecute(cmdLine1);
    }

    // Fork second child process
    if (pid1 > 0 && (pid2 = fork()) < 0)
        error("Fork Error");

    if (pid1 > 0 && pid2 > 0)
    {
        close(pipeFileDescriptor[0]);
        waitpid(pid1, NULL, 0);
        waitpid(pid2, NULL, 0);
    }
    if (pid1 > 0 && pid2 == 0)
    {
        close(STDIN_FILENO);
        dup(pipeFileDescriptor[0]);
        close(pipeFileDescriptor[0]);

        redirectAndExecute(cmdLine2);
    }

    return 1;
}

void redirectAndExecute(cmdLine *cmdLine)
{
    if (cmdLine->inputRedirect)
        inputRedirect(cmdLine->inputRedirect);
    if (cmdLine->outputRedirect)
        outputRedirect(cmdLine->outputRedirect);
    if (execvp(cmdLine->arguments[0], cmdLine->arguments) < 0)
        error("Execution Error");
}