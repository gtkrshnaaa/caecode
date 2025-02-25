#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <glib.h>

GtkWidget *window, *tree_view, *text_view, *status_bar, *sidebar_scrolled_window, *scrolled_window, *box, *search_popup, *search_entry, *search_list;
GtkSourceBuffer *text_buffer;
GtkTreeStore *tree_store;
GList *file_list = NULL;
gboolean sidebar_visible = TRUE;
char current_file[1024] = "";
char *last_saved_content = NULL; // Saves the last saved content
GtkSourceStyleSchemeManager *theme_manager; // Manager for themes
gboolean is_dark_theme = TRUE; // Track current theme state

char current_folder[1024] = ""; // Saves the path of the currently opened folder


void select_file_in_sidebar(const char *filepath) {
    GtkTreeModel *model = GTK_TREE_MODEL(tree_store);
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            char *path;
            gtk_tree_model_get(model, &iter, 1, &path, -1);

            if (path && strcmp(path, filepath) == 0) {
                GtkTreePath *tree_path = gtk_tree_model_get_path(model, &iter);

                // Expand all parent folders
                gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree_view), tree_path);

                // Highlight selected files
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree_view), tree_path, NULL, FALSE);
                gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(tree_view), tree_path, NULL, TRUE, 0.5, 0.0);

                gtk_tree_path_free(tree_path);
                g_free(path);
                return;
            }

            // Check child nodes recursively
            GtkTreeIter child_iter;
            if (gtk_tree_model_iter_children(model, &child_iter, &iter)) {
                select_file_in_sidebar_recursive(model, &child_iter, filepath);
            }

            g_free(path);
        } while (gtk_tree_model_iter_next(model, &iter));
    }
}

// Recursive function with auto-expand parent folder
void select_file_in_sidebar_recursive(GtkTreeModel *model, GtkTreeIter *iter, const char *filepath) {
    do {
        char *path;
        gtk_tree_model_get(model, iter, 1, &path, -1);

        if (path && strcmp(path, filepath) == 0) {
            GtkTreePath *tree_path = gtk_tree_model_get_path(model, iter);

            // Expand all parent folders
            gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree_view), tree_path);

            // Highlight files
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree_view), tree_path, NULL, FALSE);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(tree_view), tree_path, NULL, TRUE, 0.5, 0.0);

            gtk_tree_path_free(tree_path);
            g_free(path);
            return;
        }

        // Recursive for child iter
        GtkTreeIter child_iter;
        if (gtk_tree_model_iter_children(model, &child_iter, iter)) {
            select_file_in_sidebar_recursive(model, &child_iter, filepath);
        }

        g_free(path);
    } while (gtk_tree_model_iter_next(model, iter));
}


// Load file content into the text view
void load_file(const char *filepath) {
    FILE *file = fopen(filepath, "r");
    if (!file) return;

    strncpy(current_file, filepath, sizeof(current_file));

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char *content = malloc(length + 1);
    fread(content, 1, length, file);
    content[length] = '\0';

    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(text_buffer), content, -1);

    // Save the last saved content snapshot
    if (last_saved_content) free(last_saved_content);
    last_saved_content = strdup(content);

    free(content);
    fclose(file);

    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
    GtkSourceLanguage *language = gtk_source_language_manager_guess_language(lm, filepath, NULL);
    gtk_source_buffer_set_language(text_buffer, language);

    mark_unsaved_file(filepath, FALSE); // Mark as saved

    update_status_with_relative_path();

    select_file_in_sidebar(filepath);

}


// Save the current file
void save_file() {
    if (strlen(current_file) == 0) return;

    FILE *file = fopen(current_file, "w");
    if (!file) return;

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(text_buffer), &start, &end);
    char *text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(text_buffer), &start, &end, FALSE);
    fprintf(file, "%s", text);
    fclose(file);

    // Update the last saved content snapshot
    if (last_saved_content) free(last_saved_content);
    last_saved_content = strdup(text);

    g_free(text);

    mark_unsaved_file(current_file, FALSE); // Remove mark after save
    update_status_with_unsaved_mark();
}


// Update status bar with the relative path of the current file
void update_status_with_relative_path() {
    if (strlen(current_file) == 0) {
        set_status_message("No file opened");
        return;
    }

    if (strlen(current_folder) == 0) {
        set_status_message("No folder opened");
        return;
    }

    // Get the relative path
    char relative_path[1024];
    if (g_str_has_prefix(current_file, current_folder)) {
        snprintf(relative_path, sizeof(relative_path), "%s", current_file + strlen(current_folder) + 1); // +1 to skip the '/'
    } else {
        snprintf(relative_path, sizeof(relative_path), "%s", current_file); // Use absolute path if not in base path
    }

    // Update status bar with the relative path
    set_status_message(relative_path);
}


