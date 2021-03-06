#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <liim/pointers.h>
#include <pwd.h>
#include <sh/sh_lexer.h>
#include <sh/sh_parser.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "builtin.h"
#include "command.h"
#include "input.h"
#include "job.h"

enum class LineStatus { Done, Continue, EscapedNewline, Error };

struct suggestion {
    size_t length;
    size_t index;
    char *suggestion;
};

static char **history;
static size_t history_length;
static size_t history_max;

size_t g_command_count;

static struct termios saved_termios;

SharedPtr<String> g_line;

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &saved_termios);

    struct termios to_set = saved_termios;

    to_set.c_cflag |= (CS8);
    to_set.c_lflag &= ~(ECHO | ICANON | IEXTEN);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &to_set);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
}

static size_t __cwd_size;
static char *__cwd_buffer;

void __refreshcwd() {
    if (!__cwd_size) {
        __cwd_size = PATH_MAX;
    }

    if (!__cwd_buffer) {
        __cwd_buffer = static_cast<char *>(malloc(__cwd_size));
    }

    while (!getcwd(__cwd_buffer, __cwd_size)) {
        __cwd_size *= 2;
        __cwd_buffer = static_cast<char *>(realloc(__cwd_buffer, __cwd_size));
    }
}

char *__getcwd() {
    if (!__cwd_buffer) {
        __refreshcwd();
    }

    return __cwd_buffer;
}

extern struct passwd *user_passwd;
extern struct utsname system_name;

static const char *month(int m) {
    switch (m) {
        case 1:
            return "Jan";
        case 2:
            return "Feb";
        case 3:
            return "Mar";
        case '4':
            return "Apr";
        case 5:
            return "May";
        case 6:
            return "Jun";
        case 7:
            return "Jul";
        case 8:
            return "Aug";
        case 9:
            return "Sep";
        case 10:
            return "Oct";
        case 11:
            return "Nov";
        case 12:
            return "Dec";
        default:
            assert(false);
            return nullptr;
    }
}

static const char *weekday(int d) {
    switch (d) {
        case 1:
            return "Mon";
        case 2:
            return "Tue";
        case 3:
            return "Wed";
        case 4:
            return "Thu";
        case 5:
            return "Fri";
        case 6:
            return "Sat";
        case 7:
            return "Sun";
        default:
            assert(false);
            return nullptr;
    }
}

static void print_ps1_prompt() {
    const char *PS1 = getenv("PS1");
    if (!PS1) {
        PS1 = "\\$ ";
    }

    bool prev_was_backslash = false;
    for (size_t i = 0; PS1[i] != '\0'; i++) {
        char c = PS1[i];
        if (prev_was_backslash) {
            switch (c) {
                case 'a':
                    fprintf(stderr, "%c", '\a');
                    break;
                case 'd': {
                    time_t time = ::time(nullptr);
                    struct tm *date = localtime(&time);
                    fprintf(stderr, "%s %s %d", weekday(date->tm_wday), month(date->tm_mon), date->tm_mday);
                    break;
                }
                case 'H':
                case 'h':
                    fprintf(stderr, "%s", system_name.nodename);
                    break;
                case 'n':
                    fprintf(stderr, "%c", '\n');
                    break;
                case 'r':
                    fprintf(stderr, "%c", '\r');
                    break;
                case 's':
                    fprintf(stderr, "%s", "sh");
                    break;
                case 't': {
                    time_t time = ::time(nullptr);
                    struct tm *date = localtime(&time);
                    fprintf(stderr, "%02d:%02d:%02d", date->tm_hour, date->tm_min, date->tm_sec);
                    break;
                }
                case 'T': {
                    time_t time = ::time(nullptr);
                    struct tm *date = localtime(&time);
                    fprintf(stderr, "%02d:%02d:%02d", date->tm_hour % 12, date->tm_min, date->tm_sec);
                    break;
                }
                case '@': {
                    time_t time = ::time(nullptr);
                    struct tm *date = localtime(&time);
                    fprintf(stderr, "%02d:%02d %s", date->tm_hour % 12, date->tm_min, date->tm_hour < 12 ? "AM" : "PM");
                    break;
                }
                case 'A': {
                    time_t time = ::time(nullptr);
                    struct tm *date = localtime(&time);
                    fprintf(stderr, "%02d:%02d", date->tm_hour, date->tm_min);
                    break;
                }
                case 'u':
                    fprintf(stderr, "%s", user_passwd->pw_name);
                    break;
                case 'W':
                case 'w': {
                    char *cwd = strdup(__getcwd());
                    char *cwd_use = cwd;

                    size_t home_dir_length = strlen(user_passwd->pw_dir);
                    if (strcmp(user_passwd->pw_dir, "/") != 0 && strncmp(cwd, user_passwd->pw_dir, home_dir_length) == 0) {
                        cwd_use = cwd + home_dir_length - 1;
                        *cwd_use = '~';
                    }

                    fprintf(stderr, "%s", cwd_use);
                    free(cwd);
                    break;
                }
                case '#':
                    fprintf(stderr, "%lu", g_command_count);
                    break;
                case '!':
                    fprintf(stderr, "%lu", history_length);
                    break;
                case '$':
                    if (geteuid() == 0) {
                        fprintf(stderr, "%c", '#');
                    } else {
                        fprintf(stderr, "%c", '$');
                    }
                    break;
                case 'e':
                    fprintf(stderr, "%c", '\033');
                    break;
                case '[':
                case ']':
                    break;
                default:
                    fprintf(stderr, "%c", c);
                    break;
            }

            prev_was_backslash = false;
            continue;
        }

        if (c == '\\') {
            prev_was_backslash = true;
        } else {
            fprintf(stderr, "%c", c);
        }
    }
}

