#include <stdbool.h>
#include <string.h>

size_t strspn(const char *s, const char *accept) {
    size_t i = 0;
    while (s[i] != '\0') {
        bool found = false;
        for (size_t j = 0; accept[j] != '\0'; j++) {
            if (s[i] == accept[j]) {
                found = true;
                break;
            }
        }

        if (!found) {
            break;
        }

        i++;
    }

    return i;
}