void update_status_with_unsaved_mark() {
    if (strlen(current_file) == 0) {
        set_status_message("No file opened");
        return;
    }

    if (strlen(current_folder) == 0) {
        set_status_message("No folder opened");
        return;
    }

    // Get relative path
    char relative_path[1024];
    if (g_str_has_prefix(current_file, current_folder)) {
        snprintf(relative_path, sizeof(relative_path), "%s", current_file + strlen(current_folder) + 1);
    } else {
        snprintf(relative_path, sizeof(relative_path), "%s", current_file);
    }

    // Check if there are any changes to the currently opened file.
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(text_buffer), &start, &end);
    char *current_text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(text_buffer), &start, &end, FALSE);

    gboolean is_same = (last_saved_content && strcmp(current_text, last_saved_content) == 0);
    g_free(current_text);

    // Add '*' mark if the file is not saved yet.
    char display_path[1050]; // Buffer for relative path + " *"
    snprintf(display_path, sizeof(display_path), "%s%s", relative_path, is_same ? "" : " *");

    // Update status bar
    set_status_message(display_path);
}

// Comparator to sort file/folder names
gint sort_names(gconstpointer a, gconstpointer b) {
    const char *name_a = *(const char **)a;
    const char *name_b = *(const char **)b;
    return g_strcmp0(name_a, name_b);
}

// Recursively populate the tree store and file list
void populate_tree(const char *folder_path, GtkTreeIter *parent) {
    DIR *dir = opendir(folder_path);
    if (!dir) return;

    struct dirent *entry;
    GList *folders = NULL; // List for folders
    GList *files = NULL;   // List for files

    // Pisahkan folder dan file
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", folder_path, entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                folders = g_list_insert_sorted(folders, g_strdup(entry->d_name), (GCompareFunc)g_ascii_strcasecmp);
            } else {
                files = g_list_insert_sorted(files, g_strdup(entry->d_name), (GCompareFunc)g_ascii_strcasecmp);
            }
        }
    }
    closedir(dir);

    // Show folders first
    for (GList *l = folders; l != NULL; l = l->next) {
        const char *folder_name = (const char *)l->data;
        char display_name[1024];
        snprintf(display_name, sizeof(display_name), "%s/", folder_name); // Add "/"

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, folder_name);

        GtkTreeIter iter;
        gtk_tree_store_append(tree_store, &iter, parent);
        gtk_tree_store_set(tree_store, &iter, 0, display_name, 1, g_strdup(full_path), -1);

        // Recursive folder contents
        populate_tree(full_path, &iter);
    }

    // Show files after folder
    for (GList *l = files; l != NULL; l = l->next) {
        const char *file_name = (const char *)l->data;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, file_name);

        GtkTreeIter iter;
        gtk_tree_store_append(tree_store, &iter, parent);
        gtk_tree_store_set(tree_store, &iter, 0, file_name, 1, g_strdup(full_path), -1);
        file_list = g_list_append(file_list, g_strdup(full_path));
    }

    // Clear list memory
    g_list_free_full(folders, g_free);
    g_list_free_full(files, g_free);
}



// Open file on row activation in the sidebar
void on_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data) {
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        char *filepath;
        gtk_tree_model_get(model, &iter, 1, &filepath, -1);

        if (filepath) {
            struct stat st;
            if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
                load_file(filepath);
            }
            g_free(filepath);
        }
    }
}

// Open a folder dialog
void open_folder(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open Folder",
        GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *folder_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        strncpy(current_folder, folder_path, sizeof(current_folder)); // Save the currently opened folder


        gtk_tree_store_clear(tree_store);
        g_list_free_full(file_list, g_free); // Clear previous file list
        file_list = NULL; // Reset file list
        populate_tree(folder_path, NULL);

        g_free(folder_path);
    }

    gtk_widget_destroy(dialog);
}

// Toggle sidebar
void toggle_sidebar(GtkWidget *widget, gpointer data) {
    if (GTK_IS_WIDGET(sidebar_scrolled_window)) {
        sidebar_visible = !sidebar_visible;
        gtk_widget_set_visible(sidebar_scrolled_window, sidebar_visible);
    }
}

// Close search popup
void close_search_popup() {
    gtk_widget_hide(search_popup);
}

// Filter files based on search query
void filter_file_list(const gchar *query) {
    GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(search_list)));
    gtk_list_store_clear(list_store);

    gboolean first_selected = FALSE; 
    GtkTreeIter iter;

    for (GList *l = file_list; l != NULL; l = l->next) {
        const char *filepath = (const char *)l->data;
        if (g_strrstr(filepath, query) || strlen(query) == 0) {
            gtk_list_store_append(list_store, &iter);
            gtk_list_store_set(list_store, &iter, 0, filepath, -1);

            // Auto-select first result
            if (!first_selected) {
                GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(list_store), &iter);
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(search_list), path, NULL, FALSE);
                gtk_tree_path_free(path);
                first_selected = TRUE;
            }
        }
    }
}


