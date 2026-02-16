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
GtkCssProvider *app_css_provider = NULL;
GtkWidget *editor_stack;
GtkWidget *welcome_screen;
GtkWidget *bottom_panel;
GtkWidget *chat_panel;
GtkWidget *terminal_stack;
GtkWidget *terminal_list;
GtkWidget *empty_state;

int current_theme_idx = 0;
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

void show_empty_state() {
    gtk_stack_set_visible_child_name(GTK_STACK(editor_stack), "empty");
}

void close_all_files() {
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(text_buffer), "", 0);
    current_file[0] = '\0';
    if (last_saved_content) {
        g_free(last_saved_content);
        last_saved_content = NULL;
    }
    gtk_text_buffer_set_modified(GTK_TEXT_BUFFER(text_buffer), FALSE);
    if (current_file_row_ref) {
        gtk_tree_row_reference_free(current_file_row_ref);
        current_file_row_ref = NULL;
    }
    gtk_window_set_title(GTK_WINDOW(window), "Caecode");
    show_empty_state();
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    static gboolean ctrl_k_pending = FALSE;
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(window));
    gboolean is_terminal = focus && VTE_IS_TERMINAL(focus);

    if ((event->state & GDK_CONTROL_MASK) != 0) {
        gboolean shift = (event->state & GDK_SHIFT_MASK) != 0;

        // Specialized Terminal Shortcuts
        if (is_terminal) {
            if (event->keyval == GDK_KEY_c && !shift) {
                // Pass Ctrl+C to terminal for signal interruption
                return FALSE;
            }
            if ((event->keyval == GDK_KEY_C || event->keyval == GDK_KEY_c) && shift) {
                vte_terminal_copy_clipboard(VTE_TERMINAL(focus));
                return TRUE;
            }
            if ((event->keyval == GDK_KEY_V || event->keyval == GDK_KEY_v) && shift) {
                vte_terminal_paste_clipboard(VTE_TERMINAL(focus));
                return TRUE;
            }
        }

        guint key = gdk_keyval_to_lower(event->keyval);
        
        // Handle Ctrl+K Chord Start
        if (key == GDK_KEY_k) {
            ctrl_k_pending = TRUE;
            set_status_message("Ctrl+K pressed. Waiting for next key...");
            return TRUE;
        }

        switch (key) {
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
                ctrl_k_pending = FALSE;
                return TRUE;
            }
            case GDK_KEY_b: toggle_sidebar(); ctrl_k_pending = FALSE; return TRUE;
            case GDK_KEY_s: save_file(); ctrl_k_pending = FALSE; return TRUE;
            case GDK_KEY_m: switch_theme(); ctrl_k_pending = FALSE; return TRUE;
            case GDK_KEY_p: show_search_popup(); ctrl_k_pending = FALSE; return TRUE;
            case GDK_KEY_r: reload_sidebar(); ctrl_k_pending = FALSE; return TRUE;
            case GDK_KEY_q: close_folder(); show_welcome_screen(); ctrl_k_pending = FALSE; return TRUE;
            
            case GDK_KEY_t: gtk_widget_set_visible(bottom_panel, !gtk_widget_get_visible(bottom_panel)); ctrl_k_pending = FALSE; return TRUE;
            case GDK_KEY_g: gtk_widget_set_visible(chat_panel, !gtk_widget_get_visible(chat_panel)); ctrl_k_pending = FALSE; return TRUE;
            
            case GDK_KEY_i: move_cursor_up(); return TRUE;
            case GDK_KEY_k: move_cursor_down(); return TRUE;
            case GDK_KEY_j: move_cursor_left(); return TRUE;
            case GDK_KEY_l: move_cursor_right(); return TRUE;
            
            case GDK_KEY_c: 
                if (!is_terminal) {
                    gtk_text_buffer_copy_clipboard(GTK_TEXT_BUFFER(text_buffer), gtk_clipboard_get(GDK_SELECTION_CLIPBOARD)); 
                    ctrl_k_pending = FALSE;
                    return TRUE;
                }
                break;
            case GDK_KEY_v: 
                if (!is_terminal) {
                    gtk_text_buffer_paste_clipboard(GTK_TEXT_BUFFER(text_buffer), gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), NULL, TRUE); 
                    ctrl_k_pending = FALSE;
                    return TRUE;
                }
                break;
            case GDK_KEY_x: gtk_text_buffer_cut_clipboard(GTK_TEXT_BUFFER(text_buffer), gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), TRUE); ctrl_k_pending = FALSE; return TRUE;
            case GDK_KEY_z: 
                if (shift) {
                    if (gtk_source_buffer_can_redo(text_buffer)) gtk_source_buffer_redo(text_buffer);
                } else {
                    if (gtk_source_buffer_can_undo(text_buffer)) gtk_source_buffer_undo(text_buffer);
                }
                ctrl_k_pending = FALSE; 
                return TRUE;
            case GDK_KEY_y: if (gtk_source_buffer_can_redo(text_buffer)) gtk_source_buffer_redo(text_buffer); ctrl_k_pending = FALSE; return TRUE;
            
            case GDK_KEY_w:
                if (ctrl_k_pending) {
                    close_all_files();
                    ctrl_k_pending = FALSE;
                    return TRUE;
                }
                break;
        }
    } else {
        // Plain key pressed, check for chord continuation without Ctrl
        if (ctrl_k_pending) {
            if (gdk_keyval_to_lower(event->keyval) == GDK_KEY_w) {
                close_all_files();
                ctrl_k_pending = FALSE;
                return TRUE;
            }
            ctrl_k_pending = FALSE; // Reset if any other key is pressed
            update_status_with_relative_path();
        }
    }
    return FALSE;
}

