#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define HIST_MAX 100

static char *hist_buf[HIST_MAX];   
static int   hist_count = 0;      

static void hist_add(const char *line) {
    int slot = hist_count % HIST_MAX;
    free(hist_buf[slot]);
    hist_buf[slot] = strdup(line);
    hist_count++;
}


int lsh_cd      (char **args);
int lsh_help    (char **args);
int lsh_exit    (char **args);
int lsh_pwd     (char **args);
int lsh_echo    (char **args);
int lsh_history (char **args);
int lsh_env     (char **args);


char *builtin_str[] = {
    "cd",
    "help",
    "exit",
    "pwd",
    "echo",
    "history",
    "env"
};

int (*builtin_func[])(char **) = {
    &lsh_cd,
    &lsh_help,
    &lsh_exit,
    &lsh_pwd,
    &lsh_echo,
    &lsh_history,
    &lsh_env
};

int lsh_num_builtins(void) {
    return sizeof(builtin_str) / sizeof(char *);
}


int lsh_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "lsh: cd: expected 1 argument\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("lsh: cd");
        }
    }
    return 1;
}


int lsh_help(char **args) {
    (void)args;
    printf("\nLSH — Little Shell\n");
    printf("Type a program name and arguments, then press Enter.\n");
    printf("Built-in commands:\n\n");
    for (int i = 0; i < lsh_num_builtins(); i++) {
        printf("  %s\n", builtin_str[i]);
    }
    printf("\nUse 'man <command>' for help on external programs.\n\n");
    return 1;
}


int lsh_exit(char **args) {
    (void)args;
    return 0;
}


int lsh_pwd(char **args) {
    (void)args;
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        perror("lsh: pwd");
    } else {
        printf("%s\n", buf);
    }
    return 1;
}


int lsh_echo(char **args) {
    for (int i = 1; args[i] != NULL; i++) {
        printf("%s", args[i]);
        if (args[i + 1] != NULL) printf(" ");
    }
    printf("\n");
    return 1;
}


int lsh_history(char **args) {
    int show = hist_count;  

    if (args[1] != NULL) {
        show = atoi(args[1]);
        if (show <= 0) {
            fprintf(stderr, "lsh: history: argument must be a positive number\n");
            return 1;
        }
        if (show > hist_count) show = hist_count;
    }

    if (hist_count == 0) {
        printf("(no commands in history yet)\n");
        return 1;
    }

    int oldest = hist_count - show;
    for (int i = 0; i < show; i++) {
        int slot = (oldest + i) % HIST_MAX;
        printf("  %4d  %s\n", oldest + i + 1, hist_buf[slot]);
    }
    return 1;
}


extern char **environ;

int lsh_env(char **args) {
    (void)args;
    if (environ == NULL) {
        printf("(empty environment)\n");
        return 1;
    }
    for (int i = 0; environ[i] != NULL; i++) {
        printf("%s\n", environ[i]);
    }
    return 1;
}


int lsh_launch(char **args) {
    pid_t pid;
    int   status;

    pid = fork();

    if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            perror("lsh");
        }
        exit(EXIT_FAILURE);

    } else if (pid < 0) {
        perror("lsh");

    } else {

        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

 
int lsh_execute(char **args) {
    if (args[0] == NULL) return 1;  

    for (int i = 0; i < lsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    return lsh_launch(args);
}


char *lsh_read_line(void) {
    char   *line    = NULL;
    size_t  bufsize = 0;

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            printf("\n");
            exit(EXIT_SUCCESS);   
        } else {
            perror("lsh: readline");
            exit(EXIT_FAILURE);
        }
    }

    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

    return line;
}


#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM   " \t\r\n\a"

char **lsh_split_line(char *line) {
    int    bufsize = LSH_TOK_BUFSIZE;
    int    pos     = 0;
    char **tokens  = malloc(bufsize * sizeof(char *));

    if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    char *token = strtok(line, LSH_TOK_DELIM);
    while (token != NULL) {
        tokens[pos++] = token;
        if (pos >= bufsize) {
            bufsize += LSH_TOK_BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens) {
                fprintf(stderr, "lsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, LSH_TOK_DELIM);
    }
    tokens[pos] = NULL;
    return tokens;
}


void lsh_loop(void) {
    char  *line;
    char **args;
    int    status;

    for (int i = 0; i < HIST_MAX; i++) hist_buf[i] = NULL;

    do {
        printf("lsh> ");
        fflush(stdout);

        line = lsh_read_line();

        if (line[0] != '\0') hist_add(line);

        args   = lsh_split_line(line);
        status = lsh_execute(args);

        free(line);
        free(args);

    } while (status);

    for (int i = 0; i < HIST_MAX; i++) free(hist_buf[i]);
}


int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    lsh_loop();
    return EXIT_SUCCESS;
}
