#include "types.h"
#include "user.h"
#include "jsmn.h"
#include "fcntl.h"


//fnc from the jsmn lib
//checks for token match
static int 
jsoneq(char *json, jsmntok_t *tok, char *s) 
{
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
	  //implementation of strncmp can be found in ulib.c
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}



char **
parse_specs(char *path)
{
    int fd;
    int size;
    char buffer[100];
    
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    size = read(fd, buffer, sizeof(buffer) - 1);
    if (size < 0) {
        close(fd);
        return NULL;
    }
    buffer[size] = '\0';

    if (close(fd) < 0) {
        return NULL;
    }

    jsmn_parser parser;
    jsmntok_t t[128];

    jsmn_init(&parser);
    int count = jsmn_parse(&parser, buffer, strlen(buffer), t, sizeof(t) / sizeof(t[0]));

    if (count < 1 || t[0].type != JSMN_OBJECT) {
        return NULL;
    }

    char **specs = malloc(sizeof(char *) * 3);
    if (specs == NULL) {
        return NULL;
    }

    for (int i = 1, j = 0; i < count && j < 3; i++) {
        if (jsoneq(buffer, &t[i], "init") == 0 || 
            jsoneq(buffer, &t[i], "fs") == 0 || 
            jsoneq(buffer, &t[i], "nproc") == 0) {

            int length = t[i + 1].end - t[i + 1].start;
            specs[j] = malloc(length + 1);
            if (specs[j] == NULL) {
                for (int k = 0; k < j; k++) {
                    free(specs[k]);
                }
                free(specs);
                return NULL;
            }

            safe_strcpy(specs[j], buffer + t[i + 1].start, length);
            specs[j][length] = '\0';
            j++;
            i++;
        }
    }
    return specs;
}



void containerstart(char *jsonFilePath) {
    int pid = fork();

    if (pid < 0) {
        //fork failed i guess
        printf(1, "Fork failed\n");
        return;
    } else if (pid == 0) {

        //parse JSON file
        char **specs = parse_specs(jsonFilePath);
        if (specs == NULL) {
            printf(1, "Failed to parse JSON file\n");
            exit();  //exit child process
        }

        //setroot
        cm_setroot(specs[1], strlen(specs[1]));
        //set cwd to newroot
        if (chdir(specs[1]) != 0) {
            printf(1, "Failed to change directory to %s\n", specs[1]);
            exit();
        }

        //maxproc
        cm_maxproc((int) specs[2]);

        //create and enter
        if (cm_create_and_enter() < 0) {
            printf(1, "Failed to set namespace (c and e syscall)\n");
            exit();
        }

        //init process
        char *execArgs[] = {specs[0], 0};
        exec(specs[0], execArgs);

        // If exec fail exit child proc
        printf(1, "Exec failed\n");
        for (int i = 0; i < 3; i++) {
            free(specs[i]);
        }
        free(specs);
        exit();
    } else {
        //parent process
        wait();  //wait for the child proc
    }
}
