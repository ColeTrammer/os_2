#include <assert.h>
#include <errno.h>
#include <ext/parse_mode.h>
#include <fcntl.h>
#include <liim/hash_map.h>
#include <liim/string.h>
#include <liim/vector.h>
#include <sh/sh_lexer.h>
#include <sh/sh_parser.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef USERLAND_NATIVE
#include <wordexp.h>
#else
#include "../../libs/libc/include/wordexp.h"
#endif /* USERLAND_NATIVE */

#include "builtin.h"
#include "command.h"
#include "input.h"
#include "job.h"
#include "sh_state.h"

static int op_exit(char **args) {
    int status = 0;
    if (args[1] != NULL) {
        if (args[2] != NULL) {
            printf("Usage: %s [n]\n", args[0]);
            return 1;
        }
        status = atoi(args[1]);
    }

    exit(status);
}

static int op_cd(char **args) {
    if (args[2]) {
        printf("Usage: %s <dir>\n", args[0]);
        return 0;
    }

    char *dir = NULL;
    if (!args[1]) {
        dir = getenv("HOME");
    } else {
        dir = args[1];
    }

    int ret = chdir(dir);
    if (ret != 0) {
        perror("Shell");
    }

    __refreshcwd();
    return 0;
}

static int op_echo(char **args) {
    if (!args[1]) {
        printf("%c", '\n');
        return 0;
    }

    size_t i = 1;
    bool print_new_line = true;
    if (strcmp(args[i], "-n") == 0) {
        print_new_line = false;
        i++;
    }

    for (; args[i];) {
        printf("%s", args[i]);
        if (args[i + 1] != NULL) {
            printf("%c", ' ');
            i++;
        } else {
            break;
        }
    }

    if (print_new_line) {
        printf("%c", '\n');
    }

    if (fflush(stdout)) {
        perror("echo");
        return 1;
    }
    return 0;
}

static int op_colon(char **) {
    return 0;
}

static int op_true(char **) {
    return 0;
}

static int op_false(char **) {
    return 1;
}

static int op_export(char **argv) {
    if (!argv[1]) {
        printf("Usage: %s <key=value>\n", argv[0]);
        return 0;
    }

    for (size_t i = 1; argv[i] != NULL; i++) {
        char *equals = strchr(argv[i], '=');
        if (equals == NULL) {
            fprintf(stderr, "Invalid environment string: %s\n", argv[i]);
            continue;
        }
        *equals = '\0';

        if (setenv(argv[i], equals + 1, 1)) {
            perror("shell");
            return 0;
        }
    }

    return 0;
}

static int op_unset(char **argv) {
    if (!argv[1]) {
        printf("Usage: %s <key>\n", argv[0]);
        return 0;
    }

    for (size_t i = 1; argv[i] != NULL; i++) {
        if (unsetenv(argv[i])) {
            perror("shell");
            return 0;
        }
    }

    return 0;
}

static int op_jobs(char **argv) {
    (void) argv;

    job_check_updates(false);
    job_print_all();
    return 0;
}

static int op_fg(char **argv) {
    if (!argv[1] || argv[2]) {
        printf("Usage: %s <job>\n", argv[0]);
        return 0;
    }

    job_check_updates(true);

    struct job_id id;
    if (argv[1][0] == '%') {
        id = job_id(JOB_ID, atoi(argv[1] + 1));
    } else {
        id = job_id(JOB_PGID, atoi(argv[1]));
    }

    return job_run(id);
}

static int op_bg(char **argv) {
    if (!argv[1] || argv[2]) {
        printf("Usage: %s <job>\n", argv[0]);
        return 0;
    }

    job_check_updates(true);

    struct job_id id;
    if (argv[1][0] == '%') {
        id = job_id(JOB_ID, atoi(argv[1] + 1));
    } else {
        id = job_id(JOB_PGID, atoi(argv[1]));
    }

    return job_run_background(id);
}

static int op_kill(char **argv) {
    if (!argv[1] || argv[2]) {
        printf("Usage: %s <job>\n", argv[0]);
        return 0;
    }

    struct job_id id;
    if (argv[1][0] == '%') {
        id = job_id(JOB_ID, atoi(argv[1] + 1));
    } else {
        id = job_id(JOB_PGID, atoi(argv[1]));
    }

    int ret = killpg(get_pgid_from_id(id), SIGTERM);

    job_check_updates(true);
    return ret;
}

