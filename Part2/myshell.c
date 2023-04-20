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
#include "errno.h"

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
#define HISTLEN 20

void execute(cmdLine *cmdLine);
int quit();
void error(char *errorMessage, int isExecvp);
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
void printStatus(int status);
void freeHistory();
int printHistory();
void addHistory(char *cmdLine);
int executeLastCommand();
int executeNthCommand(char *n);

// Global Variables
int debug = 0;
process **process_list;
char *history[HISTLEN];
int oldestIndex = 0;
int newestIndex = 0;

int main(int argc, char const *argv[])
{
    char cwd[PATH_MAX];
    process *firstProcess;
    process_list = &firstProcess;

    // Debug mode
    for (int i = 1; i < argc; i++)
        if (argv[i][0] == '-' && argv[i][1] == 'd')
            debug = 1;

    while (1)
    {
        char *line = (char *)malloc(2048);
        cmdLine *cmdLine;
        getcwd(cwd, PATH_MAX);
        printf("%s ", cwd);

        if (fgets(line, 2048, stdin) == NULL)
            error("Line Reading Error", 0);

        if (line[0] != '!' && (!history[(newestIndex - 1) % HISTLEN] || strcmp(history[(newestIndex - 1) % HISTLEN], line) != 0))
            addHistory(line);

        if ((cmdLine = parseCmdLines(line)) == NULL)
            error("Parsing Error", 0);

        execute(cmdLine);
    }
    return 0;
}

void execute(cmdLine *cmdLine)
{
    int isBasicCommand = 0;

    // Basic shell commands
    if (strcmp(cmdLine->arguments[0], "cd") == 0)
        isBasicCommand = cd(cmdLine->arguments[1]);
    else if (strcmp(cmdLine->arguments[0], "quit") == 0)
        isBasicCommand = quit();
    // History commands
    else if (strcmp(cmdLine->arguments[0], "history") == 0)
        isBasicCommand = printHistory();
    else if (strcmp(cmdLine->arguments[0], "!!") == 0)
        isBasicCommand = executeLastCommand();
    else if (cmdLine->arguments[0][0] == '!')
        isBasicCommand = executeNthCommand(cmdLine->arguments[0] + 1);
    // Job control
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
        error("Fork Error", 0);

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

void error(char *errorMessage, int isExecvp)
{
    perror(errorMessage);
    freeProcessList(*process_list);
    freeHistory();
    if (isExecvp)
        _exit(EXIT_FAILURE);
    exit(EXIT_FAILURE);
}

int quit()
{
    // TODO: terminate all processes
    freeProcessList(*process_list);
    freeHistory();
    exit(EXIT_SUCCESS);

    return 1;
}

int signalProcess(char *pid, int signal)
{
    int PID = atoi(pid);
    if (kill(PID, signal) < 0)
        error("Waking Process Error",  0);

    return 1;
}

int cd(char *path)
{
    if (chdir(path) < 0)
        error("Changing Directories Error", 0);

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
        error("Redirect Error", 0);

    close(STDIN_FILENO);

    if (dup(fileDescriptor) < 0)
        error("Duplication Error", 0);

    close(fileDescriptor);
}

void outputRedirect(char const *path)
{
    int fileDescriptor = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fileDescriptor < 0)
        error("Redirect Error", 0);

    close(STDOUT_FILENO);

    if (dup(fileDescriptor) < 0)
        error("Duplication Error", 0);

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
        error("Pipe Error", 0);

    // Fork first child process
    if ((pid1 = fork()) < 0)
        error("Fork Error", 0);

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
        error("Fork Error",  0);

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
        error("Execution Error",  1);
}

void addProcess(process **process_list, cmdLine *cmd, pid_t pid)
{
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
        int code = waitpid(currProcess->pid, &status, WNOHANG | WUNTRACED | WCONTINUED);
        if (code < 0 && errno != ECHILD)
            error("Wait Error", 0);
        if (code < 0 && errno == ECHILD)
            currProcess->status = TERMINATED;
        if (code > 0)
            updateProcessStatus(currProcess, currProcess->pid, status);
        currProcess = currProcess->next;
        status = 0;
    }
}