void show_welcome_screen() {
    gtk_stack_set_visible_child_name(GTK_STACK(editor_stack), "welcome");
}

void show_editor_view() {
    if (strlen(current_file) == 0) {
        show_empty_state();
    } else {
        gtk_stack_set_visible_child_name(GTK_STACK(editor_stack), "editor");
    }
}

static void on_open_folder_clicked(GtkButton *btn, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Open Folder",
        GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        open_folder(path);
        g_free(path);
    }
    gtk_widget_destroy(dialog);
}

static void on_recent_clicked(GtkButton *btn, gpointer data) {
    char *path = (char *)data;
    open_folder(path);
}

static GtkWidget* create_welcome_screen() {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold'>Welcome to Caecode</span>");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    GtkWidget *subtitle = gtk_label_new("A lightweight, premium editor for GNOME");
    gtk_label_set_line_wrap(GTK_LABEL(subtitle), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(subtitle), 40);
    gtk_label_set_justify(GTK_LABEL(subtitle), GTK_JUSTIFY_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "dim-label");
    gtk_box_pack_start(GTK_BOX(box), subtitle, FALSE, FALSE, 0);

    GtkWidget *btn_open = gtk_button_new_with_label("Open Folder...");
    gtk_widget_set_size_request(btn_open, 200, 40);
    g_signal_connect(btn_open, "clicked", G_CALLBACK(on_open_folder_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(box), btn_open, FALSE, FALSE, 20);

    // Recent Projects
    GList *recent = get_recent_folders();
    if (recent) {
        GtkWidget *recent_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(recent_label), "<b>Recent Projects</b>");
        gtk_box_pack_start(GTK_BOX(box), recent_label, FALSE, FALSE, 10);

        GtkWidget *recent_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        for (GList *l = recent; l != NULL; l = l->next) {
            char *path = (char *)l->data;
            char *basename = g_path_get_basename(path);
            GtkWidget *recent_btn = gtk_button_new_with_label(basename);
            gtk_button_set_relief(GTK_BUTTON(recent_btn), GTK_RELIEF_NONE);
            g_signal_connect(recent_btn, "clicked", G_CALLBACK(on_recent_clicked), g_strdup(path));
            gtk_box_pack_start(GTK_BOX(recent_box), recent_btn, FALSE, FALSE, 0);
            g_free(basename);
        }
        gtk_box_pack_start(GTK_BOX(box), recent_box, FALSE, FALSE, 0);
        g_list_free_full(recent, g_free);
    }

    return box;
}