static char *scandir_match_string = NULL;

static int scandir_filter(const struct dirent *d) {
    assert(scandir_match_string);
    if (d->d_name[0] == '.' && scandir_match_string[0] != '.') {
        return false;
    }
    return strstr(d->d_name, scandir_match_string) == d->d_name;
}

static int suggestion_compar(const void *a, const void *b) {
    return strcmp(((const struct suggestion *) a)->suggestion, ((const struct suggestion *) b)->suggestion);
}

static void init_suggestion(struct suggestion *suggestions, size_t at, size_t suggestion_index, const char *name, const char *post) {
    suggestions[at].length = strlen(name) + strlen(post);
    suggestions[at].index = suggestion_index;
    suggestions[at].suggestion = (char *) malloc(suggestions[at].length + 1);
    strcpy(suggestions[at].suggestion, name);
    strcat(suggestions[at].suggestion, post);
}

static struct suggestion *get_path_suggestions(char *line, int *num_suggestions, bool at_end) {
    char *path_var = getenv("PATH");
    if (path_var == NULL) {
        return NULL;
    }

    char *path_copy = strdup(path_var);
    char *search_path = strtok(path_copy, ":");
    struct suggestion *suggestions = NULL;
    while (search_path != NULL) {
        scandir_match_string = line;
        struct dirent **list;
        int num_found = scandir(search_path, &list, scandir_filter, alphasort);

        if (num_found > 0) {
            suggestions = (struct suggestion *) realloc(suggestions, ((*num_suggestions) + num_found) * sizeof(struct suggestion));

            for (int i = 0; i < num_found; i++) {
                init_suggestion(suggestions, (*num_suggestions) + i, 0, list[i]->d_name, at_end ? " " : "");
                free(list[i]);
            }

            (*num_suggestions) += num_found;
            free(list);
        }

        search_path = strtok(NULL, ":");
    }

    // Check builtins
    struct builtin_op *builtins = get_builtins();
    for (size_t i = 0; i < NUM_BUILTINS; i++) {
        if (strstr(builtins[i].name, line) == builtins[i].name) {
            suggestions = (struct suggestion *) realloc(suggestions, ((*num_suggestions) + 1) * sizeof(struct suggestion));

            init_suggestion(suggestions, *num_suggestions, 0, builtins[i].name, " ");

            (*num_suggestions)++;
        }
    }

    free(path_copy);
    qsort(suggestions, *num_suggestions, sizeof(struct suggestion), suggestion_compar);
    return suggestions;
}

static struct suggestion *get_suggestions(char *line, int *num_suggestions, bool at_end) {
    *num_suggestions = 0;

    char *last_space = strrchr(line, ' ');
    bool is_first_word = last_space == NULL;
    if (is_first_word) {
        last_space = line - 1;
    }

    char *to_match_start = last_space + 1;
    char *to_match = strdup(to_match_start);

    char *last_slash = strrchr(to_match, '/');
    char *dirname;
    char *currname;
    bool relative_to_root = false;
    if (last_slash == NULL) {
        if (is_first_word) {
            free(to_match);
            return get_path_suggestions(line, num_suggestions, at_end);
        }

        dirname = (char *) ".";
        currname = to_match;
    } else if (last_slash == to_match) {
        dirname = (char *) "/";
        currname = to_match + 1;
        relative_to_root = true;
    } else {
        *last_slash = '\0';
        dirname = to_match;
        currname = last_slash + 1;
    }

    scandir_match_string = currname;
    struct dirent **list;
    *num_suggestions = scandir(dirname, &list, scandir_filter, alphasort);
    if (*num_suggestions <= 0) {
        free(to_match);
        return NULL;
    }

