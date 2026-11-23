#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "linenoise/linenoise.h"

struct ArgsVec {
    size_t size;
    size_t capacity;
    char **arr;
};

int construct_argsvec(struct ArgsVec **out_vec) {
    if (out_vec == NULL) {
        return 0;
    }

    struct ArgsVec *vec = malloc(sizeof(*vec));
    if (vec == NULL) {
        return 0;
    }

    vec->capacity = 2;
    vec->size = 0;
    vec->arr = malloc(vec->capacity * sizeof(*vec->arr));
    if (vec->arr == NULL) {
        free(vec);
        return 0;
    }

    vec->arr[0] = NULL;
    *out_vec = vec;
    return 1;
}

void destruct_argsvec(struct ArgsVec **vec_ptr) {
    if (vec_ptr == NULL || *vec_ptr == NULL) {
        return;
    }

    struct ArgsVec *vec = *vec_ptr;

    if (vec->arr != NULL) {
        for (size_t i = 0; i < vec->size; i++) {
            free(vec->arr[i]);
        }
        free(vec->arr);
    }

    free(vec);
    *vec_ptr = NULL;
}

int vec_push_back(struct ArgsVec *vec, const char *t) {
    if (vec == NULL || t == NULL) {
        return 0;
    }

    // Reserve 1 extra slot so vec->arr[vec->size] can always be NULL
    if (vec->size + 1 >= vec->capacity) {
        size_t new_cap = vec->capacity << 1;
        char **new_arr = malloc(new_cap * sizeof(*vec->arr));
        if (new_arr == NULL) {
            return 0;
        }

        memcpy(new_arr, vec->arr, vec->size * sizeof(*vec->arr));
        free(vec->arr);
        vec->arr = new_arr;
        vec->capacity = new_cap;
    }

    char *copy = strdup(t);
    if (copy == NULL) {
        return 0;
    }

    vec->arr[vec->size] = copy;
    vec->size += 1;
    vec->arr[vec->size] = NULL; // Keep array NULL-terminated for execv
    return 1;
}

int is_home_path(const char *str) {
    if (str == NULL) {
        return 0;
    }

    return (strcmp(str, "~") == 0); 
}


int is_home_prefix(const char *str) {
    if (str == NULL) {
        return 0;
    }

    return (strncmp(str, "~/", 2) == 0);
}

int cd_cmd(const struct ArgsVec *vec) {
    
    const char *home = getenv("HOME");
    char path_buf[PATH_MAX];
    const char *path;
    
    if (vec->size <= 1 || is_home_path(vec->arr[1])) {
        path = home;
    }
    else if (is_home_prefix(vec->arr[1])) {
        if (home == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
        
        int n = snprintf(path_buf, sizeof(path_buf), "%s%s", home, vec->arr[1] + 1);
        
        if (n < 0 || (size_t)n >= sizeof(path_buf)) {
            fprintf(stderr, "cd: path too long\n");
            return 1;
        } 
        path = path_buf;
    }
    else {
        path = vec->arr[1];
    }

    if (path == NULL) {
        fprintf(stderr, "cd: HOME not set\n");
        return 1;
    }
    
    if (chdir(path) != 0) {
        fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
        return 1;
    }

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        setenv("PWD", cwd, 1);
    }
    return 0;
}

void status_cmd(const struct ArgsVec *vec, const int valid_raw, const int status, const int last_status) {
    if (vec->size > 1) {
        if (strcmp(vec->arr[1], "raw") == 0){
            if (valid_raw) {
                printf("%d\n", status);
            } else {
                printf("Raw status is invalid\n");
            }
        }
        else if (strcmp(vec->arr[1], "exit") == 0) {
            if (!valid_raw) {
                printf("%d\n", last_status);
            }
            else if (WIFEXITED(status)) {
                printf("%d\n", WEXITSTATUS(status));
            } else {
                printf("NULL\n");
            }
        } 
        else if (strcmp(vec->arr[1], "signal") == 0) {
            if (!valid_raw || !WIFSIGNALED(status)) {
                printf("NULL\n");                   // SIGNAL status ain't exit status
            }
            else {
                printf("%d\n", WTERMSIG(status));
            }
        } else {
                printf("status: invalid arg '%s'\n", vec->arr[1]);
        }
    } else {
        printf("%d\n", last_status);
    }
}

void  SIGINT_Handler(int signo) {
    (void)signo;
    int errtmp = errno; //save errno
    write(STDOUT_FILENO, "\n", 1);
    errno = errtmp; // recover errno
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = SIGINT_Handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    
    struct ArgsVec *vec = NULL;
    char *arg = NULL;
    char *save = NULL;
    int status = 0;
    int last_status = 0;
    int valid_raw = 0;
    
    if (getenv("PWD") == NULL) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            setenv("PWD", cwd, 1);
        }
    }
    
    while (1) {
        char prompt_buf[PATH_MAX + 8];
        const char *pwd = getenv("PWD");
        snprintf(prompt_buf, sizeof(prompt_buf), "%s$ ", pwd ? pwd : "?");
        fflush(stdout);
        errno = 0;
        char *line = linenoise(prompt_buf);
        if (line == NULL) {
            int status_tmp = errno;
            if (status_tmp == EAGAIN || status_tmp == EINTR) {   // Ctrl-C, or a signal interrupted the read
                clearerr(stdin);
                continue;
            } else if (status_tmp == ENOENT || feof(stdin)) {    // Ctrl-D on a tty, or EOF on a pipe/file
                printf("Control D?\n");
                printf("Bro, he's getting out of hand.\n");
                status_tmp = last_status;
            } else {
                fprintf(stderr, "Read error?\n");
                fprintf(stderr, "Bro, I can't read.\n");
            }
            valid_raw = 0;
            last_status = status_tmp;
            break;
        }
        
        if (line[0] != '\0') linenoiseHistoryAdd(line);
        
        if (!construct_argsvec(&vec)) {
            return EXIT_FAILURE;
        }

        arg = strtok_r(line, " \t", &save);
        while (arg != NULL) {
            if (!vec_push_back(vec, arg)) {
                destruct_argsvec(&vec);
                return EXIT_FAILURE;
            }
            arg = strtok_r(NULL, " \t", &save);
        }
        // Ignore blank lines
        if (vec->size == 0) {
            goto END;
        }

        if (strcmp(vec->arr[0], "exit") == 0) {
            break;
        } else if (strcmp(vec->arr[0], "cd") == 0) {
            last_status = cd_cmd(vec);
            valid_raw = 0;
            goto END;
        } else if (strcmp(vec->arr[0], "status") == 0) {
            status_cmd(vec, valid_raw, status, last_status);
            goto END;
        }

        pid_t pid = fork();
        if (pid == -1) {
            int errno_tmp = errno;
            perror("fork");
            destruct_argsvec(&vec);
            last_status = errno_tmp ? errno_tmp : -1;
            valid_raw = 0;
        } else if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            execvp(vec->arr[0], vec->arr);
            int errno_tmp = errno;
            perror("execvp");
            _exit(errno_tmp); // Exit child immediately if execv fails
        } else {
            pid_t r;
            while ((r = waitpid(pid, &status, 0)) == -1 && errno == EINTR);
            if (r == -1) {
                last_status = errno;
                valid_raw = 0;
                perror("waitpid");
                goto END;
            }
            valid_raw = 1;
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);        /* the code passed to exit/_exit */
            } else if (WIFSIGNALED(status)) {
                last_status = 128 + WTERMSIG(status);     /* shell convention */
            }
        }
        END:
        free(line);
        destruct_argsvec(&vec);
    }
    destruct_argsvec(&vec);
    return last_status;
}
