#include "ui.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1) {
        if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            printf("Caecode version %s\n", VERSION);
            return 0;
        }
    }
    gtk_init(&argc, &argv);
    
    create_main_window();
    
    gtk_main();

    // Cleanup
    if (last_saved_content) g_free(last_saved_content);
    if (current_file_row_ref) gtk_tree_row_reference_free(current_file_row_ref);

    return 0;
}
