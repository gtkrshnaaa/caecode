#include "ui.h"
#include "editor.h"
#include "sidebar.h"
#include "search.h"
#include "file_ops.h"
#include <string.h>

// Instantiate globals defined as extern in app_state.h
GtkWidget *window;
GtkWidget *sidebar;
GtkWidget *status_bar;
GtkSourceView *source_view;
GtkSourceBuffer *text_buffer;
GtkTreeStore *tree_store;
GtkWidget *tree_view;
GtkSourceStyleSchemeManager *theme_manager;
GtkTreeRowReference *current_file_row_ref = NULL;
GList *file_list = NULL;

char current_file[1024] = "";
char current_folder[1024] = "";
char *last_saved_content = NULL;

static GtkWidget *sidebar_scrolled_window;
static gboolean sidebar_visible = TRUE;

void set_status_message(const char *message) {
    GtkStatusbar *sb = GTK_STATUSBAR(status_bar);
    gtk_statusbar_pop(sb, 0);
    gtk_statusbar_push(sb, 0, message);
}

void update_status_with_relative_path() {
    if (strlen(current_file) == 0) {
        set_status_message("No file opened");
        return;
    }

    if (strlen(current_folder) == 0) {
        set_status_message("No folder opened");
        return;
    }

    char relative_path[1024];
    if (g_str_has_prefix(current_file, current_folder)) {
        snprintf(relative_path, sizeof(relative_path), "%s", current_file + strlen(current_folder) + 1);
    } else {
        snprintf(relative_path, sizeof(relative_path), "%s", current_file);
    }
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

    char relative_path[1024];
    if (g_str_has_prefix(current_file, current_folder)) {
        snprintf(relative_path, sizeof(relative_path), "%s", current_file + strlen(current_folder) + 1);
    } else {
        snprintf(relative_path, sizeof(relative_path), "%s", current_file);
    }

    char display_path[1050];
    snprintf(display_path, sizeof(display_path), "%s%s", relative_path, is_same ? "" : " *");
    set_status_message(display_path);
}

static void move_cursor_up() {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(GTK_TEXT_BUFFER(text_buffer), &iter, gtk_text_buffer_get_insert(GTK_TEXT_BUFFER(text_buffer)));
    if (gtk_text_iter_backward_line(&iter)) {
        gtk_text_buffer_place_cursor(GTK_TEXT_BUFFER(text_buffer), &iter);
    }
}

static void move_cursor_down() {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(GTK_TEXT_BUFFER(text_buffer), &iter, gtk_text_buffer_get_insert(GTK_TEXT_BUFFER(text_buffer)));
    if (gtk_text_iter_forward_line(&iter)) {
        gtk_text_buffer_place_cursor(GTK_TEXT_BUFFER(text_buffer), &iter);
    }
}

static void move_cursor_left() {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(GTK_TEXT_BUFFER(text_buffer), &iter, gtk_text_buffer_get_insert(GTK_TEXT_BUFFER(text_buffer)));
    if (gtk_text_iter_backward_char(&iter)) {
        gtk_text_buffer_place_cursor(GTK_TEXT_BUFFER(text_buffer), &iter);
    }
}

static void move_cursor_right() {
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(GTK_TEXT_BUFFER(text_buffer), &iter, gtk_text_buffer_get_insert(GTK_TEXT_BUFFER(text_buffer)));
    if (gtk_text_iter_forward_char(&iter)) {
        gtk_text_buffer_place_cursor(GTK_TEXT_BUFFER(text_buffer), &iter);
    }
}

static void toggle_sidebar() {
    sidebar_visible = !sidebar_visible;
    gtk_widget_set_visible(sidebar_scrolled_window, sidebar_visible);
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    if ((event->state & GDK_CONTROL_MASK) != 0) {
        switch (event->keyval) {
            case GDK_KEY_o: {
                GtkWidget *dialog = gtk_file_chooser_dialog_new("Open Folder",
                    GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                    "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
                if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
                    char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
                    open_folder(path);
                    g_free(path);
                }
                gtk_widget_destroy(dialog);
                return TRUE;
            }
            case GDK_KEY_b: toggle_sidebar(); return TRUE;
            case GDK_KEY_s: save_file(); return TRUE;
            case GDK_KEY_m: switch_theme(); return TRUE;
            case GDK_KEY_p: show_search_popup(); return TRUE;
            case GDK_KEY_r: reload_sidebar(); return TRUE;
            case GDK_KEY_q: close_folder(); return TRUE;
            
            case GDK_KEY_i: move_cursor_up(); return TRUE;
            case GDK_KEY_k: move_cursor_down(); return TRUE;
            case GDK_KEY_j: move_cursor_left(); return TRUE;
            case GDK_KEY_l: move_cursor_right(); return TRUE;
            
            case GDK_KEY_c: gtk_text_buffer_copy_clipboard(GTK_TEXT_BUFFER(text_buffer), gtk_clipboard_get(GDK_SELECTION_CLIPBOARD)); return TRUE;
            case GDK_KEY_v: gtk_text_buffer_paste_clipboard(GTK_TEXT_BUFFER(text_buffer), gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), NULL, TRUE); return TRUE;
            case GDK_KEY_x: gtk_text_buffer_cut_clipboard(GTK_TEXT_BUFFER(text_buffer), gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), TRUE); return TRUE;
            case GDK_KEY_z: if (gtk_source_buffer_can_undo(text_buffer)) gtk_source_buffer_undo(text_buffer); return TRUE;
            case GDK_KEY_y: if (gtk_source_buffer_can_redo(text_buffer)) gtk_source_buffer_redo(text_buffer); return TRUE;
        }
    }
    return FALSE;
}

void create_main_window() {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Caecode");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 600);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);

    // Sidebar
    init_sidebar();
    sidebar_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar_scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    // Use a VBox to wrap tree_view and a spacer for a proper scrolling gap
    GtkWidget *sidebar_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(sidebar_box), tree_view, TRUE, TRUE, 0);
    
    // Add a spacer at the bottom (100px)
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(spacer, -1, 100);
    gtk_box_pack_start(GTK_BOX(sidebar_box), spacer, FALSE, FALSE, 0);

    // GtkScrolledWindow needs a viewport for GtkBox
    gtk_container_add(GTK_CONTAINER(sidebar_scrolled_window), sidebar_box);
    gtk_paned_pack1(GTK_PANED(paned), sidebar_scrolled_window, FALSE, FALSE);

    // Editor
    init_editor();
    
    // Apply margins for better spacing
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(source_view), 20);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(source_view), 20);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(source_view), 15);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(source_view), 15);

    // Apply cleaner font via CSS
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "textview, textview text { font-family: 'Monospace'; font-size: 11pt; }",
        -1, NULL);
    GtkStyleContext *ctx = gtk_widget_get_style_context(GTK_WIDGET(source_view));
    gtk_style_context_add_provider(ctx, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    GtkWidget *editor_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(editor_scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(editor_scrolled_window), GTK_WIDGET(source_view));
    gtk_paned_pack2(GTK_PANED(paned), editor_scrolled_window, TRUE, FALSE);

    // Status bar
    status_bar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), status_bar, FALSE, FALSE, 0);
    set_status_message("Welcome to Caecode");

    init_search_popup();

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);

    gtk_widget_show_all(window);
}
