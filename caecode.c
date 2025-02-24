#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global variables
GtkWidget *window, *text_view, *scrolled_window, *sidebar;
GtkSourceBuffer *source_buffer;
gchar *last_saved_content = NULL;
gboolean sidebar_visible = TRUE;

// Function to save file content
void save_file(const gchar *filename) {
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(source_buffer), &start);
    gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(source_buffer), &end);
    gchar *text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(source_buffer), &start, &end, FALSE);

    FILE *file = fopen(filename, "w");
    if (file) {
        fputs(text, file);
        fclose(file);
        g_free(last_saved_content);
        last_saved_content = g_strdup(text);
    }
    g_free(text);
}

// Shortcut callback for saving file
gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_KEY_s) {
        save_file("output.txt");
        return TRUE;
    }
    if ((event->state & GDK_CONTROL_MASK) && event->keyval == GDK_KEY_b) {
        sidebar_visible = !sidebar_visible;
        gtk_widget_set_visible(sidebar, sidebar_visible);
        return TRUE;
    }
    return FALSE;
}

// Function to initialize text view
void init_text_view() {
    source_buffer = gtk_source_buffer_new(NULL);
    text_view = gtk_source_view_new_with_buffer(source_buffer);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(text_view), TRUE);
    gtk_source_view_set_tab_width(GTK_SOURCE_VIEW(text_view), 4);
    gtk_widget_set_hexpand(text_view, TRUE);
    gtk_widget_set_vexpand(text_view, TRUE);

    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
    GtkSourceLanguage *language = gtk_source_language_manager_get_language(lm, "c");
    gtk_source_buffer_set_language(source_buffer, language);

    GtkSourceStyleSchemeManager *sm = gtk_source_style_scheme_manager_get_default();
    GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme(sm, "classic");
    gtk_source_buffer_set_style_scheme(source_buffer, scheme);
}

// Function to initialize sidebar
void init_sidebar() {
    sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(sidebar, 200, -1);
}

// Activate function to build UI
void activate(GtkApplication *app, gpointer user_data) {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Caecode");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    init_sidebar();
    gtk_box_pack_start(GTK_BOX(main_box), sidebar, FALSE, FALSE, 0);

    init_text_view();
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    gtk_box_pack_start(GTK_BOX(main_box), scrolled_window, TRUE, TRUE, 0);

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
