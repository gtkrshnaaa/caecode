#include "editor.h"
#include <string.h>

void init_editor() {
    text_buffer = gtk_source_buffer_new(NULL);
    source_view = GTK_SOURCE_VIEW(gtk_source_view_new_with_buffer(text_buffer));
    
    // Configure editor
    gtk_source_view_set_show_line_numbers(source_view, TRUE);
    gtk_source_view_set_auto_indent(source_view, TRUE);
    gtk_source_view_set_tab_width(source_view, 4);
    gtk_source_view_set_insert_spaces_instead_of_tabs(source_view, TRUE);
    gtk_source_view_set_highlight_current_line(source_view, TRUE);

    // Initial theme setup
    theme_manager = gtk_source_style_scheme_manager_get_default();
    
    // Add local themes path (development)
    char *cwd = g_get_current_dir();
    char *local_theme_path = g_build_filename(cwd, "core", "themes", NULL);
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

    g_signal_connect(text_buffer, "changed", G_CALLBACK(on_text_changed), NULL);
}

void switch_theme() {
    static const char *themes[] = {
        "caecode-dark", "caecode-light"
    };
    static int current_theme = 0;

    current_theme = (current_theme + 1) % (sizeof(themes) / sizeof(themes[0]));
    GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme(theme_manager, themes[current_theme]);
    
    if (scheme) {
        gtk_source_buffer_set_style_scheme(text_buffer, scheme);
        set_status_message(g_strdup_printf("Theme switched to: %s", themes[current_theme]));
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