static int op_history(char **argv) {
    if (argv[1]) {
        printf("Usage: %s\n", argv[0]);
        return 0;
    }

    print_history();
    return 0;
}

static int op_break(char **argv) {
    int break_count = 1;
    if (argv[1] != NULL) {
        break_count = atoi(argv[1]);
    }

    if (get_loop_depth_count() == 0) {
        fprintf(stderr, "Break is meaningless outside of for,while,until.\n");
        return 0;
    }

    if (break_count == 0) {
        fprintf(stderr, "Invalid loop number.\n");
        return 1;
    }

    set_break_count(break_count);
    return 0;
}

static int op_continue(char **argv) {
    int continue_count = 1;
    if (argv[1] != NULL) {
        continue_count = atoi(argv[1]);
    }

    if (get_loop_depth_count() == 0) {
        fprintf(stderr, "Continue is meaningless outside of for,while,until.\n");
        return 0;
    }

    if (continue_count == 0) {
        fprintf(stderr, "Invalid loop number.\n");
        return 1;
    }

    set_continue_count(continue_count);
    return 0;
}

int op_dot(char **argv) {
    if (argv[1] == NULL) {
        fprintf(stderr, "Usage: %s <filename> [args]\n", argv[0]);
        return 2;
    }

    struct input_source source;
    source.mode = INPUT_FILE;
    source.source.file = fopen(argv[1], "r");
    if (!source.source.file) {
        fprintf(stderr, "%s: Failed to open file `%s'\n", argv[0], argv[1]);
        return 1;
    }

    int i;
    for (i = 2; argv[i] != NULL; i++)
        ;

    command_push_position_params(PositionArgs(argv + 2, i - 2));

    inc_exec_depth_count();
    int ret = do_command_from_source(&source);
    dec_exec_depth_count();
    return ret;
}

extern HashMap<String, String> g_aliases;

static String sh_escape(const String &string) {
    // FIXME: should escape the literal `'` with `'\''`
    return String::format("'%s'", string.string());
}

static int op_alias(char **argv) {
    if (argv[1] == nullptr) {
        g_aliases.for_each_key([&](const String &name) {
            printf("alias %s=%s\n", name.string(), sh_escape(*g_aliases.get(name)).string());
        });
        return 0;
    }

    bool any_failed = false;
    for (int i = 1; argv[i] != nullptr; i++) {
        String arg(argv[i]);
        auto equal_index = arg.index_of('=');
        if (!equal_index.has_value()) {
            auto *alias_name = g_aliases.get(arg);
            if (!alias_name) {
                any_failed = true;
                continue;
            }

            printf("alias %s=%s\n", arg.string(), sh_escape(*alias_name).string());
            continue;
        }

        arg[equal_index.value()] = '\0';
        String name(arg.string());
        String alias(arg.string() + equal_index.value() + 1);
        g_aliases.put(name, alias);
    }

    return any_failed ? 1 : 0;
}

static int op_unalias(char **argv) {
    if (argv[1] == nullptr) {
        fprintf(stderr, "Usage: %s [-a] name [...]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-a") == 0) {
        g_aliases.clear();
        return 0;
    }

    bool any_failed = false;
    for (int i = 1; argv[i] != nullptr; i++) {
        String s(argv[i]);

        if (!g_aliases.get(s)) {
            any_failed = true;
            continue;
        }
        g_aliases.remove(s);
    }

    return any_failed ? 1 : 0;
}

static int op_return(char **argv) {
    int status = 0;
    if (argv[1] != NULL) {
        if (argv[2] != NULL) {
            fprintf(stderr, "Usage: %s [status]\n", argv[0]);
            return 1;
        }

        status = atoi(argv[1]);
    }

    if (get_exec_depth_count() == 0) {
        fprintf(stderr, "Cannot return when not in function or . script\n");
        return 1;
    }

    set_should_return();
    return status;
}

static int op_shift(char **argv) {
    int amount = 1;
    if (argv[1] != NULL) {
        if (argv[2] != NULL) {
            fprintf(stderr, "Usage %s [n]\n", argv[0]);
            return 1;
        }

        amount = atoi(argv[1]);
    }

    if (amount == 1 && command_position_params_size() == 0) {
        return 0;
    }

    if (amount < 0 || amount > (int) command_position_params_size()) {
        fprintf(stderr, "Invalid shift amount\n");
        return 1;
    }

    command_shift_position_params_left(amount);
    return 0;
}

