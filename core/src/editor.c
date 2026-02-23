#include "editor.h"
#include <string.h>
#include "sidebar.h"
#include "ui.h"
#include "file_ops.h"
#include <gio/gunixinputstream.h>

static guint autosave_timeout_id = 0;

static gboolean on_autosave_timer(gpointer data) {
    autosave_timeout_id = 0;
    if (gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(text_buffer))) {
        save_file();
    }
    return FALSE;
}

static GdkPixbuf *create_color_bar_pixbuf(const char *color_str) {
    GdkRGBA rgba;
    gdk_rgba_parse(&rgba, color_str);
    GdkPixbuf *pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 3, 24);
    // Fill with RBGA: R << 24 | G << 16 | B << 8 | A
    guint32 pixel = ((guint8)(rgba.red * 255) << 24) |
                    ((guint8)(rgba.green * 255) << 16) |
                    ((guint8)(rgba.blue * 255) << 8) |
                    ((guint8)(rgba.alpha * 255));
    gdk_pixbuf_fill(pixbuf, pixel);
    return pixbuf;
}

static void on_git_gutter_child_watch(GPid pid, gint status, gpointer user_data) {
    g_spawn_close_pid(pid);
}

static void on_git_gutter_spliced(GObject *source, GAsyncResult *res, gpointer user_data) {
    GOutputStream *out = G_OUTPUT_STREAM(source);
    GError *splice_err = NULL;
    if (g_output_stream_splice_finish(out, res, &splice_err) >= 0) {
        gsize size = g_memory_output_stream_get_data_size(G_MEMORY_OUTPUT_STREAM(out));
        if (size > 0) {
            char *output = g_strndup(g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM(out)), size);
            char **lines = g_strsplit(output, "\n", -1);
            
            gboolean is_diff = (g_strstr_len(output, size, "diff --git") != NULL);
            if (!is_diff && size < 50) { 
            }
            
            for (int i = 0; lines[i]; i++) {
                if (g_str_has_prefix(lines[i], "@@")) {
                    int old_line, old_count, new_line, new_count;
                    old_count = 1; new_count = 1; 
    
                    if (sscanf(lines[i], "@@ -%d,%d +%d,%d @@", &old_line, &old_count, &new_line, &new_count) >= 2 ||
                        sscanf(lines[i], "@@ -%d +%d,%d @@", &old_line, &new_line, &new_count) >= 2 ||
                        sscanf(lines[i], "@@ -%d,%d +%d @@", &old_line, &old_count, &new_line) >= 2 ||
                        sscanf(lines[i], "@@ -%d +%d @@", &old_line, &new_line) >= 2) {
                        
                        const char *category = (old_count == 0) ? "git-added" : "git-modified";
                        if (new_count == 0 && old_count > 0) category = "git-deleted";
    
                        int start_line = (new_line > 0) ? new_line - 1 : 0;
                        int count = (new_count > 0) ? new_count : 1;
    
                        for (int j = 0; j < count; j++) {
                            GtkTextIter iter;
                            gtk_text_buffer_get_iter_at_line(GTK_TEXT_BUFFER(text_buffer), &iter, start_line + j);
                            gtk_source_buffer_create_source_mark(text_buffer, NULL, category, &iter);
                        }
                    }
                }
            }
            g_strfreev(lines);
            g_free(output);
        }
    }
    if (splice_err) g_error_free(splice_err);
    g_object_unref(out);
}

