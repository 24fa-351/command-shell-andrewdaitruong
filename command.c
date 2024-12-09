#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_LINE 1000
#define MAX_TOKENS 100

// Function prototypes
void execute_pipeline(char *commands[], int num_commands, char *input_file, char *output_file, int append, int background);
int parse_line(char *line, char **commands, char **input_file, char **output_file, int *append, int *background);

int main() {
    char line[MAX_LINE];

    while (1) {
        printf("xsh> ");
        if (!fgets(line, MAX_LINE, stdin)) {
            perror("Error reading input");
            continue;
        }

        // Remove the newline character at the end of the line
        line[strcspn(line, "\n")] = 0;

        // Exit command
        if (!strcmp(line, "exit") || !strcmp(line, "quit")) {
            break;
        }

        // Parse the line
        char *commands[MAX_TOKENS] = {NULL};
        char *input_file = NULL;
        char *output_file = NULL;
        int append = 0;
        int background = 0;

        int num_commands = parse_line(line, commands, &input_file, &output_file, &append, &background);
        if (num_commands > 0) {
            execute_pipeline(commands, num_commands, input_file, output_file, append, background);
        }
    }

    return 0;
}

// Parse the input line based on EBNF
int parse_line(char *line, char **commands, char **input_file, char **output_file, int *append, int *background) {
    int num_commands = 0;
    char *token = strtok(line, " ");
    int parsing_command = 1;

    while (token != NULL) {
        if (strcmp(token, "|") == 0) {
            parsing_command = 1; // Start a new command
        } else if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " ");
            if (token == NULL) {
                fprintf(stderr, "Syntax error: expected file after '<'\n");
                return 0;
            }
            *input_file = token;
        } else if (strcmp(token, ">") == 0 || strcmp(token, ">>") == 0) {
            *append = (strcmp(token, ">>") == 0);
            token = strtok(NULL, " ");
            if (token == NULL) {
                fprintf(stderr, "Syntax error: expected file after '>' or '>>'\n");
                return 0;
            }
            *output_file = token;
        } else if (strcmp(token, "&") == 0) {
            *background = 1;
        } else {
            if (parsing_command) {
                commands[num_commands++] = token;
                parsing_command = 0;
            } else {
                strcat(commands[num_commands - 1], " ");
                strcat(commands[num_commands - 1], token);
            }
        }
        token = strtok(NULL, " ");
    }

    return num_commands;
}

// Execute commands based on parsing
void execute_pipeline(char *commands[], int num_commands, char *input_file, char *output_file, int append, int background) {
    int pipes[MAX_TOKENS][2];
    pid_t pids[MAX_TOKENS];
    int fd_in = STDIN_FILENO;
    int fd_out = STDOUT_FILENO;

    // Create pipes for all commands
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("Pipe failed");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();
        if (pids[i] == 0) { // Child process
            // Input redirection
            if (i == 0 && input_file) {
                fd_in = open(input_file, O_RDONLY);
                if (fd_in < 0) {
                    perror("Error opening input file");
                    exit(EXIT_FAILURE);
                }
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }

            // Output redirection
            if (i == num_commands - 1 && output_file) {
                fd_out = open(output_file, O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC), 0644);
                if (fd_out < 0) {
                    perror("Error opening output file");
                    exit(EXIT_FAILURE);
                }
                dup2(fd_out, STDOUT_FILENO);
                close(fd_out);
            }

            char *args[MAX_TOKENS] = {NULL};
            int arg_count = 0;

            // Tokenize command
            char *token = strtok(commands[i], " ");
            while (token != NULL) {
                args[arg_count++] = token;
                token = strtok(NULL, " ");
            }
            args[arg_count] = NULL;

            execvp(args[0], args); // Search for command in PATH
            perror("Error executing command");
            exit(EXIT_FAILURE);
        } else if (pids[i] < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }

        // Close pipe ends in parent process
        if (i > 0) {
            close(pipes[i - 1][0]);
        }
        if (i < num_commands - 1) {
            close(pipes[i][1]);
        }
    }

    // Wait for child processes
    if (!background) {
        for (int i = 0; i < num_commands; i++) {
            waitpid(pids[i], NULL, 0);
        }
    }
}
