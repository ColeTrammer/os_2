#ifndef OLD_STDIO

#include <bits/lock.h>
#include <stdio.h>

int getchar_unlocked(void) {
    return fgetc_unlocked(stdin);
}

#endif /* OLD_STDIO */