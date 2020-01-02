#ifndef _COMMAND_H
#define _COMMAND_H 1

#include <liim/linked_list.h>
#include <liim/stack.h>
#include <liim/string.h>
#include <liim/vector.h>
#include <stdio.h>
#include <wordexp.h>

#include "sh_token.h"

#define MAX_REDIRECTIONS 10

class PositionArgs {
public:
    Vector<char*> argv;
    int argc;

    PositionArgs(char** argv, int _argc) : argc(_argc) {
        for (int i = 0; i < argc; i++) {
            strings.add(String(argv[i]));
        }
        sync_argv();
    }

    const PositionArgs& operator=(const PositionArgs& other) {
        strings = other.strings;
        argc = other.argc;
        sync_argv();

        return *this;
    }

    PositionArgs(const PositionArgs& other) : argc(other.argc), strings(other.strings) { sync_argv(); }

private:
    void sync_argv() {
        strings.for_each([&](const auto& s) {
            argv.add((char*) s.string());
        });
    }

    LinkedList<String> strings;
};

int command_run(ShValue::Program& program);

void command_init_special_vars(char* arg_zero);

void set_exit_status(int n);
int get_last_exit_status();

void command_push_position_params(const PositionArgs& args);
void command_pop_position_params();

void set_break_count(int count);
void set_continue_count(int count);
int get_loop_depth_count();

#endif /* _COMMAND_H */