static int op_exec(char **argv) {
    if (argv[1] == NULL) {
        return 0;
    }

    execvp(argv[1], argv + 1);
    if (errno == ENOENT) {
        return 127;
    }
    return 126;
}

static int op_set(char **argv) {
    if (argv[1] == NULL) {
        // FIXME: should print out every shell variable.
        return 0;
    }

    int i = 1;
    for (; argv[i]; i++) {
        if (argv[i][0] == '-' || argv[i][0] == '+') {
            bool worked = false;
            if (argv[i][1] != 'o') {
                worked = ShState::the().process_arg(argv[i]);
            } else {
                bool to_set = argv[i][0] == '-';
                i++;

                if (argv[i]) {
                    ShState::the().process_option(argv[i], to_set);
                    worked = true;
                } else {
                    if (to_set) {
                        ShState::the().dump_for_reinput();
                    } else {
                        ShState::the().dump();
                    }
                    worked = false;
                }
            }

            if (worked) {
                continue;
            }
        }

        break;
    }

    for (; argv[i]; i++) {
        command_add_position_param(argv[i]);
    }

    return 0;
}

static int op_test(char **argv) {
    if (!argv[1]) {
        return 1;
    }

    int argc = 0;
    while (argv[++argc]) {
    }

    if (strcmp(argv[0], "[") == 0) {
        if (strcmp(argv[argc - 1], "]") != 0) {
            fprintf(stderr, "%s: expected `]'\n", argv[0]);
            return 1;
        }

        argc--;
    }

    argc--;
    argv++;

    bool invert = false;
    if (strcmp(argv[0], "!") == 0) {
        invert = true;
        argc--;
        argv++;
    }

    if (argc == 1) {
        return strlen(argv[0]) != 0 ? invert : !invert;
    }

    if (argc == 2) {
        if (strcmp(argv[0], "-b") == 0) {
            struct stat st;
            if (stat(argv[1], &st)) {
                return 1;
            }

            return S_ISBLK(st.st_mode) ? invert : !invert;
        }
        if (strcmp(argv[0], "-c") == 0) {
            struct stat st;
            if (stat(argv[1], &st)) {
                return 1;
            }

            return S_ISCHR(st.st_mode) ? invert : !invert;
        }
        if (strcmp(argv[0], "-d") == 0) {
            struct stat st;
            if (stat(argv[1], &st)) {
                return 1;
            }

            return S_ISDIR(st.st_mode) ? invert : !invert;
        }
        if (strcmp(argv[0], "-e") == 0) {
            struct stat st;
            if (stat(argv[1], &st)) {
                return 1;
            }

            return invert;
        }
        if (strcmp(argv[0], "-f") == 0) {
            struct stat st;
            if (stat(argv[1], &st)) {
                return 1;
            }

            return S_ISREG(st.st_mode) ? invert : !invert;
        }
        if (strcmp(argv[0], "-g") == 0) {
            struct stat st;
            if (stat(argv[1], &st)) {
                return 1;
            }

            return (st.st_mode & 02000) ? invert : !invert;
        }
        if (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "-L") == 0) {
            struct stat st;
            if (lstat(argv[1], &st)) {
                return 1;
            }

            return S_ISLNK(st.st_mode) ? invert : !invert;
        }
        if (strcmp(argv[0], "-n") == 0) {
            return strlen(argv[1]) != 0 ? invert : !invert;
        }
        if (strcmp(argv[0], "-p") == 0) {
            struct stat st;
            if (stat(argv[1], &st)) {
                return 1;
            }

            return S_ISFIFO(st.st_mode) ? invert : !invert;
        }
        if (strcmp(argv[0], "-r") == 0) {
            if (access(argv[1], R_OK)) {
                return !invert;
            }
            return invert;
        }
        if (strcmp(argv[0], "-S") == 0) {
            struct stat st;
            if (stat(argv[1], &st)) {
                return 1;
            }

            return S_ISSOCK(st.st_mode) ? invert : !invert;
        }
        if (strcmp(argv[0], "-s") == 0) {
            struct stat st;
            if (stat(argv[1], &st)) {
                return 1;
            }

            return st.st_size ? invert : !invert;
        }
        if (strcmp(argv[0], "-t") == 0) {
            return isatty(atoi(argv[1])) ? invert : !invert;
        }
        if (strcmp(argv[0], "-u") == 0) {
            struct stat st;
            if (stat(argv[1], &st)) {
                return 1;
            }

            return (st.st_mode & 04000) ? invert : !invert;
        }
        if (strcmp(argv[0], "-w") == 0) {
            if (access(argv[1], W_OK)) {
                return !invert;
            }
            return invert;
        }
        if (strcmp(argv[0], "-x") == 0) {
            if (access(argv[1], X_OK)) {
                return !invert;
            }
            return invert;
        }
        if (strcmp(argv[0], "-z") == 0) {
            return strlen(argv[1]) == 0 ? invert : !invert;
        }
    }

    if (argc == 3) {
        if (strcmp(argv[1], "-lt") == 0) {
            return atol(argv[0]) < atol(argv[2]) ? invert : !invert;
        }
        if (strcmp(argv[1], "-eq") == 0) {
            return atol(argv[0]) == atol(argv[2]) ? invert : !invert;
        }
        if (strcmp(argv[1], "-gt") == 0) {
            return atol(argv[0]) > atol(argv[2]) ? invert : !invert;
        }
        if (strcmp(argv[1], "-ge") == 0) {
            return atol(argv[0]) >= atol(argv[2]) ? invert : !invert;
        }
        if (strcmp(argv[1], "-le") == 0) {
            return atol(argv[0]) <= atol(argv[2]) ? invert : !invert;
        }
        if (strcmp(argv[1], "==") == 0) {
            return strcmp(argv[0], argv[2]) == 0 ? invert : !invert;
        }
        if (strcmp(argv[1], "!=") == 0) {
            return strcmp(argv[0], argv[2]) != 0 ? invert : !invert;
        }
    }

    return !invert;
}