    struct suggestion *suggestions = (struct suggestion *) malloc(*num_suggestions * sizeof(struct suggestion));

    for (ssize_t i = 0; i < (ssize_t) *num_suggestions; i++) {
        struct stat stat_struct;
        char *path;
        if (relative_to_root) {
            path = static_cast<char *>(malloc(strlen(list[i]->d_name) + 2));
            strcpy(stpcpy(path, "/"), list[i]->d_name);
        } else {
            path = static_cast<char *>(malloc(strlen(dirname) + strlen(list[i]->d_name) + 2));
            stpcpy(stpcpy(stpcpy(path, dirname), "/"), list[i]->d_name);
        }

        if (stat(path, &stat_struct)) {
            goto suggestions_skip_entry;
        }

        if (is_first_word && !(stat_struct.st_mode & S_IXUSR)) {
            goto suggestions_skip_entry;
        }

        free(path);
        init_suggestion(suggestions, (size_t) i, 0, list[i]->d_name, S_ISDIR(stat_struct.st_mode) ? "/" : at_end ? " " : "");

        if (last_slash == NULL) {
            suggestions[i].index = to_match_start - line;
        } else if (relative_to_root) {
            suggestions[i].index = to_match_start - line + 1;
        } else {
            suggestions[i].index = to_match_start - line + (currname - dirname);
        }
        free(list[i]);
        continue;

    suggestions_skip_entry:
        free(path);
        (*num_suggestions)--;
        memmove(list + i, list + i + 1, ((*num_suggestions) - i) * sizeof(struct dirent *));
        i--;
        continue;
    }

    free(list);
    free(to_match);
    return suggestions;
}

static void free_suggestions(struct suggestion *suggestions, size_t num_suggestions) {
    if (num_suggestions == 0 || suggestions == NULL) {
        return;
    }

    for (size_t i = 0; i < num_suggestions; i++) {
        free(suggestions[i].suggestion);
    }

    free(suggestions);
}

// NOTE: since the suggestions are already sorted alphabetically (using alphasort) on dirents, this method
//       only needs to check the first and last strings
static size_t longest_common_starting_substring_length(struct suggestion *suggestions, size_t num_suggestions) {
    struct suggestion *last = &suggestions[num_suggestions - 1];
    size_t length = 0;
    while (suggestions->suggestion[length] != '\0' && suggestions->suggestion[length] != '\0' &&
           suggestions->suggestion[length] == last->suggestion[length]) {
        length++;
    }
    return length;
}

// Checks whether there are any open quotes or not in the line
static LineStatus get_line_status(char *line, size_t len, ShValue *value, bool consider_buffer_termination_to_be_end_of_line = false) {
    bool prev_was_backslash = false;
    bool in_s_quotes = false;
    bool in_d_quotes = false;
    bool in_b_quotes = false;

    for (size_t i = 0; i < len; i++) {
        switch (line[i]) {
            case '\\':
                if (!in_s_quotes) {
                    prev_was_backslash = !prev_was_backslash;
                }
                continue;
            case '"':
                in_d_quotes = (prev_was_backslash || in_s_quotes || in_b_quotes) ? in_d_quotes : !in_d_quotes;
                break;
            case '\'':
                in_s_quotes = (in_d_quotes || in_b_quotes) ? in_s_quotes : !in_s_quotes;
                break;
            case '`':
                in_b_quotes = (prev_was_backslash || in_d_quotes || in_s_quotes) ? in_b_quotes : !in_b_quotes;
                break;
            case '\n':
                if (i == len - 1 && prev_was_backslash) {
                    return LineStatus::EscapedNewline;
                }
            default:
                break;
        }

        prev_was_backslash = false;
    }

    if (prev_was_backslash && consider_buffer_termination_to_be_end_of_line) {
        return LineStatus::EscapedNewline;
    }

    ShLexer lexer(line, len);
    auto lex_result = lexer.lex();
    if (!lex_result) {
        return LineStatus::Continue;
    }

    ShParser parser(lexer);
    auto parse_result = parser.parse();
    if (!parse_result) {
        return parser.needs_more_tokens() ? LineStatus::Continue : LineStatus::Error;
    }

    new (value) ShValue(parser.result());
    return LineStatus::Done;
}

static void history_add(char *item) {
    if (history_length > 0 && strcmp(item, history[history_length - 1]) == 0) {
        return;
    }

    if (history_length == history_max) {
        free(history[0]);
        memmove(history, history + 1, (history_max - 1) * sizeof(char *));
        history_length--;
    }

    history[history_length] = strdup(item);
    history_length++;
}

struct HistorySearchResult {
    size_t index;
    size_t position;
};

