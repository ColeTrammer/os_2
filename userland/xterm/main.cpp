#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <liim/pointers.h>
#include <liim/vector.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

#include <kernel/hal/input.h>

#include "terminal.h"
#include "tty.h"
#include "vga_buffer.h"

class Application {
public:
    Application() {
        for (int i = 0; i < 4; i++) {
            Terminal t;
            m_terminals.add(move(t));
        }
        current_tty().load(m_container);
        current_tty().vga_buffer().switch_to(nullptr);
    }

    int current_mfd() const { return current_tty().mfd(); }

    Terminal& current_tty() { return m_terminals[m_current_tty]; }
    const Terminal& current_tty() const { return m_terminals[m_current_tty]; }

private:
    int m_current_tty { 0 };
    Vector<Terminal> m_terminals;
    VgaBuffer::GraphicsContainer m_container;
};

int main() {
    // FIXME: There is some buf that makes it necessary to always ignore SIGTTOU,
    //        when only ignoring it for the tcsetpgrp calls should suffice
    signal(SIGTTOU, SIG_IGN);

    signal(SIGCHLD, [](auto) {
        int status;
        waitpid(-1, &status, WNOHANG);
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            exit(0);
        }
    });

    Application application;

    int kfd = open("/dev/keyboard", O_RDONLY);
    assert(kfd != -1);

    int mouse_fd = open("/dev/mouse", O_RDONLY);
    assert(mouse_fd != -1);

    char buf[4096];
    mouse_event mouse_event;
    key_event event;

    fd_set set;

    for (;;) {
        int mfd = application.current_mfd();
        assert(mfd != -1);
        auto& tty = application.current_tty().tty();

        FD_ZERO(&set);
        FD_SET(mouse_fd, &set);
        FD_SET(kfd, &set);
        FD_SET(mfd, &set);

        int ret = select(FD_SETSIZE, &set, nullptr, nullptr, nullptr);
        assert(ret != 0);
        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("select");
            return 1;
        }

        if (FD_ISSET(mouse_fd, &set)) {
            if (read(mouse_fd, &mouse_event, sizeof(struct mouse_event)) == sizeof(struct mouse_event)) {
                if (mouse_event.scroll_state == SCROLL_UP) {
                    tty.scroll_up();
                } else if (mouse_event.scroll_state == SCROLL_DOWN) {
                    tty.scroll_down();
                }
            }
        }

        if (FD_ISSET(kfd, &set)) {
            if (read(kfd, &event, sizeof(key_event)) == sizeof(key_event)) {
                if (event.flags & KEY_DOWN) {
                    if (event.flags & KEY_SHIFT_ON && !(event.flags & KEY_ALT_ON) && !(event.flags & KEY_CONTROL_ON)) {
                        if (event.key == KEY_HOME) {
                            tty.scroll_to_top();
                            continue;
                        } else if (event.key == KEY_END) {
                            tty.scroll_to_bottom();
                            continue;
                        }
                    }
                    if (event.flags & KEY_CONTROL_ON) {
                        event.ascii &= 0x1F;
                        switch (event.key) {
                            case KEY_BACKSPACE: {
                                // NOTE: no one knows what this should be...
                                char c = 'W' & 0x1F;
                                write(mfd, &c, 1);
                                break;
                            }
                            case KEY_DELETE:
                                write(mfd, "\033[3;5~", 6);
                                break;
                            case KEY_PAGE_UP:
                                write(mfd, "\033[5~", 4);
                                break;
                            case KEY_PAGE_DOWN:
                                write(mfd, "\033[6~", 4);
                                break;
                            case KEY_CURSOR_UP:
                                write(mfd, "\033[1;5A", 6);
                                break;
                            case KEY_CURSOR_DOWN:
                                write(mfd, "\033[1;5B", 6);
                                break;
                            case KEY_CURSOR_RIGHT:
                                write(mfd, "\033[1;5C", 6);
                                break;
                            case KEY_CURSOR_LEFT:
                                write(mfd, "\033[1;5D", 6);
                                break;
                            case KEY_HOME:
                                write(mfd, "\033[1;5H", 6);
                                break;
                            case KEY_END:
                                write(mfd, "\033[1;5F", 6);
                                break;
                            default:
                                if (event.ascii != '\0') {
                                    write(mfd, &event.ascii, 1);
                                }
                                break;
                        }
                    } else {
                        switch (event.key) {
                            case KEY_DELETE:
                                write(mfd, "\033[3~", 4);
                                break;
                            case KEY_PAGE_UP:
                                write(mfd, "\033[5~", 4);
                                break;
                            case KEY_PAGE_DOWN:
                                write(mfd, "\033[6~", 4);
                                break;
                            case KEY_CURSOR_UP:
                                write(mfd, "\033[A", 3);
                                break;
                            case KEY_CURSOR_DOWN:
                                write(mfd, "\033[B", 3);
                                break;
                            case KEY_CURSOR_RIGHT:
                                write(mfd, "\033[C", 3);
                                break;
                            case KEY_CURSOR_LEFT:
                                write(mfd, "\033[D", 3);
                                break;
                            case KEY_HOME:
                                write(mfd, "\033[H", 3);
                                break;
                            case KEY_END:
                                write(mfd, "\033[F", 3);
                                break;
                            default:
                                if (event.ascii != '\0') {
                                    write(mfd, &event.ascii, 1);
                                }
                                break;
                        }
                    }
                }
            }
        }

        if (FD_ISSET(mfd, &set)) {
            ssize_t bytes;
            if ((bytes = read(mfd, buf, 4096)) > 0) {
                for (int i = 0; i < bytes; i++) {
                    tty.on_char(buf[i]);
                }
            }

            if (bytes == -1) {
                perror("master read");
                return 1;
            }

            tty.refresh();
        }
    }

    return 0;
}