static int op_eval(char **argv) {
    if (!argv[1]) {
        return 0;
    }

    String string;
    for (size_t i = 1; argv[i]; i++) {
        string += argv[i];
        if (argv[i + 1]) {
            string += " ";
        }
    }

    ShLexer lexer(string.string(), string.size());
    if (!lexer.lex()) {
        fprintf(stderr, "syntax error: unexpected end of input\n");
        return 1;
    }

    ShParser parser(lexer);
    if (!parser.parse()) {
        return 1;
    }

    ShValue program(parser.result());
    command_run(program.program());
    return get_last_exit_status();
}

static int op_time(char **argv) {
    struct timeval start;
    gettimeofday(&start, nullptr);
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[1], argv + 1);
        perror("execv");
        _exit(127);
    } else if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (waitpid(pid, nullptr, 0) < 0) {
        perror("waitpid");
        return 1;
    }

    struct timeval end;
    gettimeofday(&end, nullptr);

    struct tms tm;
    times(&tm);

    double real_seconds = end.tv_sec - start.tv_sec + (end.tv_usec - start.tv_usec) / 1000000.0;
    double user_seconds = (double) tm.tms_cutime / sysconf(_SC_CLK_TCK);
    double sys_seconds = (double) tm.tms_cstime / sysconf(_SC_CLK_TCK);

    int real_minutes = real_seconds / 60.0;
    int user_minutes = user_seconds / 60.0;
    int sys_minutes = sys_seconds / 60.0;

    while (real_seconds > 60.0) {
        real_seconds -= 60.0;
    }
    while (user_seconds > 60.0) {
        user_seconds -= 60.0;
    }
    while (sys_seconds > 60.0) {
        sys_seconds -= 60.0;
    }

    printf("\nreal %4dm%.3fs\nuser %4dm%.3fs\nsys  %4dm%.3fs\n", real_minutes, real_seconds, user_minutes, user_seconds, sys_minutes,
           sys_seconds);

    return 0;
}

static Vector<String> s_dir_stack;

static void print_dirs() {
    printf("%s", __getcwd());
    s_dir_stack.for_each([](const auto &dir) {
        printf("%c%s", ' ', dir.string());
    });
    printf("%c", '\n');
}

static int op_dirs(char **) {
    print_dirs();
    return 0;
}

