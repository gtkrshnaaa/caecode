#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

// Callback function for handling keyboard shortcuts
gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget)));

    // Ctrl key is pressed
    if (event->state & GDK_CONTROL_MASK) {
        switch (gdk_keyval_to_upper(event->keyval)) {
            case 'C': // Copy
                gtk_text_buffer_copy_clipboard(GTK_TEXT_BUFFER(buffer), gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
                return TRUE;
            case 'V': // Paste
                gtk_text_buffer_paste_clipboard(GTK_TEXT_BUFFER(buffer), gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), NULL, TRUE);
                return TRUE;
            case 'X': // Cut
                gtk_text_buffer_cut_clipboard(GTK_TEXT_BUFFER(buffer), gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), TRUE);
                return TRUE;
            case 'Z': // Undo using GtkSourceBuffer built-in functionality
                if (gtk_source_buffer_can_undo(buffer)) {
                    gtk_source_buffer_undo(buffer);
                }
                return TRUE;
            case 'Y': // Redo using GtkSourceBuffer built-in functionality
                if (gtk_source_buffer_can_redo(buffer)) {
                    gtk_source_buffer_redo(buffer);
                }
                return TRUE;
        }
    }

    return FALSE; // Propagate the event further
}

// Initialize UI
void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *scrolled_window;
    GtkSourceBuffer *source_buffer;
    GtkWidget *source_view;
    GtkSourceLanguageManager *lang_manager;
    GtkSourceLanguage *language;

    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Caecode");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create scrolled window
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(window), scrolled_window);

    // Create source buffer with syntax highlighting
    lang_manager = gtk_source_language_manager_get_default();
    language = gtk_source_language_manager_get_language(lang_manager, "c");
    source_buffer = gtk_source_buffer_new_with_language(language);
    
    // Enable undo/redo functionality
    gtk_source_buffer_set_highlight_syntax(source_buffer, TRUE);

    // Create source view
    source_view = gtk_source_view_new_with_buffer(source_buffer);
    gtk_widget_set_hexpand(source_view, TRUE);
    gtk_widget_set_vexpand(source_view, TRUE);

    // Connect key press event
    g_signal_connect(source_view, "key-press-event", G_CALLBACK(on_key_press), NULL);

    // Enable line numbers
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(source_view), TRUE);

    // Set font using CSS
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "textview { font-family: 'Monospace'; font-size: 13pt; }",
        -1, NULL);
    GtkStyleContext *context = gtk_widget_get_style_context(source_view);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);

    // Disable word wrap
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(source_view), GTK_WRAP_NONE);

    // Add source view to scrolled window
    gtk_container_add(GTK_CONTAINER(scrolled_window), source_view);

    // Show all widgets
    gtk_widget_show_all(window);
}

// Main function
int main(int argc, char **argv) {
    gtk_init(&argc, &argv);
    activate(NULL, NULL);
    gtk_main();

    return 0;
}
