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

typedef struct process
{
    cmdLine *cmd;         /* the parsed command line*/
    pid_t pid;            /* the process id that is running the command*/
    int status;           /* status of the process: RUNNING/SUSPENDED/TERMINATED */
    struct process *next; /* next process in chain */
} process;

#define TERMINATED -1
#define RUNNING 1
#define SUSPENDED 0

void execute(cmdLine *cmdLine, int debug, process **process_list);
void error(char *errorMessage, cmdLine *cmdLine, int isExecvp);
int signalProcess(char *pid, int signal);
int cd(char *path);
int procs(process **process_list);
int pipeCommands(cmdLine *cmdLine1, cmdLine *cmdLine2);
void inputRedirect(char const *path);
void outputRedirect(char const *path);
void addProcess(process **process_list, cmdLine *cmd, pid_t pid);
void printProcessList(process **process_list);
void freeProcessList(process *process_list);
void updateProcessList(process **process_list);
void updateProcessStatus(process *process_list, int pid, int status);
void redirectAndExecute(cmdLine *cmdLine);

int main(int argc, char const *argv[])
{
    char cwd[PATH_MAX];
    char line[2048];
    process *firstProcess;
    process **process_list = &firstProcess;

    int debug = 0;

    // Debug mode
    for (int i = 1; i < argc; i++)
        if (argv[i][0] == '-' && argv[i][1] == 'd')
            debug = 1;

    while (1)
    {
        cmdLine *cmdLine;
        getcwd(cwd, PATH_MAX);
        printf("%s ", cwd);
        if (fgets(line, sizeof(line), stdin) == NULL)
            error("Line Reading Error", cmdLine, 1);

        if (strncmp(line, "quit\n", 5) == 0)
            exit(EXIT_SUCCESS);

        cmdLine = parseCmdLines(line);

        if (cmdLine == NULL)
            error("Parsing Error", cmdLine, 1);

        execute(cmdLine, debug, process_list);
        // freeCmdLines(cmdLine);
    }
    return 0;
}

void execute(cmdLine *cmdLine, int debug, process **process_list)
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
    else if (strcmp(cmdLine->arguments[0], "procs") == 0)
        isBasicCommand = procs(process_list);
    else if (cmdLine->next)
        isBasicCommand = pipeCommands(cmdLine, cmdLine->next);

    if (isBasicCommand)
        return;

    pid_t pid = fork();

    // Couldn't fork for any reason
    if (pid < 0)
        error("Fork Error", cmdLine, 1);

    // Parent process
    if (pid > 0 && debug)
        fprintf(stderr, "PID: %d\nExecuting command: %s\n", pid, cmdLine->arguments[0]);
    if (pid > 0)
        addProcess(process_list, cmdLine, pid);
    if (pid > 0 && cmdLine->blocking)
        waitpid(pid, NULL, 0);

    // Child process
    if (pid == 0)
        redirectAndExecute(cmdLine);
}

void error(char *errorMessage, cmdLine *cmdLine, int isExecvp)
{
    perror(errorMessage);
    if (cmdLine)
        freeCmdLines(cmdLine);
    if (isExecvp)
        _exit(EXIT_FAILURE);
    exit(EXIT_FAILURE);
}

int signalProcess(char *pid, int signal)
{
    int PID = atoi(pid);
    if (kill(PID, signal) < 0)
        error("Waking Process Error", NULL, 0);

    return 1;
}

int cd(char *path)
{
    if (chdir(path) < 0)
        error("Changing Directories Error", NULL, 0);

    return 1;
}

int procs(process **process_list)
{
    printProcessList(process_list);
    return 1;
}

void inputRedirect(char const *path)
{
    int fileDescriptor = open(path, O_RDONLY);

    if (fileDescriptor < 0)
        error("Redirect Error", NULL, 0);

    close(STDIN_FILENO);

    if (dup(fileDescriptor) < 0)
        error("Duplication Error", NULL, 0);

    close(fileDescriptor);
}

void outputRedirect(char const *path)
{
    int fileDescriptor = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fileDescriptor < 0)
        error("Redirect Error", NULL, 0);

    close(STDOUT_FILENO);

    if (dup(fileDescriptor) < 0)
        error("Duplication Error", NULL, 0);

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
        error("Pipe Error", cmdLine1, 0);

    // Fork first child process
    if ((pid1 = fork()) < 0)
        error("Fork Error", cmdLine1, 0);

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
        error("Fork Error", cmdLine1, 0);

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
        error("Execution Error", cmdLine, 1);
}

void addProcess(process **process_list, cmdLine *cmd, pid_t pid)
{
    // printf("the command is: %s and the num of args is: %d\n", cmd->arguments[0], cmd->argCount);
    process *newProcess = malloc(sizeof(process));
    newProcess->cmd = cmd;
    newProcess->pid = pid;
    newProcess->status = RUNNING;

    process *currProcess = *process_list;
    if (!currProcess)
    {
        (*process_list) = newProcess;
        return;
    }

    while (currProcess->next)
        currProcess = currProcess->next;

    currProcess->next = newProcess;
}

void freeProcessList(process *process_list)
{
    if (process_list == NULL)
        return;

    freeProcessList(process_list->next);
    freeCmdLines(process_list->cmd);
    free(process_list);
}

void updateProcessList(process **process_list)
{
    int status = 0;
    process *currProcess = *process_list;
    while (currProcess)
    {
        pid_t pid = waitpid(currProcess, &status, WNOHANG);
        if (status < 0)
            error("asdfasdf", NULL, 0);
        if (status > 0)
            updateProcessStatus(currProcess, pid, status);
        currProcess = currProcess->next;
        status = 0;
    }
}

void updateProcessStatus(process *process_list, int pid, int status)
{
    process_list->status = status;
    process_list->pid = pid;
}

void printProcessList(process **process_list)
{
    int j = 0;
    process *currProcess = *process_list;
    printf("Index        PID          STATUS       Command      \n");
    while (currProcess)
    {
        printf("%d) %d %d ", j, currProcess->pid, currProcess->status);
        for (int i = 0; i < currProcess->cmd->argCount; i++)
            printf("%s%s", currProcess->cmd->arguments[i], i == currProcess->cmd->argCount - 1 ? "\n" : " ");

        currProcess = currProcess->next;
        j++;
    }
}