void updateProcessStatus(process *process_list, int pid, int status)
{
    if (WIFEXITED(status) || WIFSIGNALED(status))
        process_list->status = TERMINATED;
    else if (WIFSTOPPED(status))
        process_list->status = SUSPENDED;
    else if (WIFCONTINUED(status))
        process_list->status = RUNNING;
}

void printProcessList(process **process_list)
{
    updateProcessList(process_list);
    int index = 0;
    process *currProcess = *process_list;
    process *prevProcess = NULL;
    printf("Index        PID          STATUS       Command      \n");
    while (currProcess)
    {
        printf("%d)       %d       ", index, currProcess->pid);
        printStatus(currProcess->status);
        for (int i = 0; i < currProcess->cmd->argCount; i++)
            printf("%s%s", currProcess->cmd->arguments[i], i == currProcess->cmd->argCount - 1 ? "\n" : " ");

        // first process in the list was terminated
        if (currProcess->status == TERMINATED && !prevProcess)
        {
            (*process_list) = currProcess->next;
            freeCmdLines(currProcess->cmd);
            prevProcess = currProcess;
            currProcess = currProcess->next;
            free(prevProcess);
            prevProcess = NULL;
        }
        // non first process in the list was terminated
        else if (currProcess->status == TERMINATED && prevProcess)
        {
            prevProcess->next = currProcess->next;
            freeCmdLines(currProcess->cmd);
            free(currProcess);
            currProcess = prevProcess->next;
        }
        // this process wasn't terminated
        else
        {
            prevProcess = currProcess;
            currProcess = currProcess->next;
        }
        index++;
    }
}

void printStatus(int status)
{
    printf("%s      ", status == -1 ? "Terminated" : status == 1 ? "Running"
                                                                 : "Suspended");
}

void freeHistory()
{
    for (int i = 0; i < HISTLEN; i++)
        if (history[i])
            free(history[i]);
}

int printHistory()
{
    for (int i = 0; i < HISTLEN; i++)
    {
        int currIndex = (i + oldestIndex) % HISTLEN;
        if (history[currIndex])
            printf("%d %s", i + 1, history[currIndex]);
    }

    return 1;
}

void addHistory(char *cmdLine)
{
    history[newestIndex] = cmdLine;
    newestIndex = (newestIndex + 1) % HISTLEN;

    if (newestIndex == oldestIndex)
        oldestIndex = (oldestIndex + 1) % HISTLEN;
}

int executeLastCommand()
{
    cmdLine *cmdLine;
    printf("test\n");

    if (!history[(newestIndex - 1) % HISTLEN])
    {
        printf("History Error: There are no previous commands.\n");
        return 1;
    }
    if ((cmdLine = parseCmdLines(history[(newestIndex - 1) % HISTLEN])) == NULL)
        error("Parsing Error", 0);

    execute(cmdLine);
    return 1;
}

int executeNthCommand(char *n)
{
    cmdLine *cmdLine;
    int number = atoi(n);

    if (1 > number || number > HISTLEN)
    {
        printf("History Error: Command number %s does not exist.\n", n);
        return 1;
    }
    if (!history[(oldestIndex + number - 1) % HISTLEN])
    {
        printf("History Error: Could not find command number %s.\n", n);
        return 1;
    }
    if ((cmdLine = parseCmdLines(history[(oldestIndex + number - 1) % HISTLEN])) == NULL)
        error("Parsing Error", 0);

    if (strcmp(history[(newestIndex - 1) % HISTLEN], history[(oldestIndex + number - 1) % HISTLEN]) != 0)
        addHistory(history[(oldestIndex + number - 1) % HISTLEN]);

    execute(cmdLine);
    return 1;
}