static GtkWidget* create_empty_state() {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_valign(box, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);

    GtkWidget *icon = gtk_image_new_from_icon_name("document-open-recent-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_style_context_add_class(gtk_widget_get_style_context(icon), "dim-label");
    gtk_box_pack_start(GTK_BOX(box), icon, FALSE, FALSE, 0);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='x-large' weight='bold'>No File Selected</span>");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 0);

    GtkWidget *subtitle = gtk_label_new("Select a file from the sidebar or use shortcuts");
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitle), "dim-label");
    gtk_box_pack_start(GTK_BOX(box), subtitle, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 20);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);

    const char *shortcuts[][2] = {
        {"Search File", "Ctrl + P"},
        {"Open Folder", "Ctrl + O"},
        {"Terminal", "Ctrl + T"},
        {"Show Chat", "Ctrl + G"}
    };

    for (int i = 0; i < 4; i++) {
        GtkWidget *l1 = gtk_label_new(shortcuts[i][0]);
        gtk_widget_set_halign(l1, GTK_ALIGN_START);
        GtkWidget *l2 = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(l2), g_strdup_printf("<b>%s</b>", shortcuts[i][1]));
        gtk_widget_set_halign(l2, GTK_ALIGN_END);
        gtk_style_context_add_class(gtk_widget_get_style_context(l1), "dim-label");
        gtk_style_context_add_class(gtk_widget_get_style_context(l2), "dim-label");

        gtk_grid_attach(GTK_GRID(grid), l1, 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), l2, 1, i, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(box), grid, FALSE, FALSE, 20);

    return box;
}

static void on_bottom_panel_size_allocate(GtkWidget *widget, GdkRectangle *allocation, gpointer user_data) {
    GtkPaned *paned = GTK_PANED(user_data);
    int total_width = allocation->width;
    // Lock the list (right side) to 15% of total width.
    // So the handle position (left side width) should be 85%.
    int position = (int)(total_width * 0.85);
    gtk_paned_set_position(paned, position);
}

static void on_terminal_list_row_selected(GtkListBox *list, GtkListBoxRow *row, gpointer user_data) {
    if (!row) return;
    const char *name = g_object_get_data(G_OBJECT(row), "terminal-name");
    gtk_stack_set_visible_child_name(GTK_STACK(terminal_stack), name);
}

