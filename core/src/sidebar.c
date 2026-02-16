#include "sidebar.h"
#include "file_ops.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

typedef struct {
    char *path;
    GtkTreeIter parent_iter;
    gboolean has_parent;
} DirTask;

typedef struct {
    GQueue *queue;
    gboolean cancelled;
    guint source_id;
} PopulateContext;

static PopulateContext *populate_ctx = NULL;
static GFileMonitor *folder_monitor = NULL;
static guint refresh_timeout_id = 0;

static void stop_population_if_running() {
    if (populate_ctx) {
        populate_ctx->cancelled = TRUE;
        if (populate_ctx->source_id > 0) {
            g_source_remove(populate_ctx->source_id);
        }
        void *data;
        while ((data = g_queue_pop_head(populate_ctx->queue))) {
            DirTask *t = (DirTask *)data;
            g_free(t->path);
            g_free(t);
        }
        g_queue_free(populate_ctx->queue);
        g_free(populate_ctx);
        populate_ctx = NULL;
    }
}

static void add_to_tree(const char *path, const char *name, const char *icon_name, GtkTreeIter *parent, GtkTreeIter *iter) {
    gtk_tree_store_append(tree_store, iter, parent);
    gtk_tree_store_set(tree_store, iter, 0, icon_name, 1, name, 2, path, -1);
}

static gboolean populate_step(gpointer data) {
    if (!populate_ctx || populate_ctx->cancelled) return FALSE;

    DirTask *task = g_queue_pop_head(populate_ctx->queue);
    if (!task) {
        populate_ctx->source_id = 0;
        return FALSE;
    }

    DIR *dir = opendir(task->path);
    if (dir) {
        struct dirent *entry;
        GList *subdirs = NULL;
        GList *files = NULL;

        while ((entry = readdir(dir))) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            if (entry->d_name[0] == '.') continue;

            char *name = g_strdup(entry->d_name);
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", task->path, name);

            struct stat st;
            if (stat(full_path, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    subdirs = g_list_insert_sorted(subdirs, name, (GCompareFunc)g_ascii_strcasecmp);
                } else {
                    files = g_list_insert_sorted(files, name, (GCompareFunc)g_ascii_strcasecmp);
                }
            } else {
                g_free(name);
            }
        }
        closedir(dir);

        // Add subdirectories first (folders at top)
        for (GList *l = subdirs; l != NULL; l = l->next) {
            char *name = (char *)l->data;
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", task->path, name);
            
            GtkTreeIter iter;
            add_to_tree(full_path, name, "folder-symbolic", task->has_parent ? &task->parent_iter : NULL, &iter);

            DirTask *subtask = g_new0(DirTask, 1);
            subtask->path = g_strdup(full_path);
            subtask->parent_iter = iter;
            subtask->has_parent = TRUE;
            g_queue_push_tail(populate_ctx->queue, subtask);
            
            file_list = g_list_append(file_list, g_strdup(full_path));
        }

        // Add files after subdirectories
        for (GList *l = files; l != NULL; l = l->next) {
            char *name = (char *)l->data;
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", task->path, name);
            
            GtkTreeIter iter;
            add_to_tree(full_path, name, "text-x-generic-symbolic", task->has_parent ? &task->parent_iter : NULL, &iter);
            
            file_list = g_list_append(file_list, g_strdup(full_path));
        }

        g_list_free_full(subdirs, g_free);
        g_list_free_full(files, g_free);
    }

    g_free(task->path);
    g_free(task);

    if (g_queue_is_empty(populate_ctx->queue)) {
        populate_ctx->source_id = 0;
        return FALSE;
    }
    return TRUE;
}

static gboolean debounced_refresh(gpointer data) {
    refresh_timeout_id = 0;
    reload_sidebar();
    return FALSE;
}

static void on_folder_changed(GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data) {
    if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT || 
        event_type == G_FILE_MONITOR_EVENT_CREATED || 
        event_type == G_FILE_MONITOR_EVENT_DELETED ||
        event_type == G_FILE_MONITOR_EVENT_RENAMED ||
        event_type == G_FILE_MONITOR_EVENT_MOVED) {
        
        if (refresh_timeout_id > 0) g_source_remove(refresh_timeout_id);
        refresh_timeout_id = g_timeout_add(500, debounced_refresh, NULL);
    }
}

void open_folder(const char *path) {
    stop_population_if_running();
    gtk_tree_store_clear(tree_store);
    g_list_free_full(file_list, g_free);
    file_list = NULL;
    g_strlcpy(current_folder, path, sizeof(current_folder));

    populate_ctx = g_new0(PopulateContext, 1);
    populate_ctx->queue = g_queue_new();
    
    DirTask *root_task = g_new0(DirTask, 1);
    root_task->path = g_strdup(path);
    root_task->has_parent = FALSE;
    g_queue_push_tail(populate_ctx->queue, root_task);

    populate_ctx->source_id = g_idle_add(populate_step, NULL);

    // Setup file monitor
    if (folder_monitor) {
        g_file_monitor_cancel(folder_monitor);
        g_object_unref(folder_monitor);
    }
    GFile *gf = g_file_new_for_path(path);
    folder_monitor = g_file_monitor_directory(gf, G_FILE_MONITOR_NONE, NULL, NULL);
    g_signal_connect(folder_monitor, "changed", G_CALLBACK(on_folder_changed), NULL);
    g_object_unref(gf);

    save_recent_folder(path);
    show_editor_view(); // Will show empty state as current_file is empty
}

