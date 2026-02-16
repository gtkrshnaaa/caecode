#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <glib.h>
#include <gio/gio.h>

GtkWidget *window, *tree_view, *text_view, *status_bar, *sidebar_scrolled_window, *scrolled_window, *box, *search_popup, *search_entry, *search_list;
GtkSourceBuffer *text_buffer;
GtkTreeStore *tree_store;
GList *file_list = NULL;
gboolean sidebar_visible = TRUE;
char current_file[1024] = "";
char *last_saved_content = NULL; // Saves the last saved content
GtkSourceStyleSchemeManager *theme_manager; // Manager for themes
GtkTreeRowReference *current_file_row_ref = NULL; // Reference to the currently opened file in the sidebar

char current_folder[1024] = ""; // Saves the path of the currently opened folder

// --- Async population state ---
typedef struct {
    char *path;
    GtkTreeIter parent_iter;   // valid only if has_parent == TRUE
    gboolean has_parent;
} DirTask;

typedef struct {
    GQueue *queue;        // queue of DirTask*
    gboolean cancelled;   // set TRUE to cancel ongoing population
    guint source_id;      // idle source id
} PopulateContext;

static PopulateContext *populate_ctx = NULL;

// --- Forward declarations ---
void set_status_message(const char *message);
void update_status_with_relative_path();
void update_status_with_unsaved_mark(gboolean is_same);
void mark_unsaved_file(const char *filepath, gboolean unsaved);
void switch_theme();
void select_file_in_sidebar_recursive(GtkTreeModel *model, GtkTreeIter *iter, const char *filepath);
void load_file_async(const char *filepath);
static void on_window_destroy(GtkWidget *w, gpointer user_data);


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

                // Store row reference for fast access
                if (current_file_row_ref) gtk_tree_row_reference_free(current_file_row_ref);
                current_file_row_ref = gtk_tree_row_reference_new(model, tree_path);

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

            // Store row reference for fast access
            if (current_file_row_ref) gtk_tree_row_reference_free(current_file_row_ref);
            current_file_row_ref = gtk_tree_row_reference_new(model, tree_path);

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


// Async file load helpers
typedef struct {
    char *path;
} LoadCtx;

static void on_file_loaded(GObject *src, GAsyncResult *res, gpointer user_data) {
    LoadCtx *ctx = (LoadCtx *)user_data;
    gsize len = 0;
    char *contents = NULL;
    GError *err = NULL;
    gboolean ok = g_file_load_contents_finish(G_FILE(src), res, &contents, &len, NULL, &err);
    if (!ok) {
        if (err) {
            set_status_message(err->message);
            g_error_free(err);
        }
        g_free(ctx->path);
        g_free(ctx);
        return;
    }

    // Apply to UI (main thread)
    g_strlcpy(current_file, ctx->path, sizeof(current_file));
    // Prevent this programmatic load from being added to the undo history
    gtk_source_buffer_begin_not_undoable_action(text_buffer);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(text_buffer), contents, (gint)len);
    gtk_source_buffer_end_not_undoable_action(text_buffer);

    if (last_saved_content) g_free(last_saved_content);
    last_saved_content = g_strdup(contents);

    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
    GtkSourceLanguage *language = gtk_source_language_manager_guess_language(lm, ctx->path, NULL);
    gtk_source_buffer_set_language(text_buffer, language);

    mark_unsaved_file(ctx->path, FALSE);
    update_status_with_relative_path();
    select_file_in_sidebar(ctx->path);

    g_free(contents);
    g_free(ctx->path);
    g_free(ctx);
}

void load_file_async(const char *filepath) {
    GFile *gf = g_file_new_for_path(filepath);
    LoadCtx *ctx = g_new0(LoadCtx, 1);
    ctx->path = g_strdup(filepath);
    g_file_load_contents_async(gf, NULL, on_file_loaded, ctx);
    g_object_unref(gf);
}


// Save the current file (async)
static void on_file_saved(GObject *src, GAsyncResult *res, gpointer user_data) {
    GError *err = NULL;
    gboolean ok = g_file_replace_contents_finish(G_FILE(src), res, NULL, &err);
    if (!ok) {
        if (err) {
            set_status_message(err->message);
            g_error_free(err);
        }
        return;
    }
    // success
    mark_unsaved_file(current_file, FALSE);
    update_status_with_unsaved_mark(TRUE);
}

void save_file() {
    if (strlen(current_file) == 0) return;

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(text_buffer), &start, &end);
    char *text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(text_buffer), &start, &end, FALSE);

    if (last_saved_content) g_free(last_saved_content);
    last_saved_content = g_strdup(text);

    GFile *gf = g_file_new_for_path(current_file);
    g_file_replace_contents_async(gf,
        text, strlen(text),
        NULL, FALSE, G_FILE_CREATE_NONE,
        NULL, on_file_saved, NULL);
    g_object_unref(gf);

    g_free(text);
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


