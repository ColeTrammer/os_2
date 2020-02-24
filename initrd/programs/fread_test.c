#include <stdio.h>

int main() {
    FILE* f = fopen("/bin/ls", "r");

    {
        char buf[8];
        fread(buf, 8, 1, f);
    }
    {
        fseek(f, 0, SEEK_SET);
        char buf[16];
        fread(buf, 16, 1, f);
    }
    {
        char buf[48];
        fread(buf, 28, 1, f);
    }
    {
        fseek(f, 63448, SEEK_SET);
        char buf[64];
        fread(buf, 64, 1, f);
    }
    {
        fseek(f, 63448, SEEK_SET);
        char buf[64 * 9];
        fread(buf, 64, 9, f);
    }

    fseek(f, 63390, SEEK_SET);
    char b[58];
    size_t ret = fread(b, 1, 58, f);
    fprintf(stderr, "ret=%lu\n", ret);
    fclose(f);
    return 0;
}