void update_git_gutter() {
    if (strlen(current_file) == 0 || strlen(current_folder) == 0) return;

    // Clear existing git marks
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(text_buffer), &start, &end);
    gtk_source_buffer_remove_source_marks(text_buffer, &start, &end, "git-added");
    gtk_source_buffer_remove_source_marks(text_buffer, &start, &end, "git-modified");
    gtk_source_buffer_remove_source_marks(text_buffer, &start, &end, "git-deleted");

    char *diff_cmd[] = { "git", "-C", current_folder, "diff", "-U0", "HEAD", "--", current_file, NULL };
    gint standard_output;
    GPid pid;
    GError *err = NULL;

    if (g_spawn_async_with_pipes(current_folder, diff_cmd, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL, NULL, &pid, NULL, &standard_output, NULL, &err)) {
        GInputStream *stream = g_unix_input_stream_new(standard_output, TRUE);
        GOutputStream *mem_stream = g_memory_output_stream_new(NULL, 0, g_realloc, g_free);
        
        g_output_stream_splice_async(mem_stream, stream, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET, 
            G_PRIORITY_DEFAULT, NULL, on_git_gutter_spliced, NULL);
            
        g_object_unref(stream);
        g_child_watch_add(pid, on_git_gutter_child_watch, NULL);
    } else {
        if (err) g_error_free(err);
    }
}

static const char *themes[] = { "caecode-dark", "caecode-light" };
static guint gutter_timeout_id = 0;

static gboolean debounced_gutter_update(gpointer data) {
    gutter_timeout_id = 0;
    update_git_gutter();
    return FALSE;
}

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
        
        // Define 16-color palette based on syntax colors
        const char *palette_str[16];
        if (current_theme_idx == 0) { // Dark
            const char *dark_p[16] = {
                "#1e1e1e", "#808080", "#D3D3D3", "#E0E0E0", "#FFFFFF", "#808080", "#D3D3D3", "#E0E0E0",
                "#404040", "#808080", "#D3D3D3", "#FFFFFF", "#FFFFFF", "#808080", "#D3D3D3", "#FFFFFF"
            };
            memcpy(palette_str, dark_p, sizeof(palette_str));
        } else { // Light
            const char *light_p[16] = {
                "#FFFFFF", "#606060", "#909090", "#333333", "#000000", "#606060", "#909090", "#333333",
                "#A0A0A0", "#606060", "#909090", "#000000", "#000000", "#606060", "#909090", "#000000"
            };
            memcpy(palette_str, light_p, sizeof(palette_str));
        }

        GdkRGBA bg_rgba, fg_rgba, palette_rgba[16];
        gdk_rgba_parse(&bg_rgba, bg_color);
        gdk_rgba_parse(&fg_rgba, fg_color);
        for (int i = 0; i < 16; i++) gdk_rgba_parse(&palette_rgba[i], palette_str[i]);

        char *css_data = g_strdup_printf(
            "#sidebar-scrolledwindow, #sidebar-scrolledwindow viewport, treeview, statusbar, #welcome-screen, #bottom-panel, #chat-panel { background-color: %s; color: %s; }"
            "#sidebar-header { background-color: %s; border-bottom: 1px solid %s; }"
            "#sidebar-title { font-size: 9pt; font-weight: bold; color: %s; opacity: 0.6; }"
            "#sidebar-header button { opacity: 0.6; }"
            "#sidebar-header button:hover { opacity: 1.0; }"
            "#path-bar { background-color: %s; border-bottom: 1px solid %s; }"
            "#path-label { font-size: 8.5pt; color: %s; opacity: 0.7; }"
            "treeview { padding-bottom: 100px; }"
            "treeview:selected, list row:selected, row:selected { background-color: rgba(53, 132, 228, 0.2); color: inherit; }"
            "selection { background-color: rgba(53, 132, 228, 0.2); }"
            "statusbar { border-top: 1px solid %s; }"
            ".dim-label { opacity: 0.6; }"
            "#source-view text { font-weight: 600; }",
            bg_color, fg_color,
            (current_theme_idx == 0) ? "#252525" : "#F3F3F3", // sidebar header bg
            (current_theme_idx == 0) ? "#333333" : "#DDDDDD", // sidebar header border
            fg_color, // sidebar title color
            (current_theme_idx == 0) ? "#252525" : "#F8F8F8", // path bar bg (subtle)
            (current_theme_idx == 0) ? "#2a2a2a" : "#EEEEEE", // path bar border
            fg_color, // path label color
            (current_theme_idx == 0) ? "#222222" : "#DDDDDD"  // statusbar border
        );
        
        gtk_css_provider_load_from_data(app_css_provider, css_data, -1, NULL);
        g_free(css_data);

        GList *children = gtk_container_get_children(GTK_CONTAINER(terminal_stack));
        for (GList *l = children; l != NULL; l = l->next) {
            GtkWidget *page = (GtkWidget *)l->data;
            if (GTK_IS_SCROLLED_WINDOW(page)) {
                GtkWidget *terminal = gtk_bin_get_child(GTK_BIN(page));
                if (VTE_IS_TERMINAL(terminal)) {
                    vte_terminal_set_colors(VTE_TERMINAL(terminal), &fg_rgba, &bg_rgba, palette_rgba, 16);
                }
            }
        }
        g_list_free(children);

        if (chat_panel) {
            GList *chat_children = gtk_container_get_children(GTK_CONTAINER(chat_panel));
            for (GList *l = chat_children; l != NULL; l = l->next) {
                GtkWidget *child = (GtkWidget *)l->data;
                if (GTK_IS_SCROLLED_WINDOW(child)) {
                    GtkWidget *terminal = gtk_bin_get_child(GTK_BIN(child));
                    if (VTE_IS_TERMINAL(terminal)) {
                        vte_terminal_set_colors(VTE_TERMINAL(terminal), &fg_rgba, &bg_rgba, palette_rgba, 16);
                    }
                }
            }
            g_list_free(chat_children);
        }

        // Additional terminal-specific CSS
        char *term_css = g_strdup_printf(
            ".terminal-toolbar { background-color: %s; border-bottom: 1px solid %s; }"
            ".terminal-header-item { padding: 5px 10px; color: %s; opacity: 0.7; }"
            ".terminal-header-item.active { opacity: 1.0; border-bottom: 2px solid rgba(53, 132, 228, 0.4); }"
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

        char *msg = g_strdup_printf("Theme: %s", themes[current_theme_idx]);
        set_status_message(msg);
        g_free(msg);
    }
}