static GtkWidget* create_bottom_panel() {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(vbox, "bottom-panel");
    gtk_widget_set_size_request(vbox, -1, 230);

    // Header / Nav Bar
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_style_context_add_class(gtk_widget_get_style_context(header), "terminal-toolbar");
    
    // Just a static title label
    GtkWidget *title = gtk_label_new("TERMINAL");
    gtk_widget_set_name(title, "terminal-header-item");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "terminal-header-item");
    gtk_style_context_add_class(gtk_widget_get_style_context(title), "active");
    gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 0);
    gtk_widget_set_margin_start(title, 10);
    
    GtkWidget *header_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *btn_new = gtk_button_new_from_icon_name("list-add-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(btn_new), GTK_RELIEF_NONE);
    g_signal_connect(btn_new, "clicked", G_CALLBACK(create_new_terminal), NULL);
    gtk_box_pack_start(GTK_BOX(header_actions), btn_new, FALSE, FALSE, 0);
    
    gtk_box_pack_end(GTK_BOX(header), header_actions, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);

    // Main Content: Terminal + List
    GtkWidget *h_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    
    terminal_stack = gtk_stack_new();
    gtk_widget_set_margin_start(terminal_stack, 10);
    gtk_stack_set_transition_type(GTK_STACK(terminal_stack), GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_paned_pack1(GTK_PANED(h_paned), terminal_stack, TRUE, FALSE);
    
    GtkWidget *list_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(list_scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(list_scrolled, 150, -1);
    gtk_style_context_add_class(gtk_widget_get_style_context(list_scrolled), "terminal-session-list");
    
    terminal_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(terminal_list), GTK_SELECTION_SINGLE);
    g_signal_connect(terminal_list, "row-selected", G_CALLBACK(on_terminal_list_row_selected), NULL);
    gtk_container_add(GTK_CONTAINER(list_scrolled), terminal_list);
    
    gtk_paned_pack2(GTK_PANED(h_paned), list_scrolled, FALSE, FALSE);
    
    // Lock to 15% width dynamically
    g_signal_connect(h_paned, "size-allocate", G_CALLBACK(on_bottom_panel_size_allocate), h_paned);

    gtk_box_pack_start(GTK_BOX(vbox), h_paned, TRUE, TRUE, 0);
    
    return vbox;
}

void create_new_terminal() {
    static int term_count = 0;
    char term_id[32];
    sprintf(term_id, "term-%d", ++term_count);

    GtkWidget *terminal = vte_terminal_new();
    gtk_widget_set_name(terminal, "vte-terminal");
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), 10000);
    
    char **envp = g_get_environ();
    char **command = (char *[]){ "/bin/bash", NULL };
    const char *work_dir = (strlen(current_folder) > 0) ? current_folder : NULL;

    vte_terminal_spawn_async(VTE_TERMINAL(terminal), VTE_PTY_DEFAULT, work_dir, command, envp, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, -1, NULL, NULL, NULL);
    g_strfreev(envp);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), terminal);
    gtk_widget_show_all(scrolled_window);
    
    gtk_stack_add_named(GTK_STACK(terminal_stack), scrolled_window, term_id);

    // List Row
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon = gtk_image_new_from_icon_name("utilities-terminal-symbolic", GTK_ICON_SIZE_MENU);
    gtk_box_pack_start(GTK_BOX(row_box), icon, FALSE, FALSE, 5);
    
    char label_text[32];
    sprintf(label_text, "bash (%d)", term_count);
    GtkWidget *label = gtk_label_new(label_text);
    gtk_box_pack_start(GTK_BOX(row_box), label, TRUE, TRUE, 0);
    
    GtkWidget *btn_close = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(btn_close), GTK_RELIEF_NONE);
    gtk_box_pack_start(GTK_BOX(row_box), btn_close, FALSE, FALSE, 0);
    gtk_widget_show_all(row_box);

    GtkWidget *list_row = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(list_row), row_box);
    g_object_set_data_full(G_OBJECT(list_row), "terminal-name", g_strdup(term_id), g_free);
    gtk_widget_show_all(list_row);
    
    gtk_list_box_insert(GTK_LIST_BOX(terminal_list), list_row, -1);
    
    // Sync
    gtk_list_box_select_row(GTK_LIST_BOX(terminal_list), GTK_LIST_BOX_ROW(list_row));
    
    // Memory/Widget Management
    g_signal_connect_swapped(btn_close, "clicked", G_CALLBACK(gtk_widget_destroy), scrolled_window);
    g_signal_connect_swapped(btn_close, "clicked", G_CALLBACK(gtk_widget_destroy), list_row);
    g_signal_connect_swapped(terminal, "child-exited", G_CALLBACK(gtk_widget_destroy), list_row);

    // Ensure new terminal inherits correct theme colors
    apply_theme(current_theme_idx);
}

static GtkWidget* create_chat_panel() {
    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(panel, "chat-panel");
    gtk_widget_set_size_request(panel, 300, -1);
    
    GtkWidget *label = gtk_label_new("AI Conversation");
    gtk_style_context_add_class(gtk_widget_get_style_context(label), "dim-label");
    gtk_box_pack_start(GTK_BOX(panel), label, TRUE, TRUE, 0);
    
    return panel;
}

