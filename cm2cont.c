#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "cm.h"


int main(void) {
	printf(1, "Starting container setup test\n");

    // Specify the path to your JSON file here
    char *jsonFilePath1 = "/cmspec_cont1.json";
	char *jsonFilePath2 = "/cmspec_cont2.json";

    //start cont function
    containerstart(jsonFilePath1);
	containerstart(jsonFilePath2);

    // The containersetup function will handle the process creation,
    // environment setup, and execution of the specified program.

    // Post-setup verification can go here, if possible.
    // Note: Most verification will likely occur within the containersetup function,
    // especially since the child process will exec a new program.

    printf(1, "Container setup test completed\n");

    exit();
}