void switch_theme() {
    apply_theme((current_theme_idx + 1) % 2);
}

void init_editor() {
    text_buffer = gtk_source_buffer_new(NULL);
    source_view = GTK_SOURCE_VIEW(gtk_source_view_new_with_buffer(text_buffer));
    gtk_widget_set_name(GTK_WIDGET(source_view), "source-view");
    
    // Configure editor
    gtk_source_view_set_show_line_numbers(source_view, TRUE);
    gtk_source_view_set_auto_indent(source_view, TRUE);
    gtk_source_view_set_tab_width(source_view, 4);
    gtk_source_view_set_insert_spaces_instead_of_tabs(source_view, TRUE);
    gtk_source_view_set_smart_backspace(source_view, TRUE);
    gtk_source_view_set_highlight_current_line(source_view, TRUE);

    // Ensure infinite undo and clean initial state
    gtk_source_buffer_set_max_undo_levels(text_buffer, -1);

    // Initial theme setup
    theme_manager = gtk_source_style_scheme_manager_get_default();
    
    // Configure gutter for git marks
    gtk_source_view_set_show_line_marks(source_view, TRUE);
    
    GtkSourceMarkAttributes *added_attr = gtk_source_mark_attributes_new();
    GdkPixbuf *added_pb = create_color_bar_pixbuf("#73C991");
    gtk_source_mark_attributes_set_pixbuf(added_attr, added_pb);
    g_object_unref(added_pb);
    gtk_source_view_set_mark_attributes(source_view, "git-added", added_attr, 0);

    GtkSourceMarkAttributes *modified_attr = gtk_source_mark_attributes_new();
    GdkPixbuf *mod_pb = create_color_bar_pixbuf("#3584e4");
    gtk_source_mark_attributes_set_pixbuf(modified_attr, mod_pb);
    g_object_unref(mod_pb);
    gtk_source_view_set_mark_attributes(source_view, "git-modified", modified_attr, 0);

    GtkSourceMarkAttributes *deleted_attr = gtk_source_mark_attributes_new();
    GdkPixbuf *del_pb = create_color_bar_pixbuf("#F85149");
    gtk_source_mark_attributes_set_pixbuf(deleted_attr, del_pb);
    g_object_unref(del_pb);
    gtk_source_view_set_mark_attributes(source_view, "git-deleted", deleted_attr, 0);
    
    // Add local themes path (development)
    char *cwd = g_get_current_dir();
    char *local_theme_path = g_build_filename(cwd, "core", "themes", NULL);
    gtk_source_style_scheme_manager_append_search_path(theme_manager, local_theme_path);
    g_free(local_theme_path);
    g_free(cwd);

    // Add system-wide themes path (installed)
    gtk_source_style_scheme_manager_append_search_path(theme_manager, "/usr/share/caecode/themes");

    // Language Mapping setup
    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
    const gchar * const *current_dirs = gtk_source_language_manager_get_search_path(lm);
    
    // Calculate new size: current + 2 custom + 1 NULL
    int count = 0;
    while (current_dirs && current_dirs[count]) count++;
    
    gchar **new_dirs = g_new0(gchar *, count + 3);
    for (int i = 0; i < count; i++) new_dirs[i] = g_strdup(current_dirs[i]);
    
    char *cwd_lang = g_get_current_dir();
    new_dirs[count] = g_build_filename(cwd_lang, "core", "languages", NULL);
    new_dirs[count + 1] = g_strdup("/usr/share/caecode/languages");
    new_dirs[count + 2] = NULL;
    
    gtk_source_language_manager_set_search_path(lm, new_dirs);
    
    g_strfreev(new_dirs);
    g_free(cwd_lang);

    // Initial theme detection and apply
    apply_theme(is_system_dark_mode() ? 0 : 1);

    // Listen for real-time system theme changes
    GtkSettings *settings = gtk_settings_get_default();
    g_signal_connect(settings, "notify::gtk-application-prefer-dark-theme", G_CALLBACK(on_system_theme_changed), NULL);
    g_signal_connect(settings, "notify::gtk-theme-name", G_CALLBACK(on_system_theme_changed), NULL);

    g_signal_connect(text_buffer, "changed", G_CALLBACK(on_text_changed), NULL);
}