void update_status_with_unsaved_mark(gboolean is_same) {
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

// Incremental (idle) population
static void free_dir_task(DirTask *t) {
    if (!t) return;
    g_free(t->path);
    g_free(t);
}

static gboolean populate_step(gpointer data) {
    PopulateContext *ctx = (PopulateContext *)data;
    if (!ctx || ctx->cancelled) return G_SOURCE_REMOVE;

    // Process limited number of directories per idle iteration
    int budget = 8; // tune to keep UI responsive
    while (budget-- > 0 && !ctx->cancelled) {
        DirTask *task = g_queue_pop_head(ctx->queue);
        if (!task) {
            ctx->source_id = 0;
            return G_SOURCE_REMOVE; // done
        }

        DIR *dir = opendir(task->path);
        if (!dir) { free_dir_task(task); continue; }

        struct dirent *entry;
        GList *folders = NULL;
        GList *files = NULL;

        while ((entry = readdir(dir)) != NULL) {
            if (g_str_equal(entry->d_name, ".") || g_str_equal(entry->d_name, "..")) continue;
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", task->path, entry->d_name);

            struct stat st;
            if (stat(full, &st) == 0) {
                if (S_ISDIR(st.st_mode)) folders = g_list_insert_sorted(folders, g_strdup(entry->d_name), (GCompareFunc)g_ascii_strcasecmp);
                else files = g_list_insert_sorted(files, g_strdup(entry->d_name), (GCompareFunc)g_ascii_strcasecmp);
            }
        }
        closedir(dir);

        // Add folder nodes
        for (GList *l = folders; l; l = l->next) {
            const char *name = l->data;
            char display[1024];
            snprintf(display, sizeof(display), "%s/", name);
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", task->path, name);

            GtkTreeIter iter;
            gtk_tree_store_append(tree_store, &iter, task->has_parent ? &task->parent_iter : NULL);
            // GTK copies string values internally; no need to g_strdup
            gtk_tree_store_set(tree_store, &iter, 0, display, 1, full, -1);

            // enqueue subdir
            DirTask *sub = g_new0(DirTask, 1);
            sub->path = g_strdup(full);
            sub->parent_iter = iter;
            sub->has_parent = TRUE;
            g_queue_push_tail(ctx->queue, sub);
        }

        // Add files
        for (GList *l = files; l; l = l->next) {
            const char *name = l->data;
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", task->path, name);

            GtkTreeIter iter;
            gtk_tree_store_append(tree_store, &iter, task->has_parent ? &task->parent_iter : NULL);
            gtk_tree_store_set(tree_store, &iter, 0, name, 1, full, -1);
            file_list = g_list_append(file_list, g_strdup(full));
        }

        g_list_free_full(folders, g_free);
        g_list_free_full(files, g_free);
        free_dir_task(task);
    }

    return G_SOURCE_CONTINUE;
}

static void stop_population_if_running() {
    if (!populate_ctx) return;
    populate_ctx->cancelled = TRUE;
    if (populate_ctx->source_id) {
        g_source_remove(populate_ctx->source_id);
        populate_ctx->source_id = 0;
    }
    // drain remaining tasks
    while (!g_queue_is_empty(populate_ctx->queue)) free_dir_task(g_queue_pop_head(populate_ctx->queue));
    g_queue_free(populate_ctx->queue);
    g_free(populate_ctx);
    populate_ctx = NULL;
}

void populate_tree_async(const char *folder_path) {
    stop_population_if_running();

    gtk_tree_store_clear(tree_store);
    g_list_free_full(file_list, g_free);
    file_list = NULL;

    populate_ctx = g_new0(PopulateContext, 1);
    populate_ctx->queue = g_queue_new();
    populate_ctx->cancelled = FALSE;

    DirTask *root = g_new0(DirTask, 1);
    root->path = g_strdup(folder_path);
    root->has_parent = FALSE;
    g_queue_push_tail(populate_ctx->queue, root);

    populate_ctx->source_id = g_idle_add(populate_step, populate_ctx);
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
                load_file_async(filepath);
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

        g_strlcpy(current_folder, folder_path, sizeof(current_folder)); // Save the currently opened folder

        populate_tree_async(folder_path);

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
        load_file_async(filepath);
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
        // No folders open
        return;
    }

    // Restart async population
    populate_tree_async(current_folder);
}

void close_folder() {
    if (strlen(current_folder) == 0) {
        set_status_message("No folder is currently opened");
        return;
    }

    // Clear the current folder content in the sidebar
    stop_population_if_running();
    gtk_tree_store_clear(tree_store);
    g_list_free_full(file_list, g_free); // Clear previous file list
    file_list = NULL; // Reset file list
    memset(current_folder, 0, sizeof(current_folder)); // Clear the current folder path

    // Reset current file state
    memset(current_file, 0, sizeof(current_file));
    if (last_saved_content) {
        g_free(last_saved_content);
        last_saved_content = NULL;
    }
    if (current_file_row_ref) {
        gtk_tree_row_reference_free(current_file_row_ref);
        current_file_row_ref = NULL;
    }

    // Clear the editor content as well when folder is closed
    if (text_buffer != NULL) {
        // Do not push this clear operation into undo stack
        gtk_source_buffer_begin_not_undoable_action(text_buffer);
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(text_buffer), "", -1); // Clear text buffer
        gtk_source_buffer_end_not_undoable_action(text_buffer);
    }

    set_status_message("Folder closed and editor cleared");
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