static Maybe<HistorySearchResult> history_find(const Vector<char> &needle, size_t start_index) {
    if (needle.size() <= 1) {
        return {};
    }

    size_t hist_index = start_index + 1;
    do {
        auto string = history[--hist_index];
        char *result = strstr(string, needle.vector());
        if (result) {
            return { { hist_index, static_cast<size_t>(result - string) } };
        }

    } while (hist_index != 0);

    return {};
}

char *buffer = NULL;
char *line_save = NULL;

static InputResult get_tty_input(FILE *tty, ShValue *value) {
    print_ps1_prompt();

    size_t buffer_max = 1024;
    size_t buffer_index = 0;
    size_t buffer_length = 0;
    size_t buffer_min_index = 0;
    buffer = (char *) malloc(buffer_max);

    line_save = NULL;
    size_t hist_index = history_length;

    int consecutive_tab_presses = 0;
    bool need_input = true;

    auto set_hist_index = [&](size_t new_hist_index, ssize_t new_buffer_index = -1) {
        if (hist_index >= history_length) {
            buffer[buffer_length] = '\0';
            if (buffer_length > 0) {
                line_save = strdup(buffer);
            } else {
                line_save = NULL;
            }
        }

        hist_index = new_hist_index;

        memset(buffer + buffer_min_index, ' ', buffer_length - buffer_min_index);

        if (buffer_index - buffer_min_index > 0) {
            char f_buf[20];
            snprintf(f_buf, 20, "\033[%luD", buffer_index - buffer_min_index);
            write(fileno(tty), f_buf, strlen(f_buf));
        }

        write(fileno(tty), "\033[s", 3);
        write(fileno(tty), buffer + buffer_min_index, buffer_length - buffer_min_index);
        write(fileno(tty), "\033[u", 3);

        if (hist_index >= history_length) {
            if (!line_save) {
                buffer_index = buffer_min_index = buffer_length = 0;
                return;
            } else {
                strncpy(buffer, line_save, buffer_max);
            }
        } else {
            strncpy(buffer, history[hist_index], buffer_max);
        }

        write(fileno(tty), buffer, strlen(buffer));
        buffer_length = strlen(buffer);
        if (new_buffer_index == -1) {
            buffer_index = buffer_length;
        } else {
            buffer_index = new_buffer_index;
        }
        buffer_min_index = 0;

        if (buffer_length - buffer_index > 0) {
            char f_buf[20];
            snprintf(f_buf, 20, "\033[%luD", buffer_length - buffer_index);
            write(fileno(tty), f_buf, strlen(f_buf));
        }
    };

    char c;
    for (;;) {
        if (buffer_length + 1 >= buffer_max) {
            buffer_max += 1024;
            buffer = (char *) realloc(buffer, buffer_max);
        }

        if (need_input) {
            errno = 0;
            int ret = read(fileno(tty), &c, 1);

            if (ret == -1) {
                // We were interrupted
                if (errno == EINTR) {
                    buffer_length = 0;
                    break;
                } else {
                    free(line_save);
                    free(buffer);
                    return InputResult::Eof;
                }
            }

            // We will never get 0 back from read, since we block for input
            assert(ret == 1);
        } else {
            need_input = true;
        }

        // tab autocompletion
        if (c == '\t') {
            bool at_end = buffer_index == buffer_length;
            char *suggestion_buffer = nullptr;
            size_t i = 0;
            int num_suggestions = 0;
            char end_save = buffer[buffer_index];
            buffer[buffer_index] = '\0'; // Ensure buffer is null terminated
            struct suggestion *suggestions = get_suggestions(buffer, &num_suggestions, at_end);
            buffer[buffer_index] = end_save;

            if (!suggestions) {
                continue;
            }

            if (num_suggestions == 0) {
                consecutive_tab_presses = 0;
                continue;
            } else if (num_suggestions > 1) {
                suggestions->length = longest_common_starting_substring_length(suggestions, num_suggestions);
                if (suggestions->length == 0 || memcmp(suggestions->suggestion, buffer + suggestions->index, suggestions->length) == 0) {
                    consecutive_tab_presses++;
                }

                if (consecutive_tab_presses > 1) {
                    fprintf(stderr, "%c", '\n');

                    for (int i = 0; i < num_suggestions; i++) {
                        fprintf(stderr, "%s ", suggestions[i].suggestion);
                    }

                    fprintf(stderr, "%c", '\n');
                    print_ps1_prompt();
                    write(fileno(tty), buffer, buffer_length);
                    if (buffer_length != buffer_index) {
                        char f_buf[20];
                        snprintf(f_buf, 19, "\033[%luD", buffer_length - buffer_index);
                        write(fileno(tty), f_buf, strlen(f_buf));
                    }
                    goto cleanup_suggestions;
                }
            } else {
                consecutive_tab_presses = 0;
            }

            while (i < suggestions->length && suggestions->index + i < buffer_index &&
                   buffer[suggestions->index + i] == suggestions->suggestion[i]) {
                i++;
            }

            if (i == suggestions->length) {
                goto cleanup_suggestions;
            }

            suggestions->index += i;
            suggestions->length -= i;
            suggestion_buffer = suggestions->suggestion + i;

            if (buffer_length + suggestions->length >= buffer_max - 1) {
                buffer_max += 1024;
                buffer = (char *) realloc(buffer, buffer_max);
            }

            if (suggestions->length == 0) {
                goto cleanup_suggestions;
            }

            memmove(buffer + suggestions->index + suggestions->length, buffer + buffer_index, buffer_length - buffer_index);
            memcpy(buffer + suggestions->index, suggestion_buffer, suggestions->length);

            if (buffer_index - suggestions->index > 0) {
                char f_buf[20];
                snprintf(f_buf, 20, "\033[%luD", buffer_index - suggestions->index);
                write(fileno(tty), f_buf, strlen(f_buf));
            }

            if (buffer_length != buffer_index) {
                write(fileno(tty), buffer + suggestions->index, suggestions->length + buffer_length - buffer_index);
                buffer_index += suggestions->length;
                buffer_length += suggestions->length;

                char f_buf[20];
                snprintf(f_buf, 20, "\033[%luD", buffer_length - buffer_index);
                write(fileno(tty), f_buf, strlen(f_buf));
            } else {
                write(fileno(tty), suggestion_buffer, suggestions->length);
                buffer_index = buffer_length = suggestions->index + suggestions->length;
            }

        cleanup_suggestions:
            free_suggestions(suggestions, num_suggestions);
            continue;
        }

        consecutive_tab_presses = 0;

        // Control D
        if (c == ('D' & 0x1F)) {
            if (buffer_length == 0) {
                free(line_save);
                free(buffer);
                return InputResult::Eof;
            }

            continue;
        }

        // Control L
        if (c == ('L' & 0x1F)) {
            write(fileno(tty), "\033[1;1H\033[2J", 10);
            print_ps1_prompt();
            write(fileno(tty), buffer, buffer_length);
            if (buffer_length != buffer_index) {
                char f_buf[20];
                snprintf(f_buf, 19, "\033[%luD", buffer_length - buffer_index);
                write(fileno(tty), f_buf, strlen(f_buf));
            }
            continue;
        }

        // Control R (reverse index search)
        if (c == ('R' & 0x1F)) {
            auto clear_line = [&]() {
                dprintf(fileno(tty), "%s", "\033[999D\033[2K");
            };

            auto write_line = [&](const Vector<char> &view, bool failed = false) {
                clear_line();
                dprintf(fileno(tty), "(%sreverse-i-search)`%.*s': %.*s", failed ? "failed " : "", view.size(), view.vector(),
                        static_cast<int>(buffer_length), buffer);
                if (buffer_length - buffer_index > 0) {
                    char f_buf[20];
                    snprintf(f_buf, sizeof(f_buf) - 1, "\033[%luD", buffer_length - buffer_index);
                    write(fileno(tty), f_buf, strlen(f_buf));
                }
            };

            Vector<char> needle;
            needle.add('\0');
            bool failed = false;

            for (;;) {
                write_line(needle, failed);

                errno = 0;
                int ret = read(fileno(tty), &c, 1);
                if (ret == -1) {
                    if (errno == EINTR) {
                        break;
                    } else {
                        free(line_save);
                        free(buffer);
                        return InputResult::Eof;
                    }
                }

                if (c != saved_termios.c_cc[VERASE] && c != ('R' & 0x1F) && (iscntrl(c) || c == '\t')) {
                    need_input = false;
                    break;
                }

                if (history_length == 0) {
                    failed = true;
                    continue;
                }

                size_t search_index = history_length - 1;
                if (c == saved_termios.c_cc[VERASE]) {
                    if (needle.size() > 1) {
                        needle.remove_last();
                        needle.last() = '\0';
                    }
                } else if (c == ('R' & 0x1F)) {
                    if (hist_index == 0) {
                        failed = true;
                        continue;
                    }
                    search_index = hist_index - 1;
                } else {
                    needle.last() = c;
                    needle.add('\0');
                }

                auto result = history_find(needle, search_index);
                failed = !result.has_value();
                if (result.has_value()) {
                    set_hist_index(result.value().index, result.value().position);
                }
            }

            clear_line();
            print_ps1_prompt();
            write(fileno(tty), buffer, buffer_length);
            if (buffer_length - buffer_index > 0) {
                char f_buf[20];
                snprintf(f_buf, sizeof(f_buf) - 1, "\033[%luD", buffer_length - buffer_index);
                write(fileno(tty), f_buf, strlen(f_buf));
            }
            continue;
        }

        // Control W / WERASE
        if (c == ('W' & 0x1F)) {
            bool done_something = false;
            while (buffer_index > buffer_min_index &&
                   (!done_something || isalnum(buffer[buffer_index - 1]) || buffer[buffer_index - 1] == '_')) {
                done_something = isalnum(buffer[buffer_index - 1]) || buffer[buffer_index - 1] == '_';
                memmove(buffer + buffer_index - 1, buffer + buffer_index, buffer_length - buffer_index);
                buffer[buffer_length - 1] = ' ';

                buffer_index--;
                write(fileno(tty), "\033[1D\033[s", 7);
                write(fileno(tty), buffer + buffer_index, buffer_length - buffer_index);
                write(fileno(tty), "\033[u", 3);
                buffer[buffer_length--] = '\0';
            }

            continue;
        }

        // Terminal escape sequences
        if (c == '\033') {
            read(fileno(tty), &c, 1);

            switch (c) {
                case 'd':
                    goto control_delete;
                case '[':
                    break;
                default:
                    continue;
            }

            read(fileno(tty), &c, 1);
            if (isdigit(c)) {
                char last;
                read(fileno(tty), &last, 1);
                if (last == '~') {
                    switch (c) {
                        case '3':
                            // Delete key
                            if (buffer_index < buffer_length) {
                                memmove(buffer + buffer_index, buffer + buffer_index + 1, buffer_length - buffer_index);
                                buffer[buffer_length - 1] = ' ';

                                write(fileno(tty), "\033[s", 3);
                                write(fileno(tty), buffer + buffer_index, buffer_length - buffer_index);
                                write(fileno(tty), "\033[u", 3);

                                buffer[buffer_length--] = '\0';
                            }
                            continue;
                        default:
                            continue;
                    }
                } else if (last != ';' || (c != '1' && c != '3')) {
                    continue;
                }

                read(fileno(tty), &last, 1);
                if (last != '5') {
                    continue;
                }

                char d;
                read(fileno(tty), &d, 1);
                if (d != '~' && c == '1') {
                    switch (d) {
                        case 'C': {
                            // Control right arrow
                            size_t index = buffer_index;
                            while (index < buffer_length && !isalpha(buffer[++index]))
                                ;
                            while (index < buffer_length && isalpha(buffer[++index]))
                                ;
                            size_t delta = index - buffer_index;
                            if (delta != 0) {
                                buffer_index = index;
                                char buf[50] = { 0 };
                                snprintf(buf, 49, "\033[%luC", delta);
                                write(fileno(tty), buf, strlen(buf));
                            }
                            continue;
                        }
                        case 'D': {
                            // Control left arrow
                            size_t index = buffer_index;
                            while (index > buffer_min_index && !isalpha(buffer[--index]))
                                ;
                            while (index > buffer_min_index && isalpha(buffer[index - 1])) {
                                index--;
                            }
                            size_t delta = buffer_index - index;
                            if (delta != 0) {
                                buffer_index = index;
                                char buf[50] = { 0 };
                                snprintf(buf, 49, "\033[%luD", delta);
                                write(fileno(tty), buf, strlen(buf));
                            }
                            continue;
                        }
                        default:
                            continue;
                    }
                } else {
                    switch (c) {
                        // Control Delete
                        case '3':
                        control_delete : {
                            bool done_something = false;
                            while (buffer_index < buffer_length &&
                                   (!done_something || isalnum(buffer[buffer_index]) || buffer[buffer_index] == '_')) {
                                done_something = isalnum(buffer[buffer_index]) || buffer[buffer_index] == '_';
                                memmove(buffer + buffer_index, buffer + buffer_index + 1, buffer_length - buffer_index);
                                buffer[buffer_length - 1] = ' ';

                                write(fileno(tty), "\033[s", 3);
                                write(fileno(tty), buffer + buffer_index, buffer_length - buffer_index);
                                write(fileno(tty), "\033[u", 3);

                                buffer[buffer_length--] = '\0';
                            }
                            continue;
                        }
                        default:
                            continue;
                    }
                }
            }

            switch (c) {
                case 'A':
                    // Up arrow
                    if (hist_index > 0) {
                        set_hist_index(hist_index - 1);
                    }
                    continue;
                case 'B':
                    // Down arrow
                    if (hist_index < history_length) {
                        set_hist_index(hist_index + 1);
                    }
                    continue;
                case 'C':
                    // Right arrow
                    if (buffer_index < buffer_length) {
                        buffer_index++;
                        write(fileno(tty), "\033[1C", 4);
                    }
                    break;
                case 'D':
                    // Left arrow
                    if (buffer_index > buffer_min_index) {
                        buffer_index--;
                        write(fileno(tty), "\033[1D", 4);
                    }
                    break;
                case 'H': {
                    // Home key
                    if (buffer_index > buffer_min_index) {
                        size_t delta = buffer_index - buffer_min_index;
                        buffer_index = buffer_min_index;
                        char buf[50] = { 0 };
                        snprintf(buf, 49, "\033[%luD", delta);
                        write(fileno(tty), buf, strlen(buf));
                    }
                    break;
                }
                case 'F': {
                    // End key
                    if (buffer_index < buffer_length) {
                        size_t delta = buffer_length - buffer_index;
                        buffer_index = buffer_length;
                        char buf[50] = { 0 };
                        snprintf(buf, 49, "\033[%luC", delta);
                        write(fileno(tty), buf, strlen(buf));
                    }
                    break;
                }
                default:
                    continue;
            }

            continue;
        }

        // Pressed back space
        if (c == saved_termios.c_cc[VERASE]) {
            if (buffer_index > buffer_min_index) {
                memmove(buffer + buffer_index - 1, buffer + buffer_index, buffer_length - buffer_index);
                buffer[buffer_length - 1] = ' ';

                buffer_index--;
                write(fileno(tty), "\033[1D\033[s", 7);
                write(fileno(tty), buffer + buffer_index, buffer_length - buffer_index);
                write(fileno(tty), "\033[u", 3);
                buffer[buffer_length--] = '\0';
            }

            continue;
        }

        // Stop once we get to a new line
        if (c == '\n') {
            switch (get_line_status(buffer, buffer_length, value, true)) {
                case LineStatus::Continue:
                    buffer[buffer_index++] = c;
                    buffer_length++;
                    break;
                case LineStatus::EscapedNewline:
                    if (buffer_index == buffer_length) {
                        buffer_index--;
                    }
                    buffer_length--;
                    buffer[buffer_index] = '\0';
                    break;
                case LineStatus::Done:
                    write(fileno(tty), "\n", 1);
                    goto tty_input_done;
                case LineStatus::Error:
                    write(fileno(tty), "\n", 1);
                    g_line = String::wrap_malloced_chars(buffer);
                    return InputResult::Error;
                default:
                    assert(false);
                    break;
            }

            // The line was not finished
            buffer_min_index = buffer_index;
            write(fileno(tty), "\n> ", 3);
            continue;
        }

        if (isprint(c)) {
            if (buffer_index != buffer_length) {
                memmove(buffer + buffer_index + 1, buffer + buffer_index, buffer_length - buffer_index);
            }
            buffer[buffer_index] = c;
            buffer_length++;

            // Make sure to save and restore the cursor position
            write(fileno(tty), "\033[s", 3);
            write(fileno(tty), buffer + buffer_index, buffer_length - buffer_index);
            write(fileno(tty), "\033[u\033[1C", 7);
            buffer_index++;
        }
    }

tty_input_done:
    free(line_save);
    buffer[buffer_length] = '\0';
    if (buffer_length > 0) {
        history_add(buffer);

        g_line = String::wrap_malloced_chars(buffer);
        return InputResult::Success;
    } else {
        g_line = nullptr;
    }

    free(buffer);
    return InputResult::Empty;
}