void on_text_changed(GtkTextBuffer *buffer, gpointer user_data) {
    if (strlen(current_file) == 0) return;

    // Use built-in modified state for high-performance tracking
    gboolean modified = gtk_text_buffer_get_modified(buffer);
    
    // Deep comparison fallback only if needed or when saving/loading
    // For real-time typing, 'modified' is sufficient and efficient.
    if (modified && last_saved_content) {
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        
        gint current_len = gtk_text_iter_get_offset(&end);
        gint saved_len = strlen(last_saved_content);
        
        if (current_len == saved_len) {
            char *current_text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
            if (strcmp(current_text, last_saved_content) == 0) {
                modified = FALSE;
                gtk_text_buffer_set_modified(buffer, FALSE);
            }
            g_free(current_text);
        }
    }

    mark_unsaved_file(current_file, modified); 
    update_status_with_unsaved_mark(!modified); 

    // Real-time Git Status update (Sidebar coloring)
    // Removed because sidebar polling handles directory changes, and typing fires this too often.

    // Debounced gutter update
    if (gutter_timeout_id > 0) g_source_remove(gutter_timeout_id);
    gutter_timeout_id = g_timeout_add(300, debounced_gutter_update, NULL);

    // Debounced AutoSave (1 second)
    if (autosave_timeout_id > 0) g_source_remove(autosave_timeout_id);
    autosave_timeout_id = g_timeout_add(1000, on_autosave_timer, NULL);
}

void cleanup_editor() {
    if (autosave_timeout_id > 0) {
        g_source_remove(autosave_timeout_id);
        autosave_timeout_id = 0;
    }
    if (gutter_timeout_id > 0) {
        g_source_remove(gutter_timeout_id);
        gutter_timeout_id = 0;
    }
}
