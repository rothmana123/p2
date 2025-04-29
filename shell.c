#include <fcntl.h>
#include <locale.h>
#include <pwd.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "history.h"
#include "logger.h"

#define MAX_TOKENS 128
#define HISTORY_LIMIT 100

char *next_token(char **str_ptr, const char *delim) {
    if (*str_ptr == NULL) {
        return NULL;
    }

    size_t tok_start = strspn(*str_ptr, delim);
    size_t tok_end = strcspn(*str_ptr + tok_start, delim);

    if (tok_end == 0) {
        *str_ptr = NULL;
        return NULL;
    }

    char *current_ptr = *str_ptr + tok_start;
    *str_ptr += tok_start + tok_end;

    if (**str_ptr == '\0') {
        *str_ptr = NULL;
    } else {
        **str_ptr = '\0';
        (*str_ptr)++;
    }

    return current_ptr;
}

void tokenize_input(char *input, char **tokens, int max_tokens) {
    char *next = input;
    int count = 0;
    char *tok;

    while ((tok = next_token(&next, " \t")) != NULL && count < max_tokens - 1) {
        tokens[count++] = tok;
    }

    tokens[count] = NULL;
}

int readline_init(void) {
    rl_variable_bind("show-all-if-ambiguous", "on");
    rl_variable_bind("colored-completion-prefix", "on");
    return 0;
}

