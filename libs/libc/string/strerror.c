#include <string.h>
#include <errno.h>

const char *sys_errlist[] = {
    "Success",
    "IO error",
    "Ran out of memory",
    "Invalid arguments",
    "File does not exist",
    "File cannot be executed",
    "Path is a directory",
    "Range",
    "No space left on device",
    "Invalid command for device",
    "File exists",
    "Bad file descriptor",
    "Directory is not empty",
    "Not a directory",
    "Permission denied",
    "Address family not supported",
    "Address in use",
    "Address not available",
    "Is connected",
    "To big",
    "Domain error",
    "Would page fault",
    "Bad file descriptor",
    "Pipe disconnected",
    "Connection aborted",
    "Already",
    "Connection refused",
    "Connection reset",
    "EXDEV",
    "Destination addr request",
    "Busy",
    "EFBIG",
    "Name too long",
    "System call unsupported",
    "Host unreachable",
    "Interrupted",
    "ESPIPE",
    "Message size",
    "Network down",
    "Network reset",
    "Network unreachable",
    "ENOBUFS",
    "Child",
    "ENOLCK",
    "No message",
    "Unsupported protocol option",
    "ENXIO",
    "No such device",
    "Search failed",
    "Not a socket",
    "Not connected",
    "In progress",
    "Operation unsupported",
    "Would block",
    "Access denied",
    "Protocol unsupported",
    "Read only file system",
    "Dead lock",
    "Timed out",
    "ENFILE",
    "EMFILE",
    "EMLINK",
    "Symbolic link loop",
    "Prototype",
    "EILSEQ",
    "Errno end"
};

int sys_nerr = EMAXERRNO;

char *strerror(int errnum) {
    if (errnum >= sys_nerr || errnum < 0) {
        errno = EINVAL;
        return "Invalid Errno";
    }

    return (char*) sys_errlist[errnum];
}