static InputResult get_file_input(FILE *file, ShValue *value) {
    int sz = 1024;
    int pos = 0;
    char *buffer = (char *) malloc(sz);

    for (;;) {
        int c = fgetc(file);

        if (c == EOF && pos == 0) {
            free(buffer);
            return InputResult::Eof;
        }

        buffer[pos++] = c;

        if (pos >= sz) {
            sz *= 2;
            buffer = (char *) realloc(buffer, sz);
        }

        if (c == EOF || c == '\n') {
            buffer[pos] = '\0';
            switch (get_line_status(buffer, pos, value)) {
                case LineStatus::Continue:
                    if (c == EOF) {
                        g_line = String::wrap_malloced_chars(buffer);
                        fprintf(stderr, "Unexpected end of file\n");
                        return InputResult::Error;
                    }
                    break;
                case LineStatus::EscapedNewline:
                    pos--;
                    buffer[pos] = '\0';
                    break;
                case LineStatus::Done:
                    goto file_input_done;
                case LineStatus::Error:
                    g_line = String::wrap_malloced_chars(buffer);
                    return InputResult::Error;
                default:
                    assert(false);
                    break;
            };
        }
    }

file_input_done:
    buffer[pos] = '\0';
    g_line = String::wrap_malloced_chars(buffer);

    return InputResult::Success;
}

