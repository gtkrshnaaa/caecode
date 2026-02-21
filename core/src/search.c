#include "search.h"
#include <string.h>
#include <dirent.h>
#include "file_ops.h"

GtkWidget *search_popup, *search_entry, *search_list;

static guint search_timeout_id = 0;

static void filter_file_list(const char *query) {
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(search_list)));
    gtk_list_store_clear(store);

    char *lower_query = g_ascii_strdown(query, -1);
    gboolean first = FALSE;
    int count = 0;

    for (GList *l = file_list; l != NULL; l = l->next) {
        if (count >= 100) break; // Hard cap UI elements to prevent rendering freeze
        
        const char *path = (const char *)l->data;
        char *lower_path = g_ascii_strdown(path, -1);
        
        if (g_strrstr(lower_path, lower_query) || strlen(lower_query) == 0) {
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter, 0, path, -1);
            count++;

            if (!first) {
                GtkTreePath *tp = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iter);
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(search_list), tp, NULL, FALSE);
                gtk_tree_path_free(tp);
                first = TRUE;
            }
        }
        g_free(lower_path);
    }
    g_free(lower_query);
}

static gboolean debounced_search(gpointer user_data) {
    search_timeout_id = 0;
    const char *text = gtk_entry_get_text(GTK_ENTRY(search_entry));
    filter_file_list(text);
    return FALSE;
}

static void on_search_entry_changed(GtkEditable *editable, gpointer user_data) {
    if (search_timeout_id > 0) g_source_remove(search_timeout_id);
    search_timeout_id = g_timeout_add(150, debounced_search, NULL);
}

static void activate_selected_file() {
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(search_list));
    GtkTreeModel *model;
    GtkTreeIter iter;
    if (gtk_tree_selection_get_selected(sel, &model, &iter)) {
        char *path;
        gtk_tree_model_get(model, &iter, 0, &path, -1);
        load_file_async(path);
        g_free(path);
    }
    gtk_widget_hide(search_popup);
}

static gboolean on_search_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    if (event->keyval == GDK_KEY_Escape) {
        gtk_widget_hide(search_popup);
        return TRUE;
    } else if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
        activate_selected_file();
        return TRUE;
    }
    return FALSE;
}

void init_search_popup() {
    search_popup = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_transient_for(GTK_WINDOW(search_popup), GTK_WINDOW(window));
    gtk_window_set_default_size(GTK_WINDOW(search_popup), 600, 300);
    gtk_window_set_modal(GTK_WINDOW(search_popup), TRUE);
    gtk_window_set_position(GTK_WINDOW(search_popup), GTK_WIN_POS_CENTER);
    gtk_window_set_decorated(GTK_WINDOW(search_popup), FALSE);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(search_popup), vbox);

    search_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), search_entry, FALSE, FALSE, 0);
    
    // Use GtkTreeView for sorting and selection consistent with original
    GtkListStore *list_store = gtk_list_store_new(1, G_TYPE_STRING);
    search_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("File Path", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(search_list), column);

    GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(sw), search_list);
    gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

    g_signal_connect(search_entry, "changed", G_CALLBACK(on_search_entry_changed), NULL);
    g_signal_connect(search_popup, "key-press-event", G_CALLBACK(on_search_key_press), NULL);
}

void show_search_popup() {
    gtk_widget_show_all(search_popup);
    gtk_widget_grab_focus(search_entry);
    filter_file_list("");
}
