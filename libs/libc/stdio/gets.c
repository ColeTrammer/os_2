#ifndef OLD_STDIO

#include <bits/lock.h>
#include <stdio.h>

__attribute__((deprecated)) char *gets(char *s) {
    __lock(&stdin->__lock);

    size_t index = 0;
    int c;
    while ((c = getchar_unlocked()) != EOF && c != '\n') {
        s[index++] = (char) c;
    }

    int error_set = stdin->__flags & __STDIO_ERROR;
    __unlock(&stdin->__lock);
    return (index == 0 || error_set) ? NULL : (s[index + 1] = '\0', s);
}

#endif /* OLD_STDIO */