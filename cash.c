#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <termios.h>

#define LINE_LENGTH 128

#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
// #define ANSI_COLOR_MAGENTA "\x1b[35m"
// #define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_GRAY    "\x1b[90m"

#define m_verbose(...)                      \
    if (verbose == true) {                  \
        printf("%s", ANSI_COLOR_GRAY);      \
        printf(__VA_ARGS__);                \
        printf("%s", ANSI_COLOR_RESET);     \
    }
//#define m_error(...)                        \
//    fprintf(stderr, "%s", ANSI_COLOR_RED);  \
//    fprintf(stderr, __VA_ARGS__);           \
//    fprintf(stderr, "%s", ANSI_COLOR_RESET);

#define m_error(...)                        \
    fprintf(stdout, "%s", ANSI_COLOR_RED);  \
    fprintf(stdout, __VA_ARGS__);           \
    fprintf(stdout, "%s", ANSI_COLOR_RESET);

// structures
struct key_value {
    char *key;
    char *value;
};

// function prototypes
void execute_command(char *cmd, char *arg[], char *env[]);
char *search_path(char *cmd, char *path[]);
// int tokenize(char *);
void tokenize(char *);
void set_raw_mode(struct termios *);
void reset_terminal_mode(struct termios *);
int get_command(char *);

// global variables
bool verbose = true;
char gpath[128]; // hold token strings
char *input_token[64];  // token string pointers
int n;           // number of token strings
char dpath[128]; // hold dir strings in PATH
char *dir[64];   // dir string pointers
int ndir;        // number of dirs

struct key_value *envs[256];
char *paths[64];


int main(int argc, char *argv[], char *env[]) {
    int i;
    int pid, status;
    char *cmd;
    char line[LINE_LENGTH];  // input buffer
    printf("%s", ANSI_COLOR_RESET);
    char temp;
    int path_envs_index = -1;
    int home_envs_index = -1;
    char *next_path;

    // arguments
    m_verbose("%s\n", "arguments:");
    m_verbose("argc: %d\n", argc);
    for (i = 0; i < argc; i++) {
        m_verbose("argv[%d]: %s\n", i, argv[i]);
    }

    // environment variables
    m_verbose("\n%s\n", "environment variables:");
    i = 0;
    while (true) {
        if (env[i] == NULL) {
            break;
        }

        envs[i] = malloc(sizeof(struct key_value));
        envs[i]->key = strtok(env[i], "=");
        envs[i]->value = strtok(NULL, "=");
        m_verbose("env[%d]: %s = %s\n", i, envs[i]->key, envs[i]->value);

        if (strcmp(envs[i]->key, "PATH") == 0) {
            path_envs_index = i;
        } else if (strcmp(envs[i]->key, "HOME") == 0) {
            home_envs_index = i;
        }

        i++;
    }

    // paths
    m_verbose("\n%s\n", "paths:");
    i = 0;
    if (path_envs_index != -1) {
        next_path = strtok(envs[path_envs_index]->value, ":");
        while (next_path != NULL) {
            m_verbose("path[%d]: %s\n", i, next_path);
            paths[i] = next_path;
            next_path = strtok(NULL, ":");
            i++;
        }
        paths[i] = NULL;
    } else {
        m_verbose("%s\n", "PATH (environment variable) not found");
    }

    // home directory
    m_verbose("\n%s", "home directory: ");
    if (home_envs_index != -1) {
        m_verbose("%s\n", envs[home_envs_index]->value);
    } else {
        m_verbose("%s\n", "HOME (environment variable) not found");
    }

    printf("%s", "\n");

    // cli loop
    while (true) {
        m_verbose("Cash %d running\n", getpid());
        printf("%s", ANSI_COLOR_GREEN);
        fflush(stdout);
        printf("$ "); // cat file1 file2
        printf("%s", ANSI_COLOR_RESET);
        fflush(stdout);
        // ls -l -a -f
        // cat file | grep print
        // ANY valid Linux command line

        // fgets(line, LINE_LENGTH, stdin);
        get_command(line);
        m_verbose("%s", line);

        line[strlen(line) - 1] = 0; // kill \n at end
        if (line[0] == 0) // if (strcmp(line, "")==0) // if line is NULL
            continue;
        tokenize(line); // divide line into token strings
        for (i = 0; i < n; i++) { // show token strings
            m_verbose("input_token[%d] = %s\n", i, input_token[i]);
        }
        cmd = input_token[0]; // line = name0 name1 name2 ....
        if (strcmp(cmd, "cd") == 0) {
            chdir(input_token[1]);
            continue;
        }
        if (strcmp(cmd, "exit") == 0) {
            break;
            // exit(0);
        }
        pid = fork();
        if (pid > 0) {
            m_verbose("Cash %d forked a child sh %d\n", getpid(), pid);
            m_verbose("Cash %d wait for child sh %d to terminate\n", getpid(), pid);
            pid = wait(&status);
            m_verbose("ZOMBIE child=%d exitStatus=%x\n", pid, status);
            m_verbose("Cash %d repeat loop\n", getpid());
        } else {
            execute_command(cmd, input_token, env);
        }
    }

    return 0;
}