void move_cursor_up(GtkTextBuffer *buffer) {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer)); // Get the current cursor position
    if (gtk_text_iter_backward_line(&iter)) { // Move up one line
        gtk_text_buffer_place_cursor(buffer, &iter);
    }
}

void move_cursor_down(GtkTextBuffer *buffer) {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer)); // Get the current cursor position
    if (gtk_text_iter_forward_line(&iter)) { // Move down one line
        gtk_text_buffer_place_cursor(buffer, &iter);
    }
}

void move_cursor_left(GtkTextBuffer *buffer) {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer)); // Get the current cursor position
    if (gtk_text_iter_backward_char(&iter)) { // Move left one character
        gtk_text_buffer_place_cursor(buffer, &iter);
    }
}

void move_cursor_right(GtkTextBuffer *buffer) {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, gtk_text_buffer_get_insert(buffer)); // Get the current cursor position
    if (gtk_text_iter_forward_char(&iter)) { // Move right one character
        gtk_text_buffer_place_cursor(buffer, &iter);
    }
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
            case GDK_KEY_q: close_folder(); return TRUE;

            // Custom shortcuts for moving the cursor
            case GDK_KEY_i: move_cursor_up(GTK_TEXT_BUFFER(text_buffer)); return TRUE; // Ctrl + I for up
            case GDK_KEY_k: move_cursor_down(GTK_TEXT_BUFFER(text_buffer)); return TRUE; // Ctrl + K for down
            case GDK_KEY_j: move_cursor_left(GTK_TEXT_BUFFER(text_buffer)); return TRUE; // Ctrl + J for left
            case GDK_KEY_l: move_cursor_right(GTK_TEXT_BUFFER(text_buffer)); return TRUE; // Ctrl + L for right
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



void mark_unsaved_file(const char *filepath, gboolean unsaved) {
    if (current_file_row_ref && gtk_tree_row_reference_valid(current_file_row_ref)) {
        GtkTreePath *path = gtk_tree_row_reference_get_path(current_file_row_ref);
        GtkTreeIter iter;
        GtkTreeModel *model = GTK_TREE_MODEL(tree_store);
        
        if (gtk_tree_model_get_iter(model, &iter, path)) {
            char *filename = g_path_get_basename(filepath);
            char display_name[1024];

            // Add '*' mark if the file is not saved.
            snprintf(display_name, sizeof(display_name), "%s%s", filename, unsaved ? " *" : "");
            gtk_tree_store_set(tree_store, &iter, 0, display_name, -1);

            g_free(filename);
        }
        gtk_tree_path_free(path);
    }
}



void on_text_changed(GtkTextBuffer *buffer, gpointer user_data) {
    if (strlen(current_file) > 0 && last_saved_content) {
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        
        // Fast path: compare lengths first
        gint current_len = gtk_text_iter_get_offset(&end);
        gint saved_len = strlen(last_saved_content);
        
        gboolean is_same = FALSE;
        if (current_len == saved_len) {
            char *current_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
            is_same = (strcmp(current_text, last_saved_content) == 0);
            g_free(current_text);
        }

        mark_unsaved_file(current_file, !is_same); 
        update_status_with_unsaved_mark(is_same); 
    }
}

void switch_theme() {
    if (!theme_manager) {
        theme_manager = gtk_source_style_scheme_manager_get_default();
    }

    static const char *themes[] = {
        "caecode-dark", "caecode-light"
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

    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);
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

    // Apply font via CSS (avoid deprecated override)
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "textview, textview text { font-family: 'Monospace'; font-weight: bold; font-size: 11pt; }",
        -1, NULL);
    GtkStyleContext *ctx = gtk_widget_get_style_context(text_view);
    gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

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
    
    // Add local themes path (development)
    char *cwd = g_get_current_dir();
    char *local_theme_path = g_build_filename(cwd, "assets", "themes", NULL);
    gtk_source_style_scheme_manager_append_search_path(theme_manager, local_theme_path);
    g_free(local_theme_path);
    g_free(cwd);

    // Add system-wide themes path (installed)
    gtk_source_style_scheme_manager_append_search_path(theme_manager, "/usr/share/caecode/themes");

    GtkSourceStyleScheme *default_scheme = gtk_source_style_scheme_manager_get_scheme(theme_manager, "caecode-dark");
    if (!default_scheme) {
        default_scheme = gtk_source_style_scheme_manager_get_scheme(theme_manager, "Yaru-dark");
    }
    
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
        g_free(last_saved_content);
    }
    if (current_file_row_ref) {
        gtk_tree_row_reference_free(current_file_row_ref);
    }

    return 0;
}

static void on_window_destroy(GtkWidget *w, gpointer user_data) {
    // Ensure background tasks are stopped before quitting
    stop_population_if_running();
    gtk_main_quit();
}