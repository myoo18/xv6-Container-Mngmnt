#include "cm.h"

//TESTING IF PARSE WORKS
int main() {
    char *path = "cmspec.json";
    char **specs = parse_specs(path);

    if (!specs) {
        printf(1, "Failed to parse.\n");
        exit();
    }

    // Use specs
    printf(1, "Init: %s\nFS: %s\nNproc: %s\n", specs[0], specs[1], specs[2]);

    // Free allocated memory
    for (int i = 0; i < 3; i++) {
        free(specs[i]);
    }
    free(specs);

    exit();
}