// Activate file from search result
void activate_file_from_search() {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(search_list));
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *filepath;
        gtk_tree_model_get(model, &iter, 0, &filepath, -1);
        load_file(filepath);
        g_free(filepath);
    }
    close_search_popup();
}

// Handle search entry input
void on_search_entry_changed(GtkEditable *editable, gpointer user_data) {
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(search_entry));
    filter_file_list(text);
}

// Show search popup
void show_search_popup() {
    gtk_widget_show_all(search_popup);
    gtk_widget_grab_focus(search_entry);
    filter_file_list(""); 
}


void reload_folder() {
    if (strlen(current_folder) == 0) {
        // Tidak ada folder yang terbuka
        return;
    }

    // Clear the current view
    gtk_tree_store_clear(tree_store);
    g_list_free_full(file_list, g_free);  // Clear previous file list
    file_list = NULL;  // Reset file list

    // Reload the folder by populating it again
    populate_tree(current_folder, NULL);
}


// Handle search popup key events
gboolean on_search_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    if (event->keyval == GDK_KEY_Escape) {
        close_search_popup();
        return TRUE;
    } else if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        activate_file_from_search();
        return TRUE;
    }
    return FALSE;
}

// Keyboard shortcuts
gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    if ((event->state & GDK_CONTROL_MASK) != 0) {
        switch (event->keyval) {
            case GDK_KEY_o: open_folder(NULL, NULL); return TRUE;
            case GDK_KEY_b: toggle_sidebar(NULL, NULL); return TRUE;
            case GDK_KEY_s: save_file(); return TRUE;
            case GDK_KEY_m: switch_theme(); return TRUE; // Ctrl + M for switching theme
            case GDK_KEY_p: show_search_popup(); return TRUE; // Open search popup
            case GDK_KEY_c: gtk_text_buffer_copy_clipboard(GTK_TEXT_BUFFER(text_buffer), gtk_clipboard_get(GDK_SELECTION_CLIPBOARD)); return TRUE;
            case GDK_KEY_v: gtk_text_buffer_paste_clipboard(GTK_TEXT_BUFFER(text_buffer), gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), NULL, TRUE); return TRUE;
            case GDK_KEY_x: gtk_text_buffer_cut_clipboard(GTK_TEXT_BUFFER(text_buffer), gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), TRUE); return TRUE;
            case GDK_KEY_z: if (gtk_source_buffer_can_undo(text_buffer)) gtk_source_buffer_undo(text_buffer); return TRUE;
            case GDK_KEY_y: if (gtk_source_buffer_can_redo(text_buffer)) gtk_source_buffer_redo(text_buffer); return TRUE;
            case GDK_KEY_r: reload_folder(); return TRUE;
        }
    }
    return FALSE;
}

// Initialize search popup
void init_search_popup() {
    search_popup = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_transient_for(GTK_WINDOW(search_popup), GTK_WINDOW(window));
    // gtk_window_set_title(GTK_WINDOW(search_popup), "Search File");
    gtk_window_set_default_size(GTK_WINDOW(search_popup), 600, 300);
    gtk_window_set_modal(GTK_WINDOW(search_popup), TRUE);
    gtk_window_set_position(GTK_WINDOW(search_popup), GTK_WIN_POS_CENTER);
    gtk_window_set_decorated(GTK_WINDOW(search_popup), FALSE);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(search_popup), vbox);

    search_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), search_entry, FALSE, FALSE, 0);
    g_signal_connect(search_entry, "changed", G_CALLBACK(on_search_entry_changed), NULL);
    g_signal_connect(search_popup, "key-press-event", G_CALLBACK(on_search_key_press), NULL);

    GtkListStore *list_store = gtk_list_store_new(1, G_TYPE_STRING);
    search_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("File Path", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(search_list), column);

    // Add Gtk Scrolled Window so it can be scrolled
    GtkWidget *scrollable_list = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollable_list), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrollable_list), search_list);

    gtk_box_pack_start(GTK_BOX(vbox), scrollable_list, TRUE, TRUE, 0);
}



void mark_unsaved_file_recursive(GtkTreeModel *model, GtkTreeIter *iter, const char *filepath, gboolean unsaved) {
    do {
        char *path;
        gtk_tree_model_get(model, iter, 1, &path, -1);

        if (path && strcmp(path, filepath) == 0) {
            char *filename = g_path_get_basename(filepath);
            char display_name[1024];

            // Add '*' mark if the file is not saved.
            snprintf(display_name, sizeof(display_name), "%s%s", filename, unsaved ? " *" : "");
            gtk_tree_store_set(GTK_TREE_STORE(model), iter, 0, display_name, -1);

            g_free(filename);
            g_free(path);
            return; // File found, exiting function
        }

        // Check if iter has children (for folders)
        GtkTreeIter child_iter;
        if (gtk_tree_model_iter_children(model, &child_iter, iter)) {
            mark_unsaved_file_recursive(model, &child_iter, filepath, unsaved);
        }

        g_free(path);
    } while (gtk_tree_model_iter_next(model, iter)); // Continue to the next node
}



