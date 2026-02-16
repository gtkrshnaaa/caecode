#include "ui.h"

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    
    create_main_window();
    
    gtk_main();

    // Cleanup
    if (last_saved_content) g_free(last_saved_content);
    if (current_file_row_ref) gtk_tree_row_reference_free(current_file_row_ref);

    return 0;
}
