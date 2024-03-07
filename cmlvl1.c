#include "types.h"
#include "user.h"
#include "fcntl.h"


//MAKE SURE TO RUN CMTESTENV BEFORE
int main(void) {

    printf(1, "making new directory for tests\n");

    //SETUP TESTING ENVIRONMENT
    int fd;
    //NEED TO CREATE NEW TXT FILE TO TEST READ WRITE OPEN TREATS NEW PATH AS ROOT
    char *file_path = "/test/root/helloworld.txt";


// -------------------------------------------------------------------------------
    printf(1, "-----------Starting setroot tests-----------\n");
    // TEST 1: SET ROOT WITH NON ABS PATH
    if (cm_setroot("test", strlen("test")) >= 0) {
        printf(1, "FAILED || rejection of non-absolute path\n");
        exit();
    } else {
        printf(1, "PASSED || rejection of non-absolute path\n");
    }

    // TEST 2: SET ROOT WITH NON EXISTENT PATH
    if (cm_setroot("/asdasd", strlen("test")) >= 0) {
        printf(1, "FAILED || rejection of non existing root in FS\n");
        exit();
    } else {
        printf(1, "PASSED || rejection of non existing root in FS\n");
    }

    // TEST 3: SET ROOT WITH ABS PATH
    if (cm_setroot(file_path, sizeof(file_path)) < 0) {
        printf(1, "PASSED || rejection set root to non-dir type\n");
    } else {
        printf(1, "FAILED || rejection set root to non-dir type\n");
        exit();
    }

    // TEST 3: SET ROOT WITH ABS PATH
    char test_path[] = "/test";
    if (cm_setroot(test_path, sizeof(test_path)) < 0) {
        printf(1, "FAILED || absolute and existing path accepted\n");
        exit();
    } else {
        printf(1, "PASSED || absolute and existing path accepted\n");
    }

    //TEST 4: CHECK STRUCT
    if (test_newroot(test_path, sizeof(test_path)) < 0) {
        printf(1, "FAILED || proc struct update\n");
        exit();
    } else {
        printf(1, "PASSED || proc struct update \n");
        // printf(1, "Root set to %s\n", test_path);
    }
    
    //TEST 5: CHECK IF CD.. DOES NOT GO PAST ROOT
    chdir(test_path);
    //try to cd out of root which is test_path
    chdir("..");
    if (test_dotdot() < 0) {
        printf(1, "FAILED || cd.. went pas the root\n");
        exit();
    } else {
        printf(1, "PASSED || cd.. did not go past the root\n");
    }

    // Test accessing the file with an absolute path (should fail)
    fd = open("/test/root/helloworld.txt", O_RDONLY);
    if (fd >= 0) {
        printf(1, "FAILED || able to open file with absolute path\n");
        close(fd);
        exit();
    } else {
        printf(1, "PASSED || could not open file with absolute path\n");
    }

    // Test accessing the file with a path relative to the new root (should pass)
    fd = open("/root/helloworld.txt", O_RDONLY);
    if (fd < 0) {
        printf(1, "FAILED || could not open file with relative path\n");
        exit();
    } else {
        printf(1, "PASSED || successfully opened file with relative path\n");
        close(fd);
    }

    // Test accessing the file with a path relative to the new root (should pass)
    fd = open("/helloworld.txt", O_RDONLY);
    if (fd < 0) {
        printf(1, "PASSED || could not open path with just file\n");
    } else {
        printf(1, "FAILED || successfully opened path with just file\n");
        close(fd);
        exit();
    }


    printf(1, "tests completed\n");
    exit();
}
