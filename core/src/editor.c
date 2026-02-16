#include "editor.h"
#include <string.h>

static int current_theme_idx = 0;
static const char *themes[] = { "caecode-dark", "caecode-light" };

static gboolean is_system_dark_mode() {
    gboolean prefer_dark = FALSE;
    GtkSettings *settings = gtk_settings_get_default();
    g_object_get(settings, "gtk-application-prefer-dark-theme", &prefer_dark, NULL);
    
    if (!prefer_dark) {
        char *theme_name;
        g_object_get(settings, "gtk-theme-name", &theme_name, NULL);
        if (theme_name) {
            char *lower_theme = g_ascii_strdown(theme_name, -1);
            if (strstr(lower_theme, "dark")) {
                prefer_dark = TRUE;
            }
            g_free(lower_theme);
            g_free(theme_name);
        }
    }
    return prefer_dark;
}

static void on_system_theme_changed(GObject *object, GParamSpec *pspec, gpointer user_data) {
    int detected_idx = is_system_dark_mode() ? 0 : 1;
    if (detected_idx != current_theme_idx) {
        apply_theme(detected_idx);
    }
}

void apply_theme(int index) {
    current_theme_idx = index;
    GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme(theme_manager, themes[current_theme_idx]);
    
    if (scheme) {
        gtk_source_buffer_set_style_scheme(text_buffer, scheme);
        
        // Unify UI colors based on the selected theme (GNOME Adwaita palette)
        const char *bg_color = (current_theme_idx == 0) ? "#1e1e1e" : "#FFFFFF";
        const char *fg_color = (current_theme_idx == 0) ? "#E0E0E0" : "#333333";
        
        char *css_data = g_strdup_printf(
            "#sidebar-scrolledwindow, #sidebar-scrolledwindow viewport, treeview, statusbar, #welcome-screen, #bottom-panel, #chat-panel { background-color: %s; color: %s; }"
            "treeview { padding-bottom: 100px; }"
            "treeview:selected { background-color: %s; }"
            "statusbar { border-top: 1px solid %s; }"
            ".dim-label { opacity: 0.6; }",
            bg_color, fg_color,
            (current_theme_idx == 0) ? "#333333" : "#EEEEEE", // selection color
            (current_theme_idx == 0) ? "#222222" : "#DDDDDD"  // statusbar border
        );
        
        gtk_css_provider_load_from_data(app_css_provider, css_data, -1, NULL);
        g_free(css_data);

        // Update Terminal colors
        GdkRGBA bg_rgba, fg_rgba;
        gdk_rgba_parse(&bg_rgba, bg_color);
        gdk_rgba_parse(&fg_rgba, fg_color);

        GList *children = gtk_container_get_children(GTK_CONTAINER(terminal_stack));
        for (GList *l = children; l != NULL; l = l->next) {
            GtkWidget *page = (GtkWidget *)l->data;
            if (GTK_IS_SCROLLED_WINDOW(page)) {
                GtkWidget *terminal = gtk_bin_get_child(GTK_BIN(page));
                if (VTE_IS_TERMINAL(terminal)) {
                    vte_terminal_set_colors(VTE_TERMINAL(terminal), &fg_rgba, &bg_rgba, NULL, 0);
                }
            }
        }
        g_list_free(children);

        // Additional terminal-specific CSS
        char *term_css = g_strdup_printf(
            ".terminal-toolbar { background-color: %s; border-bottom: 1px solid %s; }"
            ".terminal-header-item { padding: 5px 10px; color: %s; opacity: 0.7; }"
            ".terminal-header-item.active { opacity: 1.0; border-bottom: 2px solid #3584e4; }"
            ".terminal-session-list { background-color: %s; border-left: 1px solid %s; }",
            (current_theme_idx == 0) ? "#252525" : "#F3F3F3",
            (current_theme_idx == 0) ? "#333333" : "#DDDDDD",
            fg_color,
            (current_theme_idx == 0) ? "#1e1e1e" : "#FFFFFF",
            (current_theme_idx == 0) ? "#333333" : "#DDDDDD"
        );
        GtkStyleContext *ts_ctx = gtk_widget_get_style_context(terminal_stack);
        GtkCssProvider *ts_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_data(ts_provider, term_css, -1, NULL);
        gtk_style_context_add_provider(ts_ctx, GTK_STYLE_PROVIDER(ts_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(ts_provider);
        g_free(term_css);

        set_status_message(g_strdup_printf("Theme: %s", themes[current_theme_idx]));
    }
}

void switch_theme() {
    apply_theme((current_theme_idx + 1) % 2);
}

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

    // Initial theme detection and apply
    apply_theme(is_system_dark_mode() ? 0 : 1);

    // Listen for real-time system theme changes
    GtkSettings *settings = gtk_settings_get_default();
    g_signal_connect(settings, "notify::gtk-application-prefer-dark-theme", G_CALLBACK(on_system_theme_changed), NULL);
    g_signal_connect(settings, "notify::gtk-theme-name", G_CALLBACK(on_system_theme_changed), NULL);

    g_signal_connect(text_buffer, "changed", G_CALLBACK(on_text_changed), NULL);
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