void create_main_window() {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Caecode");
    gtk_window_set_default_size(GTK_WINDOW(window), 1100, 750);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *main_h_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *inner_h_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *nested_v_paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);

    gtk_box_pack_start(GTK_BOX(vbox), main_h_paned, TRUE, TRUE, 0);

    // Initialize Global CSS Provider before modules that use it
    app_css_provider = gtk_css_provider_new();
    GtkStyleContext *screen_ctx = gtk_widget_get_style_context(window);
    gtk_style_context_add_provider(screen_ctx, GTK_STYLE_PROVIDER(app_css_provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Sidebar - Pack into inner_h_paned Slot 1
    init_sidebar();
    sidebar_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar_scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_name(sidebar_scrolled_window, "sidebar-scrolledwindow");
    gtk_container_add(GTK_CONTAINER(sidebar_scrolled_window), tree_view);
    
    gtk_paned_pack1(GTK_PANED(inner_h_paned), sidebar_scrolled_window, FALSE, FALSE);

    // Create bottom panel (terminals) early so it's ready for theming
    bottom_panel = create_bottom_panel();

    // Editor Area Stack
    init_editor();
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(source_view), 20);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(source_view), 20);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(source_view), 15);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(source_view), 15);

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, "textview, textview text { font-family: 'Monospace'; font-size: 11pt; }", -1, NULL);
    gtk_style_context_add_provider(gtk_widget_get_style_context(GTK_WIDGET(source_view)), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    GtkWidget *editor_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(editor_scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(editor_scrolled_window), GTK_WIDGET(source_view));

    editor_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(editor_stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(editor_stack), 300);
    
    welcome_screen = create_welcome_screen();
    gtk_widget_set_name(welcome_screen, "welcome-screen");
    
    empty_state = create_empty_state();
    gtk_widget_set_name(empty_state, "empty-state");

    GtkWidget *welcome_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(welcome_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(welcome_scroll), welcome_screen);
    gtk_widget_show_all(welcome_scroll);

    GtkWidget *empty_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(empty_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(empty_scroll), empty_state);
    gtk_widget_show_all(empty_scroll);

    gtk_stack_add_named(GTK_STACK(editor_stack), welcome_scroll, "welcome");
    gtk_stack_add_named(GTK_STACK(editor_stack), empty_scroll, "empty");
    gtk_stack_add_named(GTK_STACK(editor_stack), editor_scrolled_window, "editor");

    // Nesting logic
    gtk_paned_pack1(GTK_PANED(nested_v_paned), editor_stack, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(nested_v_paned), bottom_panel, FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(inner_h_paned), nested_v_paned, TRUE, FALSE);
    gtk_paned_pack1(GTK_PANED(main_h_paned), inner_h_paned, TRUE, FALSE);
    
    chat_panel = create_chat_panel();
    gtk_paned_pack2(GTK_PANED(main_h_paned), chat_panel, FALSE, FALSE);

    // Dynamic Sizing: 90% of primary monitor
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);

    int win_width = geometry.width * 0.85;
    int win_height = geometry.height * 0.85;
    gtk_window_set_default_size(GTK_WINDOW(window), win_width, win_height);

    if (strlen(current_folder) == 0) show_welcome_screen(); 
    else {
        show_editor_view();
        create_new_terminal(); 
    }

    // Defaults (Dynamic proportions)
    gtk_paned_set_position(GTK_PANED(inner_h_paned), 280);                    // Sidebar fix
    gtk_paned_set_position(GTK_PANED(nested_v_paned), win_height * 0.65);     // Editor vs Terminal
    gtk_paned_set_position(GTK_PANED(main_h_paned), win_width - 300);          // Right Chat area


    // Status bar
    status_bar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), status_bar, FALSE, FALSE, 0);
    set_status_message("Welcome to Caecode");

    init_search_popup();

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);

    gtk_widget_show_all(window);
}
