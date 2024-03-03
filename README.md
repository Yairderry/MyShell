# Shell Project

## Overview

This project focuses on developing a simple shell program in C with enhanced features, including job control, pipes, history mechanism, and process management. The shell is designed to mimic the behavior of common Unix-like shells while offering additional functionalities for better user experience and control.

## Prerequisites

Before building and running the shell, ensure you have the following prerequisites:

- Basic knowledge of C programming language.
- A Unix-like operating system environment (Linux preferred).
- GCC compiler installed for compiling C code.

## Building and Running the Shell

To build and run the shell, follow these steps:

1. Clone or download the project repository to your local machine.

2. Navigate to the project directory in your terminal.

3. Compile the shell program using the provided `makefile`:

This will compile the source code files `myshell.c` into executable named  `myshell`.

4. Run the shell program:

`./myshell`

This will start the shell, allowing you to enter commands and utilize its features.

## Features

### 1. Command Execution
- Execute commands entered by the user.
- Support execution of external programs and built-in shell commands.

### 2. Input/Output Redirection
- Redirect standard input and output streams using `<`, `>`, and `>>` operators.

### 3. Pipes
- Support pipelines, allowing the output of one command to be passed as input to another.

### 4. History Mechanism
- Maintain a history of previous command lines.
- Support commands like `history`, `!!`, and `!n` for accessing and re-executing command history.

### 5. Process Management
- Manage running and suspended processes within the shell.
- Provide operations such as listing processes, suspending/resuming processes, and terminating processes.

## Implementation Overview

The shell project is implemented in C language and consists of multiple components:

- **Shell Core**: Implemented in `myshell.c`, it handles command parsing, execution, and interaction with the user.

- **Pipelines**: The pipeline functionality is implemented based on the concept of creating child processes and establishing communication between them using pipes.

- **History Mechanism**: The history feature is implemented using a circular queue data structure to store previous command lines. Commands like `history`, `!!`, and `!n` are parsed and executed accordingly.

- **Process Management**: The process manager maintains a linked list of running and suspended processes. It provides functions to add, update, and print process information.

- **Makefile**: The provided `makefile` simplifies the compilation and linking process, allowing easy building of the shell program.

## Conclusion

The shell project offers a comprehensive solution for command line interaction and process management in a Unix-like environment. With its intuitive interface and enhanced features, it provides users with a powerful tool for executing commands, managing processes, and improving productivity.