void close_folder() {
    stop_population_if_running();
    gtk_tree_store_clear(tree_store);
    g_list_free_full(file_list, g_free);
    file_list = NULL;
    current_folder[0] = '\0';
    if (current_file_row_ref) {
        gtk_tree_row_reference_free(current_file_row_ref);
        current_file_row_ref = NULL;
    }

    if (folder_monitor) {
        g_file_monitor_cancel(folder_monitor);
        g_object_unref(folder_monitor);
        folder_monitor = NULL;
    }
    if (refresh_timeout_id > 0) {
        g_source_remove(refresh_timeout_id);
        refresh_timeout_id = 0;
    }

    show_welcome_screen();
}

void reload_sidebar() {
    if (strlen(current_folder) > 0) {
        open_folder(current_folder);
    }
}


static void on_row_activated(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer user_data) {
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(tv);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        char *filepath;
        gtk_tree_model_get(model, &iter, 2, &filepath, -1);
        
        struct stat st;
        if (stat(filepath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Toggle expansion for folders on single click
                if (gtk_tree_view_row_expanded(tv, path)) {
                    gtk_tree_view_collapse_row(tv, path);
                } else {
                    gtk_tree_view_expand_row(tv, path, FALSE);
                }
            } else if (S_ISREG(st.st_mode)) {
                load_file_async(filepath);
            }
        }
        g_free(filepath);
    }
}

void init_sidebar() {
    tree_store = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_store));
    
    // Enable single-click activation
    gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(tree_view), TRUE);

    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "Files");

    GtkCellRenderer *icon_renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, icon_renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, icon_renderer, "icon-name", 0);

    GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, text_renderer, "text", 1);

    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    g_signal_connect(tree_view, "row-activated", G_CALLBACK(on_row_activated), NULL);
}

void select_file_in_sidebar_recursive(GtkTreeModel *model, GtkTreeIter *iter, const char *filepath) {
    do {
        char *path;
        gtk_tree_model_get(model, iter, 2, &path, -1);
        if (path && strcmp(path, filepath) == 0) {
            GtkTreePath *tree_path = gtk_tree_model_get_path(model, iter);
            gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree_view), tree_path);
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree_view), tree_path, NULL, FALSE);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(tree_view), tree_path, NULL, TRUE, 0.5, 0.0);
            if (current_file_row_ref) gtk_tree_row_reference_free(current_file_row_ref);
            current_file_row_ref = gtk_tree_row_reference_new(model, tree_path);
            gtk_tree_path_free(tree_path);
            g_free(path);
            return;
        }
        GtkTreeIter child_iter;
        if (gtk_tree_model_iter_children(model, &child_iter, iter)) {
            select_file_in_sidebar_recursive(model, &child_iter, filepath);
        }
        g_free(path);
    } while (gtk_tree_model_iter_next(model, iter));
}

void select_file_in_sidebar(const char *filepath) {
    GtkTreeModel *model = GTK_TREE_MODEL(tree_store);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            char *path;
            gtk_tree_model_get(model, &iter, 2, &path, -1);
            if (path && strcmp(path, filepath) == 0) {
                GtkTreePath *tree_path = gtk_tree_model_get_path(model, &iter);
                gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree_view), tree_path);
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree_view), tree_path, NULL, FALSE);
                gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(tree_view), tree_path, NULL, TRUE, 0.5, 0.0);
                if (current_file_row_ref) gtk_tree_row_reference_free(current_file_row_ref);
                current_file_row_ref = gtk_tree_row_reference_new(model, tree_path);
                gtk_tree_path_free(tree_path);
                g_free(path);
                return;
            }
            GtkTreeIter child_iter;
            if (gtk_tree_model_iter_children(model, &child_iter, &iter)) {
                select_file_in_sidebar_recursive(model, &child_iter, filepath);
            }
            g_free(path);
        } while (gtk_tree_model_iter_next(model, &iter));
    }
}

void mark_unsaved_file(const char *filepath, gboolean unsaved) {
    if (current_file_row_ref && gtk_tree_row_reference_valid(current_file_row_ref)) {
        GtkTreePath *path = gtk_tree_row_reference_get_path(current_file_row_ref);
        GtkTreeIter iter;
        GtkTreeModel *model = GTK_TREE_MODEL(tree_store);
        if (gtk_tree_model_get_iter(model, &iter, path)) {
            char *filename = g_path_get_basename(filepath);
            char display_name[1024];
            snprintf(display_name, sizeof(display_name), "%s%s", filename, unsaved ? " *" : "");
            gtk_tree_store_set(tree_store, &iter, 1, display_name, -1);
            g_free(filename);
        }
        gtk_tree_path_free(path);
    }
}
