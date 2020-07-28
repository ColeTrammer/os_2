#pragma once

#include <stdarg.h>
#include <stdint.h>
#include <sys/os_2.h>
#include <sys/types.h>

#define _STRING_H 1
#define _STDLIB_H 1
#define _UNISTD_H 1

#define NDEBUG
#define LOADER_PRIVATE __attribute__((noplt)) __attribute__((visibility("internal")))
#ifndef loader_log
#define loader_log(m, ...) dprintf(2, m "\n" __VA_OPT__(, ) __VA_ARGS__)
#endif /* loader_log */

#ifdef __cplusplus
extern "C" {
#endif /* __cpluplus */

struct stat;

void _exit(int status) LOADER_PRIVATE __attribute__((noreturn));
ssize_t write(int fd, const void *buffer, size_t len) LOADER_PRIVATE;
void *sbrk(intptr_t increment) LOADER_PRIVATE;
int os_mutex(unsigned int *__protected, int op, int expected, int to_place, int to_wake, unsigned int *to_wait) LOADER_PRIVATE;
int open(const char *path, int flags, ...) LOADER_PRIVATE;
int fstat(int fd, struct stat *st) LOADER_PRIVATE;
int close(int fd) LOADER_PRIVATE;
void *mmap(void *base, size_t size, int prot, int flags, int fd, off_t offset) LOADER_PRIVATE;
int mprotect(void *base, size_t size, int prot) LOADER_PRIVATE;
int munmap(void *base, size_t size) LOADER_PRIVATE;
size_t strlen(const char *s) LOADER_PRIVATE;
void *memmove(void *dst, const void *src, size_t n) LOADER_PRIVATE;
int strcmp(const char *s1, const char *s2) LOADER_PRIVATE;
void *memset(void *s, int c, size_t n) LOADER_PRIVATE;
char *strchr(const char *s, int c) LOADER_PRIVATE;
int memcmp(const void *s1, const void *s2, size_t n) LOADER_PRIVATE;
void *memcpy(void *__restrict dest, const void *__restrict src, size_t n) LOADER_PRIVATE;
char *strcpy(char *__restrict dest, const char *__restrict src) LOADER_PRIVATE;

int dprintf(int fd, const char *__restrict format, ...) LOADER_PRIVATE __attribute__((format(printf, 2, 3)));
int vdprintf(int fd, const char *__restrict format, va_list args) LOADER_PRIVATE __attribute__((format(printf, 2, 0)));

void __lock(unsigned int *lock) LOADER_PRIVATE;
void __unlock(unsigned int *lock) LOADER_PRIVATE;

void insque(void *elem, void *prev) LOADER_PRIVATE;
void remque(void *elem) LOADER_PRIVATE;

void *loader_malloc(size_t n) LOADER_PRIVATE;
void *loader_calloc(size_t n, size_t m) LOADER_PRIVATE;
void *loader_realloc(void *p, size_t n) LOADER_PRIVATE;
void loader_free(void *p) LOADER_PRIVATE;

#ifdef __cplusplus
}
#endif /* __cpluplus */