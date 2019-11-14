#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int getaddrinfo(const char *__restrict node, const char *__restrict service, const struct addrinfo *__restrict hints, struct addrinfo **__restrict res) {
    (void) service;
    (void) hints;

    struct addrinfo *result = calloc(1, sizeof(struct addrinfo));
    *res = result;

    struct sockaddr_in *found = calloc(1, sizeof(struct sockaddr_in));
    found->sin_family = AF_INET;
    found->sin_port = 0;
    result->ai_addr = (struct sockaddr*) found;
    result->ai_addrlen = sizeof(struct sockaddr_in);

    if (node == NULL) {
        found->sin_addr.s_addr = INADDR_LOOBACK;
        return 0;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        free(result);
        free(found);
        return EAI_SYSTEM;
    }

    struct sockaddr_un conn = { 0 };
    conn.sun_family = AF_UNIX;
    strcpy(conn.sun_path, "/tmp/.nslookup.socket");

    if (connect(fd, (const struct sockaddr*) &conn, sizeof(struct sockaddr_un)) == -1) {
        free(result);
        free(found);
        close(fd);
        return EAI_SYSTEM;
    }

    size_t name_length = strlen(node);
    if (write(fd, node, name_length + 1) == -1) {
        free(result);
        free(found);
        close(fd);
        return EAI_SYSTEM;
    }

    char buf[16];
    if (read(fd, buf, 16) <= 0) {
        free(result);
        free(found);
        close(fd);
        return EAI_SYSTEM;
    }
    struct in_addr in_addr; 
    in_addr.s_addr = inet_addr(buf);
    if (in_addr.s_addr == INADDR_NONE) {
        free(result);
        free(found);
        close(fd);
        return EAI_NONAME;
    }

    found->sin_addr = in_addr;

    close(fd);
    return 0;
}

void freeaddrinfo(struct addrinfo *res) {
    while (res != NULL) {
        struct addrinfo *next = res->ai_next;
        free(res->ai_addr);
        free(res);
        res = next;
    }
}

int getnameinfo(const struct sockaddr *__restrict addr, socklen_t addrlen, char *__restrict host, socklen_t hostlen, char *__restrict serv, socklen_t servlen, int flags) {
    (void) flags;
    (void) host;
    (void) hostlen;
    (void) serv;
    (void) servlen;

    if (addr->sa_family != AF_INET || addrlen < sizeof(struct sockaddr_in)) {
        return EAI_FAMILY;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        return EAI_SYSTEM;
    }

    return EAI_NONAME;
}