static InputResult get_string_input(struct string_input_source *source, ShValue *value) {
    int sz = 1024;
    int pos = 0;
    char *buffer = (char *) malloc(sz);

    for (;;) {
        int c = source->string[source->offset++];

        bool done = c == '\0' || source->offset >= source->max;
        if (done && pos == 0) {
            free(buffer);
            return InputResult::Eof;
        }

        buffer[pos++] = c;

        if (pos >= sz) {
            sz *= 2;
            buffer = (char *) realloc(buffer, sz);
        }

        if (done || c == '\n') {
            buffer[pos] = '\0';
            switch (get_line_status(buffer, pos, value)) {
                case LineStatus::Continue:
                    if (done) {
                        g_line = String::wrap_malloced_chars(buffer);
                        fprintf(stderr, "Unexpected end of file\n");
                        return InputResult::Error;
                    }
                    break;
                case LineStatus::EscapedNewline:
                    pos--;
                    buffer[pos] = '\0';
                    break;
                case LineStatus::Done:
                    goto file_input_done;
                case LineStatus::Error:
                    g_line = String::wrap_malloced_chars(buffer);
                    return InputResult::Error;
                default:
                    assert(false);
                    break;
            };
        }
    }

file_input_done:
    buffer[pos] = '\0';
    g_line = String::wrap_malloced_chars(buffer);

    return InputResult::Success;
}