//int tokenize(char *pathname) {
//    char *s;
//    strcpy(gpath, pathname); // copy into global gpath[]
//    s = strtok(gpath, " ");
//    n = 0;
//    while (s != NULL) {
//        input_token[n++] = s; // token string pointers
//        s = strtok(NULL, " ");
//    }
//    input_token[n] = NULL;
//    return 0;
//}

void tokenize(char *pathname) {
    char *s;
    int in_quotes = 0;
    int token_start = 0;
    n = 0;
    int length = strlen(pathname);

    for (int i = 0; i <= length; i++) {
        if (pathname[i] == '"') {
            in_quotes = !in_quotes;
            if (in_quotes) {
                token_start = i + 1;
            } else {
                pathname[i] = '\0'; // Terminate the token before the closing quote
                input_token[n++] = &pathname[token_start];
                token_start = i + 1;
            }
        } else if ((pathname[i] == ' ' && !in_quotes) || pathname[i] == '\0') {
            if (i > token_start) {
                pathname[i] = '\0'; // Terminate the current token
                input_token[n++] = &pathname[token_start];
            }
            token_start = i + 1;
        }
    }
    input_token[n] = NULL; // Null-terminate the array of tokens
}

void execute_command(char *cmd, char *arg[], char *env[]) {
    m_verbose("child sh %d running\n", getpid());
    char *full_cmd_path = search_path(cmd, paths);
    if (full_cmd_path == NULL) {
        m_error("command not found: %s\n", cmd);
        exit(1);
    }
    m_verbose("full_cmd_path = %s\n", full_cmd_path);

    // Identify pipes
    int pipe_count = 0;
    int pipe_indices[64]; // Array to hold positions of pipes in arg
    int append_redirect = -1;  // >>
    int out_redirect = -1;  // >
    int in_redirect = -1;  // <

    // Identify pipes and redirections
    for (int i = 0; arg[i] != NULL; i++) {
        if (strcmp(arg[i], "|") == 0) {
            pipe_indices[pipe_count++] = i;
            arg[i] = NULL; // Split the arguments at the pipe
        } else if (strcmp(arg[i], ">>") == 0) {
            append_redirect = i;
        } else if (strcmp(arg[i], ">") == 0) {
            out_redirect = i;
        } else if (strcmp(arg[i], "<") == 0) {
            in_redirect = i;
        }
    }

    int num_commands = pipe_count + 1;
    int pipe_fds[pipe_count][2]; // File descriptors for pipes

    // Create pipes
    for (int i = 0; i < pipe_count; i++) {
        if (pipe(pipe_fds[i]) == -1) {
            m_error("pipe creation failed\n");
            exit(1);
        }
    }

    for (int i = 0; i < num_commands; i++) {
        pid_t pipe_pid = fork();
        if (pipe_pid == -1) {
            m_error("fork for pipe failed\n");
            exit(1);
        }

        if (pipe_pid == 0) {
            // Child process
            if (i > 0) {
                // If not the first command, get input from the previous pipe
                dup2(pipe_fds[i - 1][0], STDIN_FILENO);
            }
            if (i < pipe_count) {
                // If not the last command, output to the next pipe
                dup2(pipe_fds[i][1], STDOUT_FILENO);
            }

            for (int j = 0; j < pipe_count; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }

            // Handle redirections in the last command
            if (i == num_commands - 1) {
                if (out_redirect != -1) {
                    int fd = open(arg[out_redirect + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd == -1) {
                        m_error("failed to open output file\n");
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    arg[out_redirect] = NULL;
                }

                // Handle append redirection
                if (append_redirect != -1) {
                    int fd = open(arg[append_redirect + 1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd == -1) {
                        m_error("failed to open output file\n");
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    arg[append_redirect] = NULL;
                }
            }

            // Handle input redirection
            if (in_redirect != -1 && i == 0) {
                int fd = open(arg[in_redirect + 1], O_RDONLY);
                if (fd == -1) {
                    m_error("failed to open input file\n");
                    exit(1);
                }
                dup2(fd, STDIN_FILENO);
                arg[in_redirect] = NULL;
            }

            printf("%s", ANSI_COLOR_RESET);
            fflush(stdout);
            execve(
                search_path(
                    arg[i == 0 ? 0 : pipe_indices[i - 1] + 1],
                    paths
                ),
                &arg[i == 0 ? 0 : pipe_indices[i - 1] + 1],
                env
            );
            m_error("execve failed for command %d\n", i);
            exit(1);
        }
    }

    for (int i = 0; i < pipe_count; i++) {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }

    for (int i = 0; i < num_commands; i++) {
        wait(NULL); // Wait for all child processes
    }

    exit(0);
}

char *search_path(char *cmd, char *path[]) {
    static char full_path[256];
    if (cmd[0] == '/' || cmd[0] == '.') {
        // Absolute path or relative path
        return cmd;
    }
    for (int i = 0; path[i] != NULL; i++) {
        snprintf(full_path, sizeof(full_path), "%s/%s", path[i], cmd);
        if (access(full_path, X_OK) == 0) {
            return full_path;
        }
    }
    return NULL;
}

// Function to set the terminal to raw mode
void set_raw_mode(struct termios *original_termios) {
    struct termios raw;

    // Get the current terminal settings
    tcgetattr(STDIN_FILENO, original_termios);

    // Copy the settings to modify them
    raw = *original_termios;

    // Modify the settings for raw mode
    // reference: https://www.ibm.com/docs/zh-tw/aix/7.3?topic=files-termiosh-file
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    // Apply the raw mode settings
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Function to reset the terminal to its original mode
void reset_terminal_mode(struct termios *original_termios) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, original_termios);
}

int insert_char(char *line, int pos, char c) {
    if (pos >= LINE_LENGTH - 1) {
        return -1; // Prevent buffer overflow
    }
    for (int i = LINE_LENGTH - 2; i > pos; i--) {
        line[i] = line[i - 1];
    }
    line[pos] = c;
    return 0;
}

char pop_char(char *line, int pos) {
    char out = line[pos];
    for (int i = pos; line[i] != '\0'; i++) {
        line[i] = line[i + 1];
    }
    return out;
}

int get_command(char *line) {
    fflush(stdout);
    struct termios original_termios;
    set_raw_mode(&original_termios);

    int index = 0;
    line[0] = '\0';
    char c;
    bool in_quotes = false;

    while (true) {
        read(STDIN_FILENO, &c, 1);
        if (c == '\x1b') { // Escape sequence
            char seq[3];
            if (read(STDIN_FILENO, &seq[0], 1) == 0) break;
            if (read(STDIN_FILENO, &seq[1], 1) == 0) break;

            if (seq[0] == '[') {
                if (seq[1] == 'A') {
                    // up arrow ↑
                    m_verbose("↑");
                    // TODO: previous history command
                } else if (seq[1] == 'B') {
                    // down arrow ↓
                    m_verbose("↓");
                    // TODO: next history command
                } else if (seq[1] == 'C') {
                    // right arrow →
                    if (line[index] != '\0') {
                        printf("%c", line[index]);
                        index++;
                    }
                } else if (seq[1] == 'D') {
                    // left arrow ←
                    if (index > 0) {
                        printf("\b");
                        index--;
                    }
                } else if (seq[1] == 'H') {
                    // home
                    while (index > 0) {
                        printf("\b");
                        index--;
                    }
                } else if (seq[1] == 'F') {
                    // end
                    while (line[index] != '\0') {
                        printf("%c", line[index]);
                        index++;
                    }
                } else {
                    if (read(STDIN_FILENO, &seq[2], 1) == 0) break;

                    if (seq[1] == '3' && seq[2] == '~') {
                        // delete
                        // TODO: delete character
                    }
                }
            }
        } else if (c == '\x7f') {
            // backspace
            if (index > 0) {
                index--;
                pop_char(line, index);
                printf("\b%s \b", &line[index]);
                for (int i = index; line[i] != '\0'; i++) {
                    printf("\b");
                }
            }
        } else if (c == '\r') {
            // enter
            if (in_quotes) {
                continue; // Ignore enter key if inside quotes
            }
            insert_char(line, index, '\n');
            insert_char(line, index + 1, '\0');
            // index++;
            printf("%s", "\n\r");
            break;
        } else if (c == '\t') {
            // tab
            // TODO: autocomplete
        } else if (c == '"') {
            // quote
            in_quotes = !in_quotes; // Toggle in_quotes flag
            // below is the same as regular character
            insert_char(line, index, c);
            index++;
            printf("%s", &line[index - 1]);
            for (int i = index; line[i] != '\0'; i++) {
                printf("\b");
            }
        } else {
            // others (regular character)
            insert_char(line, index, c);
            index++;
            printf("%s", &line[index - 1]);
            for (int i = index; line[i] != '\0'; i++) {
                printf("\b");
            }
        }
        fflush(stdout);
    }

    // Reset the terminal to its original mode
    reset_terminal_mode(&original_termios);
    return 0;
}