int main(void) {
    rl_startup_hook = readline_init;
    int command_num = 1;
    int last_exit_status = 0;
    bool interactive = isatty(STDIN_FILENO);

    hist_init(HISTORY_LIMIT);  //  initialize history

    signal(SIGINT, SIG_IGN);

    if (interactive) {
        printf("Interactive mode enabled\n");
    } else {
        printf("Scripting mode enabled\n");
    }

    char *command = NULL;
    size_t len = 0;

    while (true) {
        //1. read input
        if (interactive) {
            char prompt[256];
            char *status_icon = (last_exit_status == 0) ? "🙂" : "🤮"; //change to normal if/else

            char cwd[PATH_MAX];

            //does this get current directory?
            getcwd(cwd, sizeof(cwd));
            snprintf(prompt, sizeof(prompt), "[%s]─[%d]─[%s]$ ", status_icon, command_num, cwd);

            //how does readLine work (vs getLine?)
            command = readline(prompt);
            if (command == NULL) {
                printf("\n");
                break;
            }
        } else {
            ssize_t read = getline(&command, &len, stdin);
            if (read == -1) {
                break;
            }
            if (command[read - 1] == '\n') {
                command[read - 1] = '\0';
            }
        }

        // 2. Skip empty input
        if (strlen(command) == 0) {
            free(command);
            continue;
        }

        //*Add command to history
        // ✅ At this point, 'command' is the full, real command to store:
        if (command[0] != '\0' && command[0] != '!') {
            printf("📦 [main] Sending to hist_add: \"%s\"\n", command);  // 🧪 Debug print
            hist_add(command);   // ✅ Save full command line
        }

        //LOG("Input command: %s\n", command);

        // 3. Tokenize input
        char *tokens[MAX_TOKENS];
        tokenize_input(command, tokens, MAX_TOKENS);

        //FYI previously had built-in commands here, but history wouldnt work so I moved it after "!" command

        // 4. check history expansion first wit --- Bang (! command) ---
        if (tokens[0][0] == '!') {
            const char *replacement = NULL;

            if (strcmp(tokens[0], "!!") == 0) {
                if (hist_last_cnum() == 0) {
                    printf("No commands in history.\n");
                    if (interactive) free(command);
                    continue;
                }
                replacement = hist_search_cnum(hist_last_cnum());
                //what is replacement
            } else if (isdigit(tokens[0][1])) {
                //what is going on here??
                int num = atoi(&tokens[0][1]);
                replacement = hist_search_cnum(num);
                if (!replacement) {
                    printf("No such command number: %d\n", num);
                    if (interactive) free(command);
                    continue;
                }
            } else {
                replacement = hist_search_prefix(&tokens[0][1]);
                if (!replacement) {
                    printf("No command starts with '%s'\n", &tokens[0][1]);
                    if (interactive) free(command);
                    continue;
                }
            }

            printf("%s\n", replacement); // Show user the expanded command
            free(command);
            command = strdup(replacement);
            // TESTING- NOW ADD TO HISTORY
            if (command[0] != '\0' && command[0] != '!') {
                printf("🧠 storing expanded command: \"%s\"\n", command);
                hist_add(command);
            }
            tokenize_input(command, tokens, MAX_TOKENS);
        }
        //*where is the best place to add to history??
         // // Add to history after any ! expansion
        // if (command[0] != '\0' && command[0] != '!') {
        //     hist_add(command);
        // }

        // 5. check built0in cases - where are these added to history?
        // --- Built-in: skip comments ---
        if (tokens[0][0] == '#') {
            if (interactive) free(command); //normal if/else
            continue;
        }

        // --- Built-in: exit ---
        if (strcmp(tokens[0], "exit") == 0) {
            if (interactive) free(command);  //normal if/else
            break; //exits shell
        }

        // --- Built-in: cd ---
        if (strcmp(tokens[0], "cd") == 0) {
            const char *path = tokens[1];
            if (path == NULL) {
                path = getenv("HOME");
            }
            if (chdir(path) != 0) {
                perror("chdir failed");
            }
            if (interactive) free(command);
            continue;
        }

        // --- Built-in: history ---
        if (strcmp(tokens[0], "history") == 0) {
            hist_print();
            if (interactive) free(command);
            continue;
        }

       
        //previous forking code was here
        
        //6. Check for pipes
        
        // Step 1: Split into pipeline segments
        char **segments[MAX_TOKENS];
        int segment_count = 0;
        segments[segment_count++] = tokens;  // first segment
        
        for (int i = 0; tokens[i] != NULL; i++) {
            if (strcmp(tokens[i], "|") == 0) {
                tokens[i] = NULL;  // cut here
                segments[segment_count++] = &tokens[i + 1];  // next segment starts after |
            }
        }
        //segments[segment_count++] = tokens;  // first segment

        int pipefd[2];
        int prev_read = -1;

        for (int i = 0; i < segment_count; i++) {
            if (i < segment_count - 1) {  // Not last command: set up pipe
                if (pipe(pipefd) < 0) {
                    perror("pipe failed");
                    break;
                }
            }

            pid_t pid = fork();
            if (pid == 0) {
                signal(SIGINT, SIG_DFL);  // child restores default behavior

                // --- Child process ---

                if (prev_read != -1) {
                    dup2(prev_read, STDIN_FILENO);
                    close(prev_read);
                }
                if (i < segment_count - 1) {
                    close(pipefd[0]); // close read end
                    dup2(pipefd[1], STDOUT_FILENO);
                    close(pipefd[1]);
                }

                // --- Handle redirection inside final command ---
                for (int j = 0; segments[i][j] != NULL; j++) {
                    if (strcmp(segments[i][j], ">") == 0 || strcmp(segments[i][j], ">>") == 0) {
                        int flags = O_CREAT | O_WRONLY | (strcmp(segments[i][j], ">>") == 0 ? O_APPEND : O_TRUNC);
                        int fd = open(segments[i][j + 1], flags, 0666);
                        if (fd < 0) {
                            perror("open output file");
                            exit(1);
                        }
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                        segments[i][j] = NULL;
                        break;
                    } else if (strcmp(segments[i][j], "<") == 0) {
                        int fd = open(segments[i][j + 1], O_RDONLY);
                        if (fd < 0) {
                            perror("open input file");
                            exit(1);
                        }
                        dup2(fd, STDIN_FILENO);
                        close(fd);
                        segments[i][j] = NULL;
                        break;
                    }
                }
                signal(SIGINT, SIG_DFL);


                execvp(segments[i][0], segments[i]);
                perror("execvp failed");
                exit(1);
            }
            else if (pid > 0) {
                // --- Parent process ---
                if (prev_read != -1) close(prev_read);
                if (i < segment_count - 1) {
                    close(pipefd[1]);  // close write end
                    prev_read = pipefd[0]; // next child reads from here
                }
            }
        
            else {
                perror("fork failed");
                exit(1);
            }
        }

        // After launching all processes, parent waits
        for (int i = 0; i < segment_count; i++) {
            int status;
            wait(&status);
        }

        if (interactive) {
            free(command);
            command = NULL;
        }

        command_num++;
        continue;
    }


    if (!interactive) {
        free(command);
    }

    hist_destroy();  // free memory
    return 0;
}

    //cc -Wall -Wextra -o hboy1 hboy1.c history.c -lreadline

    //scripting test:
    /*cat <<EOM | ./hboy1
    ls
    pwd
    echo "hello"
    exit
    EOM
    */

    