void mark_unsaved_file(const char *filepath, gboolean unsaved) {
    GtkTreeModel *model = GTK_TREE_MODEL(tree_store);
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter_first(model, &iter)) {
        mark_unsaved_file_recursive(model, &iter, filepath, unsaved);
    }
}



void on_text_changed(GtkTextBuffer *buffer, gpointer user_data) {
    if (strlen(current_file) > 0 && last_saved_content) {
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        char *current_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

        // Compare the current text with the last saved snapshot.
        gboolean is_same = strcmp(current_text, last_saved_content) == 0;

        mark_unsaved_file(current_file, !is_same); // Remove mark if same
        update_status_with_unsaved_mark(); // Update status bar with unsaved mark

        g_free(current_text);
    }
}

void switch_theme() {
    if (!theme_manager) {
        theme_manager = gtk_source_style_scheme_manager_get_default();
    }

    static const char *themes[] = {
        "Yaru-dark", "Yaru", "classic", "cobalt", "kate", "oblivion", "solarized-dark",
        "solarized-light", "tango"
    };
    static int current_theme = 0;

    current_theme = (current_theme + 1) % (sizeof(themes) / sizeof(themes[0]));
    GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme(theme_manager, themes[current_theme]);

    if (scheme) {
        gtk_source_buffer_set_style_scheme(text_buffer, scheme);
    }
}


// Function to add status to status bar
void set_status_message(const char *message) {
    gtk_statusbar_pop(GTK_STATUSBAR(status_bar), 0); // Delete old messages
    gtk_statusbar_push(GTK_STATUSBAR(status_bar), 0, message); // Show new messages
}


// Initialize UI
void activate(GtkApplication *app, gpointer user_data) {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Caecode");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);  // Use GTK_ORIENTATION_VERTICAL to arrange vertically
    gtk_container_add(GTK_CONTAINER(window), box);

    // Containers for sidebar and editor
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(box), main_box, TRUE, TRUE, 0);

    // Tree view sidebar
    tree_store = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_store));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Files", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    g_signal_connect(tree_view, "row-activated", G_CALLBACK(on_row_activated), NULL);
    sidebar_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar_scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sidebar_scrolled_window), tree_view);
    gtk_widget_set_size_request(sidebar_scrolled_window, 200, -1);
    gtk_box_pack_start(GTK_BOX(main_box), sidebar_scrolled_window, FALSE, FALSE, 0);

    // Scrolled window for text editor
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    text_buffer = GTK_SOURCE_BUFFER(gtk_source_buffer_new(NULL));
    text_view = gtk_source_view_new_with_buffer(text_buffer);

    // Set tab width to 2 spaces
    gtk_source_view_set_tab_width(GTK_SOURCE_VIEW(text_view), 4);
    gtk_source_view_set_insert_spaces_instead_of_tabs(GTK_SOURCE_VIEW(text_view), TRUE);

    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(text_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_NONE);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(text_view), 25);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(text_view), 200);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text_view), 15);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text_view), 25);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    gtk_box_pack_start(GTK_BOX(main_box), scrolled_window, TRUE, TRUE, 0);

    // Adding fonts
    PangoFontDescription *font_desc = pango_font_description_from_string("Monospace Bold 11");
    gtk_widget_override_font(text_view, font_desc);
    pango_font_description_free(font_desc);

    // Initialize the status bar below
    status_bar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(box), status_bar, FALSE, FALSE, 0);

    gtk_widget_set_margin_top(status_bar, 1);   // Small top margin
    gtk_widget_set_margin_bottom(status_bar, 1); // Small bottom margin

    // Add file path placeholder to status bar
    update_status_with_relative_path();


    g_signal_connect(text_buffer, "changed", G_CALLBACK(on_text_changed), NULL);

    init_search_popup();

    theme_manager = gtk_source_style_scheme_manager_get_default();
    GtkSourceStyleScheme *default_scheme = gtk_source_style_scheme_manager_get_scheme(theme_manager, "Yaru-dark");
    if (default_scheme) {
        gtk_source_buffer_set_style_scheme(text_buffer, default_scheme);
    }

    gtk_widget_show_all(window);
}

// Main function
int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    activate(NULL, NULL);
    gtk_main();

    if (last_saved_content) {
        free(last_saved_content);
    }

    return 0;
}