struct string_input_source *input_create_string_input_source(char *s) {
    struct string_input_source *source = (struct string_input_source *) malloc(sizeof(struct string_input_source));
    source->offset = 0;
    source->string = s;
    source->max = strlen(s);
    return source;
}

InputResult input_get_line(struct input_source *source, ShValue *command) {
    switch (source->mode) {
        case INPUT_TTY: {
            enable_raw_mode();
            auto res = get_tty_input(source->source.tty, command);
            disable_raw_mode();
            return res;
        }
        case INPUT_FILE:
            return get_file_input(source->source.file, command);
        case INPUT_STRING:
            return get_string_input(source->source.string_input_source, command);
        default:
            fprintf(stderr, "Invalid input mode: %d\n", source->mode);
            assert(false);
            break;
    }

    return InputResult::Eof;
}

void input_cleanup(struct input_source *source) {
    // Close file if necessary
    if (source->mode == INPUT_FILE) {
        fclose(source->source.file);
    } else if (source->mode == INPUT_STRING) {
        free(source->source.string_input_source);
    }
}

void init_history() {
    char *hist_size = getenv("HISTSIZE");
    if (sscanf(hist_size, "%lu", &history_max) != 1) {
        history_length = 100;
        setenv("HISTSIZE", "100", 0);
    }

    history = (char **) calloc(history_max, sizeof(char *));

    char *hist_file = getenv("HISTFILE");
    if (!hist_file) {
        return;
    }

    FILE *file = fopen(hist_file, "r");
    if (!file) {
        return;
    }

    char *line = NULL;
    size_t line_max = 0;
    while (history_length < history_max && (getline(&line, &line_max, file)) != -1) {
        line[strlen(line) - 1] = '\0'; // Remove trailing \n
        if (strlen(line) == 0) {
            continue;
        }

        history[history_length] = strdup(line);
        history_length++;
    }

    free(line);
    fclose(file);
}

void print_history() {
    for (size_t i = 0; i < history_length; i++) {
        printf("%4lu  %s\n", i, history[i]);
    }
}

void write_history() {
    char *hist_file = getenv("HISTFILE");
    if (!hist_file) {
        return;
    }

    FILE *file = fopen(hist_file, "w");
    if (!file) {
        return;
    }

    for (size_t i = 0; i < history_length; i++) {
        fprintf(file, "%s\n", history[i]);
    }

    fclose(file);
}

int do_command_from_source(struct input_source *input_source) {
    for (;;) {
        if (input_should_stop()) {
            break;
        }

        job_check_updates(true);

        ShValue command;
        auto result = input_get_line(input_source, &command);

        /* Check if we reached EOF */
        if (result == InputResult::Eof) {
            break;
        }

        /* Check If The Line Was Empty */
        if (result == InputResult::Empty) {
            continue;
        }

        if (result == InputResult::Error) {
            g_line = nullptr;
            continue;
        }

        assert(command.has_program());
        command_run(command.program());
    }

    if (input_source->mode == INPUT_TTY) {
        printf("exit\n");
    }
    input_cleanup(input_source);
    command_pop_position_params();
    return get_last_exit_status();
}
