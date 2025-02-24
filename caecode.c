#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

// Global variable for last saved content
char *last_saved_content = NULL;

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

    // Create source view
    source_view = gtk_source_view_new_with_buffer(source_buffer);
    gtk_widget_set_hexpand(source_view, TRUE);
    gtk_widget_set_vexpand(source_view, TRUE);

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

    if (last_saved_content) {
        free(last_saved_content);
    }

    return 0;
}
