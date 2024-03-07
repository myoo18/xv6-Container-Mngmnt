#include "types.h"
#include "user.h"
#include "fcntl.h"


//SET UP YOUR ENVIRONMENT FOR TESTING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
int main(void) {
    printf(1, "making new directory for tests\n");

    //SETUP TESTING ENVIRONMENT
    char *dir_path1 = "/test";
    char *dir_path2 = "/test/root";
    char *dir_path3 = "/test/ns1";
    char *dir_path4 = "/test/ns2";

    if (mkdir(dir_path1) < 0) {
        printf(1, "failed to make dir: %s\n", dir_path1);
        exit();
    } else {
        printf(1, "dir: %s successfully made\n", dir_path1);
    }

    if (mkdir(dir_path2) < 0) {
        printf(1, "failed to make dir: %s\n", dir_path2);
        exit();
    } else {
        printf(1, "dir: %s successfully made\n", dir_path2);
    }

    if (mkdir(dir_path3) < 0) {
        printf(1, "failed to make dir: %s\n", dir_path2);
        exit();
    } else {
        printf(1, "dir: %s successfully made\n", dir_path2);
    }

    if (mkdir(dir_path4) < 0) {
        printf(1, "failed to make dir: %s\n", dir_path2);
        exit();
    } else {
        printf(1, "dir: %s successfully made\n", dir_path2);
    }

    //NEED TO CREATE NEW TXT FILE TO TEST READ WRITE OPEN TREATS NEW PATH AS ROOT
    char *file_path = "/test/root/helloworld.txt";
    int fd = open(file_path, O_CREATE | O_RDWR);
    if (fd < 0) {
        printf(1, "failed to create file: %s\n", file_path);
        exit();
    } else {
        printf(1, "file: %s successfully created\n", file_path);
    }

    //write to the file
    char *content = "Hello, world!\n";
    if (write(fd, content, strlen(content)) != strlen(content)) {
        printf(1, "failed to write to file: %s\n", file_path);
        close(fd);
        exit();
    }

    // Close the file
    close(fd);

    // Create links for executables in /test
    char *executables[] = { "sh", "ls", "cat", "cm_maxproctest" };  // Executable names
    char link_path[50];

    for (int i = 0; i < sizeof(executables) / sizeof(executables[0]); i++) {

        memset(link_path, 0, sizeof(link_path));


        strconcat(link_path, dir_path1, "/");
        strconcat(link_path, link_path, executables[i]);

        if (link(executables[i], link_path) < 0) {
            printf(1, "failed to create link for %s at %s\n", executables[i], link_path);
            exit();
        } else {
            printf(1, "link created for %s at %s\n", executables[i], link_path);
        }
    }

    //paths for multicontainer tests
    char *source_path_writens1 = "/cm_writens1byc1"; // Replace with the actual path of writens1byc1 executable
    char *dest_path_writens1 = "/test/ns1/cm_writens1byc1";

    if (link(source_path_writens1, dest_path_writens1) < 0) {
        printf(1, "failed to create link for %s at %s\n", source_path_writens1, dest_path_writens1);
        exit();
    } else {
        printf(1, "link created for %s at %s\n", source_path_writens1, dest_path_writens1);
    }

    char *source_path_readns1 = "/cm_readns1byc2"; // Replace with the actual path of readns1byc2 executable
    char *dest_path_readns1 = "/test/ns2/cm_readns1byc2";

    if (link(source_path_readns1, dest_path_readns1) < 0) {
        printf(1, "failed to create link for %s at %s\n", source_path_readns1, dest_path_readns1);
        exit();
    } else {
        printf(1, "link created for %s at %s\n", source_path_readns1, dest_path_readns1);
    }



    printf(1, "Environment setup complete\n");
    exit();
}
