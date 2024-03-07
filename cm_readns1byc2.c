#include "types.h"
#include "user.h"
#include "fcntl.h"

#define BUFFER_SIZE 100

int main(void) {
    // Attempt to open a file from /test/ns1
    // If the isolation works, this should fail
    int fd = open("/../ns1/newfile.txt", O_RDONLY);  // Trying to access /test/ns1/newfile.txt
    if (fd < 0) {
        printf(1, "Failed to open file from ns1. Namespace isolation working as expected.\n");
        exit();
    }

    char buffer[BUFFER_SIZE];
    int bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        printf(1, "Failed to read file from ns1.\n");
    } else {
        // Null-terminate the string
        buffer[bytes_read] = '\0';
        printf(1, "Data read from ns1 file: %s\n", buffer);
    }

    close(fd);
    exit();
}
