#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "builtin.h"
#include "command.h"
#include "input.h"
#include "job.h"
#include "sh_lexer.h"
#include "sh_parser.h"

static char* line = NULL;
static sigjmp_buf env;
static volatile sig_atomic_t jump_active = 0;
static struct termios saved_termios;

static void restore_termios() {
    if (isatty(STDIN_FILENO)) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
    }
}

static void on_int(int signo) {
    assert(signo == SIGINT);
    if (!jump_active) {
        return;
    }

    siglongjmp(env, 1);
}

int main(int argc, char** argv) {
    struct input_source input_source;

    // Respect -c
    if (argc > 1 && strcmp(argv[1], "-c") == 0) {
        input_source.mode = INPUT_STRING;
        input_source.source.string_input_source = input_create_string_input_source(argv[2]);
    } else if (argc >= 2) {
        input_source.mode = INPUT_FILE;
        input_source.source.file = fopen(argv[1], "r");
        if (input_source.source.file == NULL) {
            perror("Shell");
            return EXIT_FAILURE;
        }
    } else {
        input_source.mode = INPUT_TTY;
        input_source.source.tty = stdin;
    }

    if (isatty(STDOUT_FILENO)) {
        struct sigaction to_set;
        to_set.sa_handler = &on_int;
        to_set.sa_flags = 0;
        sigaction(SIGINT, &to_set, NULL);

        to_set.sa_flags = 0;
        to_set.sa_handler = SIG_IGN;
        sigaction(SIGTTOU, &to_set, NULL);

        sigset_t sigset;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGTSTP);
        sigprocmask(SIG_SETMASK, &sigset, NULL);

        tcgetattr(STDOUT_FILENO, &saved_termios);
        atexit(restore_termios);

        char* base = getenv("HOME");
        char* hist_file_name = (char*) ".sh_hist";
        char* hist_file = reinterpret_cast<char*>(malloc(strlen(base) + strlen(hist_file_name) + 2));
        strcpy(hist_file, base);
        strcat(hist_file, "/");
        strcat(hist_file, hist_file_name);

        setenv("HISTFILE", hist_file, 0);
        setenv("HISTSIZE", "100", 0);

        // setenv makes a copy
        free(hist_file);

        init_history();
        atexit(write_history);
    }

    command_init_special_vars(argc - 1, argv + 1);

    for (;;) {
        if (sigsetjmp(env, 1) == 1) {
            if (line) {
                free(line);
            }
            fprintf(stderr, "^C%c", '\n');
        }
        jump_active = 1;

        job_check_updates(true);

        ShValue command;
        char* line = nullptr;
        auto result = input_get_line(&input_source, &line, &command);

        /* Check if we reached EOF */
        if (result == InputResult::Eof) {
            break;
        }

        /* Check If The Line Was Empty */
        if (result == InputResult::Empty) {
            continue;
        }

        if (result == InputResult::Error) {
            free(line);
            continue;
        }

        assert(command.has_program());
        command_run(command.program());

        free(line);
    }

    if (input_source.mode == INPUT_TTY) {
        printf("exit\n");
    }
    input_cleanup(&input_source);

    return EXIT_SUCCESS;
}
