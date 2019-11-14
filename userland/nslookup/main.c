#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <search.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "dns.h"
#include "mapping.h"
#include "server.h"

static void print_usage(char **argv);

int main(int argc, char **argv) {
    if (argc != 2) {
        print_usage(argv);
        return 0;
    }

    int opt;
    while ((opt = getopt(argc, argv, "s")) != -1) {
        switch (opt) {
            case 's':
                return start_server();
            default:
                print_usage(argv);
                return 0;
        }
    }

    char *host = argv[optind];
    fprintf(stderr, "Looking up %s\n", host);

    // try and see if the url is an ip address
    struct in_addr a;
    a.s_addr = inet_addr(host);
    if (a.s_addr != INADDR_NONE) {
        printf("%s\n", host);
        return 0;
    }

    struct host_mapping *known_hosts = get_known_hosts();
    struct host_mapping *m = known_hosts;
    do {
        if (strcmp(host, m->name) == 0) {
            printf("%s\n", inet_ntoa(m->ip));
            return 0;
        }
    } while ((m = m->next) != known_hosts);

    struct host_mapping *result = lookup_host(host);
    if (result == NULL) {
        printf("Cannot determine ip\n");
        return 1;
    }

    printf("%s\n", inet_ntoa(result->ip));
    return 0;
}

void print_usage(char **argv) {
    fprintf(stderr, "Usage: %s [-s] <host>\n", argv[0]);
}