static int op_pushd(char **argv) {
    if (!argv[1]) {
        fprintf(stderr, "Usage: %s <dir>\n", argv[0]);
        return 2;
    }

    if (chdir(argv[1]) < 0) {
        perror("chdir");
        return 1;
    }

    s_dir_stack.add(String(__getcwd()));
    __refreshcwd();
    print_dirs();
    return 0;
}

static int op_popd(char **argv) {
    if (s_dir_stack.empty()) {
        fprintf(stderr, "%s: directory stack empty\n", argv[0]);
        return 1;
    }

    if (chdir(s_dir_stack.last().string()) < 0) {
        perror("chdir");
        return 1;
    }

    s_dir_stack.remove_last();
    __refreshcwd();
    print_dirs();
    return 0;
}

static int op_read(char **argv) {
    Vector<char> input;

    bool prev_was_backslash = false;
    for (;;) {
        errno = 0;
        int ret = fgetc(stdin);
        if (ret == EOF) {
            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            break;
        }

        char c = (char) ret;
        if (c == '\\') {
            prev_was_backslash = true;
            input.add(c);
            continue;
        }

        if (c == '\n') {
            if (!prev_was_backslash) {
                break;
            }

            input.remove_last();
        } else {
            input.add(c);
        }
        prev_was_backslash = false;
    }

    input.add('\0');

    const char *ifs = getenv("IFS");
    const char *split_on = ifs ? ifs : " \t\n";

    wordexp_t exp;
    exp.we_offs = 0;
    exp.we_wordc = 0;
    exp.we_wordv = nullptr;
    int ret = we_split(input.vector(), split_on, &exp, 0);
    if (ret != 0) {
        return 1;
    }

    for (size_t i = 1; argv[i] != nullptr; i++) {
        if (exp.we_wordc <= i - 1) {
            break;
        }

        char *name = argv[i];
        setenv(name, exp.we_wordv[i - 1], 1);
    }

    wordfree(&exp);
    return 0;
}

static int op_umask(char **argv) {
    if (argv[2]) {
        fprintf(stderr, "Usage: %s <mode>\n", *argv);
        return 2;
    }

    if (!argv[1]) {
        mode_t mask = umask(0);
        umask(mask);

        printf("%#.4o\n", mask);
        return 0;
    }

    auto fancy_mode = Ext::parse_mode(argv[1]);
    if (!fancy_mode.has_value()) {
        fprintf(stderr, "%s: failed to parse mode: `%s'\n", *argv, argv[1]);
        return 1;
    }

    if (fancy_mode.value().impl().is<mode_t>()) {
        umask(fancy_mode.value().resolve(0) & 0777);
        return 0;
    }

    mode_t mode = ~umask(0);
    mode = fancy_mode.value().resolve(mode, 0);
    umask(~mode);
    return 0;
}

static struct builtin_op builtin_ops[NUM_BUILTINS] = {
    { "exit", op_exit, true },       { "cd", op_cd, true },         { "echo", op_echo, false },
    { "export", op_export, true },   { "unset", op_unset, true },   { "jobs", op_jobs, true },
    { "fg", op_fg, true },           { "bg", op_bg, true },         { "kill", op_kill, true },
    { "history", op_history, true }, { "true", op_true, true },     { "false", op_false, true },
    { ":", op_colon, true },         { "break", op_break, true },   { "continue", op_continue, true },
    { ".", op_dot, true },           { "source", op_dot, true },    { "alias", op_alias, true },
    { "unalias", op_unalias, true }, { "return", op_return, true }, { "shift", op_shift, true },
    { "exec", op_exec, true },       { "set", op_set, true },       { "[", op_test, true },
    { "test", op_test, true },       { "eval", op_eval, true },     { "time", op_time, false },
    { "pushd", op_pushd, true },     { "popd", op_popd, true },     { "dirs", op_dirs, true },
    { "read", op_read, true },       { "umask", op_umask, true }
};

struct builtin_op *get_builtins() {
    return builtin_ops;
}

struct builtin_op *builtin_find_op(char *name) {
    for (size_t i = 0; i < NUM_BUILTINS; i++) {
        if (strcmp(builtin_ops[i].name, name) == 0) {
            return &builtin_ops[i];
        }
    }

    return NULL;
}

bool builtin_should_run_immediately(struct builtin_op *op) {
    return op && op->run_immediately;
}

int builtin_do_op(struct builtin_op *op, char **args) {
    assert(op);
    return op->op(args);
}
