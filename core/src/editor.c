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
    gtk_source_view_set_smart_backspace(source_view, TRUE);
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
        
        // Unify UI colors based on the selected theme (GNOME Adwaita palette)
        const char *bg_color = (current_theme == 0) ? "#1e1e1e" : "#FFFFFF";
        const char *fg_color = (current_theme == 0) ? "#E0E0E0" : "#333333";
        const char *line_num_bg = (current_theme == 0) ? "#1e1e1e" : "#FFFFFF";
        
        char *css_data = g_strdup_printf(
            "#sidebar-scrolledwindow, #sidebar-scrolledwindow viewport, treeview, statusbar, #welcome-screen, #bottom-panel, #chat-panel { background-color: %s; color: %s; }"
            "treeview { padding-bottom: 100px; }"
            "treeview:selected { background-color: %s; }"
            "statusbar { border-top: 1px solid %s; }"
            ".dim-label { opacity: 0.6; }",
            bg_color, fg_color,
            (current_theme == 0) ? "#333333" : "#EEEEEE", // selection color
            (current_theme == 0) ? "#222222" : "#DDDDDD"  // statusbar border
        );
        
        gtk_css_provider_load_from_data(app_css_provider, css_data, -1, NULL);
        g_free(css_data);

        // Update Terminal colors
        GdkRGBA bg_rgba, fg_rgba;
        gdk_rgba_parse(&bg_rgba, bg_color);
        gdk_rgba_parse(&fg_rgba, fg_color);

        int n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(terminal_notebook));
        for (int i = 0; i < n_pages; i++) {
            GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(terminal_notebook), i);
            if (GTK_IS_SCROLLED_WINDOW(page)) {
                GtkWidget *terminal = gtk_bin_get_child(GTK_BIN(page));
                if (VTE_IS_TERMINAL(terminal)) {
                    vte_terminal_set_colors(VTE_TERMINAL(terminal), &fg_rgba, &bg_rgba, NULL, 0);
                }
            }
        }

        // Additional terminal-specific CSS
        char *term_css = g_strdup_printf(
            ".terminal-toolbar { background-color: %s; border-bottom: 1px solid %s; }"
            "notebook stack { background-color: %s; }",
            (current_theme == 0) ? "#252525" : "#F3F3F3",
            (current_theme == 0) ? "#333333" : "#DDDDDD",
            bg_color
        );
        GtkStyleContext *nb_ctx = gtk_widget_get_style_context(terminal_notebook);
        GtkCssProvider *nb_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(nb_provider, term_css, -1, NULL);
        gtk_style_context_add_provider(nb_ctx, GTK_STYLE_PROVIDER(nb_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(nb_provider);
        g_free(term_css);

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
