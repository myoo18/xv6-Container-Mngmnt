#include "types.h"
#include "user.h"
#include "fcntl.h"

int main(void) {

    int fd = open("/newfile.txt", O_CREATE | O_RDWR);  // This will create /test/ns1/newfile.txt
    if (fd < 0) {
        printf(1, "failed to create new file\n");
        exit();
    }

    char *content = "IM IN CONTAINER 1\n";
    if (write(fd, content, strlen(content)) != strlen(content)) {
        printf(1, "failed to write to new file\n");
        close(fd);
        exit();
    }

    close(fd);
    printf(1, "Successfully wrote to /newfile.txt\n");
    exit();
}
