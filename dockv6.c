#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "cm.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf(1, "Usage: dockv6 <path_to_json>\n");
        exit();
    }

    // Write to the shared file
    int fd = open("shared_file.txt", O_WRONLY | O_CREATE);
    write(fd, argv[1], strlen(argv[1]) + 1);
    close(fd);


    exit();
}