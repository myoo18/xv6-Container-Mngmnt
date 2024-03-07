#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "cm.h"

//LETS ENTER 1 CONTAINER!
int main(void) {
	printf(1, "Starting container setup test\n");

    // Specify the path to your JSON file here
    char *jsonFilePath = "/cmspec.json";

    //start cont function
    containerstart(jsonFilePath);

    // The containersetup function will handle the process creation,
    // environment setup, and execution of the specified program.

    // Post-setup verification can go here, if possible.
    // Note: Most verification will likely occur within the containersetup function,
    // especially since the child process will exec a new program.

    printf(1, "Container setup test completed\n");

    exit();
}
