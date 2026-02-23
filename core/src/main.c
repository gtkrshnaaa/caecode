#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "sidebar.h"
#include "editor.h"

static gboolean invoke_initial_folder_open(gpointer data) {
    char *path = (char *)data;
    if (path) {
        open_folder(path);
        g_free(path);
    }
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv) {
    char *target_dir = NULL;
    gboolean foreground = FALSE;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("Caecode version %s\n", VERSION);
            return 0;
        } else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--foreground") == 0) {
            foreground = TRUE;
        } else if (argv[i][0] != '-') {
            // Treat as a directory path
            char abs_path[PATH_MAX];
            if (realpath(argv[i], abs_path) != NULL) {
                struct stat path_stat;
                if (stat(abs_path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
                    target_dir = g_strdup(abs_path);
                } else {
                    fprintf(stderr, "Error: '%s' is not a valid directory\n", argv[i]);
                }
            } else {
                fprintf(stderr, "Error: Cannot resolve path '%s'\n", argv[i]);
            }
        }
    }

    // Detach from terminal unless in foreground mode
    if (!foreground) {
        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "Error: Failed to fork\n");
            exit(1);
        }
        if (pid > 0) {
            // Parent process exits, returning control to terminal
            exit(0);
        }

        // Child process continues here
        if (setsid() < 0) {
            fprintf(stderr, "Error: Failed to create new session\n");
            exit(1);
        }

        // Redirect standard I/O to /dev/null
        int fd = open("/dev/null", O_RDWR);
        if (fd != -1) {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > 2) close(fd);
        }
    }
    
    gtk_init(&argc, &argv);
    
    create_main_window();
    
    if (target_dir) {
        g_idle_add(invoke_initial_folder_open, target_dir);
    }
    
    gtk_main();

    // Cleanup: stop all timers and async operations before destroying widgets
    cleanup_sidebar();
    cleanup_editor();
    close_folder();
    if (last_saved_content) g_free(last_saved_content);
    if (current_file_row_ref) gtk_tree_row_reference_free(current_file_row_ref);

    return 0;
}
