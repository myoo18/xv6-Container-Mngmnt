#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "cm.h"

int main(void) {

    char buf[1024];

    // Read from the shared file
    int fd = open("shared_file.txt", O_RDONLY);
    int n = read(fd, buf, sizeof(buf) - 1);
    buf[n] = 0; // Null-terminate the string
    close(fd);

    printf(1, "STARTING CONTAINER ROOTED AT PATH: %s\n", buf);

    containerstart(buf);


    exit();
}