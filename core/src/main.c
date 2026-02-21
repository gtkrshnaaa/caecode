#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "sidebar.h"

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
    
    if (argc > 1) {
        if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            printf("Caecode version %s\n", VERSION);
            return 0;
        } else if (argv[1][0] != '-') {
            // Treat as a directory path
            char abs_path[PATH_MAX];
            if (realpath(argv[1], abs_path) != NULL) {
                struct stat path_stat;
                if (stat(abs_path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
                    target_dir = g_strdup(abs_path);
                } else {
                    fprintf(stderr, "Error: '%s' is not a valid directory\n", argv[1]);
                }
            } else {
                fprintf(stderr, "Error: Cannot resolve path '%s'\n", argv[1]);
            }
        }
    }
    
    gtk_init(&argc, &argv);
    
    create_main_window();
    
    if (target_dir) {
        g_idle_add(invoke_initial_folder_open, target_dir);
    }
    
    gtk_main();

    // Cleanup
    if (last_saved_content) g_free(last_saved_content);
    if (current_file_row_ref) gtk_tree_row_reference_free(current_file_row_ref);

    return 0;
}
