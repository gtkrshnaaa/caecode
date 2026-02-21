#include "sidebar.h"
#include "file_ops.h"
#include "editor.h"
#include "ui.h"
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <gio/gunixinputstream.h>

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
static guint git_poll_timeout_id = 0;
static GHashTable *global_git_status = NULL;

const char* get_git_status_letter(const char *path) {
    if (!global_git_status || !path) return "";
    const char *status = g_hash_table_lookup(global_git_status, path);
    if (!status) return "";
    
    if (strstr(status, "M")) return "M";
    if (strstr(status, "A")) return "A";
    if (strstr(status, "?")) return "U";
    return "";
}

static gboolean background_git_poll(gpointer data) {
    update_git_status();
    update_git_gutter();
    return TRUE; // Continue polling
}

// Bitmasks for Git status
#define GIT_STATUS_NONE 0
#define GIT_STATUS_MODIFIED 1
#define GIT_STATUS_ADDED 2
#define GIT_STATUS_UNTRACKED 4

static int update_tree_colors_recursive(GtkTreeModel *model, GtkTreeIter *iter, GHashTable *git_status) {
    int aggregate_status = GIT_STATUS_NONE;
    
    do {
        char *path;
        gtk_tree_model_get(model, iter, 2, &path, -1);
        
        int current_node_status = GIT_STATUS_NONE;
        const char *status = g_hash_table_lookup(git_status, path);
        const char *color = NULL;
        const char *letter = "";

        if (status) {
            if (strstr(status, "M")) {
                current_node_status |= GIT_STATUS_MODIFIED;
                color = "#E2C08D"; // Yellow
                letter = "M";
            } else if (strstr(status, "A")) {
                current_node_status |= GIT_STATUS_ADDED;
                color = "#73C991"; // Green
                letter = "A";
            } else if (strstr(status, "?")) {
                current_node_status |= GIT_STATUS_UNTRACKED;
                color = "#73C991"; // Green
                letter = "U";
            }
        }
        
        GtkTreeIter child;
        int children_status = GIT_STATUS_NONE;
        if (gtk_tree_model_iter_children(model, &child, iter)) {
            children_status = update_tree_colors_recursive(model, &child, git_status);
        }
        
        // If the current node isn't explicitly colored, but its children have changes, bubble up the color.
        // We do NOT bubble up the letter, only the color, just like VSCode for parent folders.
        if (!color && children_status != GIT_STATUS_NONE) {
            if (children_status & GIT_STATUS_MODIFIED) color = "#E2C08D";
            else if ((children_status & GIT_STATUS_ADDED) || (children_status & GIT_STATUS_UNTRACKED)) color = "#73C991";
        }

        gtk_tree_store_set(GTK_TREE_STORE(model), iter, 3, color, 4, letter, -1);
        
        aggregate_status |= current_node_status | children_status;
        g_free(path);
        
    } while (gtk_tree_model_iter_next(model, iter));
    
    return aggregate_status;
}

typedef struct {
    GString *buffer;
} GitStatusCtx;

static void on_git_status_read(GObject *source_object, GAsyncResult *res, gpointer user_data) {
    GitStatusCtx *ctx = (GitStatusCtx *)user_data;
    GInputStream *stream = G_INPUT_STREAM(source_object);
    GError *err = NULL;
    char buf[4096];
    gssize bytes_read = g_input_stream_read_finish(stream, res, &err);

    if (bytes_read > 0) {
        g_string_append_len(ctx->buffer, buf, bytes_read);
        g_input_stream_read_async(stream, buf, sizeof(buf), G_PRIORITY_DEFAULT, NULL, on_git_status_read, ctx);
    } else {
        if (err) {
            g_error_free(err);
        } else if (ctx->buffer->len > 0) {
            GHashTable *git_status = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
            char **lines = g_strsplit(ctx->buffer->str, "\n", -1);
            
            for (int i = 0; lines[i] && strlen(lines[i]) > 3; i++) {
                char status[3];
                strncpy(status, lines[i], 2);
                status[2] = '\0';
                
                char *rel_path = lines[i] + 3;
                if (rel_path[0] == '"') {
                    // Primitive unquoting if needed, or leave for exact match
                }
                
                char *abs_path = g_build_filename(current_folder, rel_path, NULL);
                g_hash_table_insert(git_status, abs_path, g_strdup(status));
            }
            g_strfreev(lines);

            GtkTreeIter iter;
            if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(tree_store), &iter)) {
                update_tree_colors_recursive(GTK_TREE_MODEL(tree_store), &iter, git_status);
            }
            g_hash_table_destroy(git_status);
        }
        
        g_string_free(ctx->buffer, TRUE);
        g_free(ctx);
        g_input_stream_close_async(stream, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
        g_object_unref(stream);
    }
}

static void on_git_status_child_watch(GPid pid, gint status, gpointer user_data) {
    g_spawn_close_pid(pid);
}

static void on_git_status_spliced(GObject *source, GAsyncResult *res, gpointer user_data) {
    GOutputStream *out = G_OUTPUT_STREAM(source);
    GError *splice_err = NULL;
    if (g_output_stream_splice_finish(out, res, &splice_err) >= 0) {
        gsize size = g_memory_output_stream_get_data_size(G_MEMORY_OUTPUT_STREAM(out));
        if (size > 0) {
            char *output = g_strndup(g_memory_output_stream_get_data(G_MEMORY_OUTPUT_STREAM(out)), size);
            
            GHashTable *git_status = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
            char **lines = g_strsplit(output, "\n", -1);
            
            for (int i = 0; lines[i] && strlen(lines[i]) > 3; i++) {
                char status[3];
                strncpy(status, lines[i], 2);
                status[2] = '\0';
                
                char *rel_path = lines[i] + 3;
                char *abs_path = g_build_filename(current_folder, rel_path, NULL);
                g_hash_table_insert(git_status, abs_path, g_strdup(status));
            }
            g_strfreev(lines);
            g_free(output);

            GtkTreeIter iter;
            if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(tree_store), &iter)) {
                update_tree_colors_recursive(GTK_TREE_MODEL(tree_store), &iter, git_status);
            }

            // Sync with global cache for UI bars
            if (global_git_status) g_hash_table_destroy(global_git_status);
            global_git_status = git_status;
            
            // Refresh UI bars
            update_path_bar();
            update_status_with_unsaved_mark(!gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(text_buffer)));
        } else {
            // No git results, clear global status
            if (global_git_status) {
                g_hash_table_destroy(global_git_status);
                global_git_status = NULL;
            }
            update_path_bar();
            update_status_with_unsaved_mark(!gtk_text_buffer_get_modified(GTK_TEXT_BUFFER(text_buffer)));
        }
    }
    if (splice_err) g_error_free(splice_err);
    g_object_unref(out);
}

void update_git_status() {
    if (strlen(current_folder) == 0) return;

    char *argv[] = { "git", "-C", current_folder, "status", "--porcelain", "-u", NULL };
    
    gint standard_output;
    GPid pid;
    GError *err = NULL;

    if (g_spawn_async_with_pipes(current_folder, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL, NULL, &pid, NULL, &standard_output, NULL, &err)) {
        
        GitStatusCtx *ctx = g_new0(GitStatusCtx, 1);
        ctx->buffer = g_string_new("");

        GInputStream *stream = g_unix_input_stream_new(standard_output, TRUE);
        char *buf = g_malloc(4096);
        g_free(buf); 
        
        GOutputStream *mem_stream = g_memory_output_stream_new(NULL, 0, g_realloc, g_free);
        g_output_stream_splice_async(mem_stream, stream, G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET, 
            G_PRIORITY_DEFAULT, NULL, on_git_status_spliced, NULL);
        
        g_object_unref(stream);
        g_child_watch_add(pid, on_git_status_child_watch, NULL);
    } else {
        if (err) g_error_free(err);
    }
}

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
    gtk_tree_store_set(tree_store, iter, 0, icon_name, 1, name, 2, path, 3, NULL, -1);
}

static GHashTable *expanded_paths = NULL;

static void save_expanded_state_recursive(GtkTreeModel *model, GtkTreeIter *iter) {
    do {
        char *path;
        gtk_tree_model_get(model, iter, 2, &path, -1);
        GtkTreePath *tree_path = gtk_tree_model_get_path(model, iter);
        
        if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(tree_view), tree_path)) {
            g_hash_table_add(expanded_paths, g_strdup(path));
        }
        
        gtk_tree_path_free(tree_path);
        g_free(path);
        
        GtkTreeIter child;
        if (gtk_tree_model_iter_children(model, &child, iter)) {
            save_expanded_state_recursive(model, &child);
        }
    } while (gtk_tree_model_iter_next(model, iter));
}

static void restore_expanded_state_recursive(GtkTreeModel *model, GtkTreeIter *iter) {
    do {
        char *path;
        gtk_tree_model_get(model, iter, 2, &path, -1);
        
        if (g_hash_table_contains(expanded_paths, path)) {
            GtkTreePath *tree_path = gtk_tree_model_get_path(model, iter);
            gtk_tree_view_expand_row(GTK_TREE_VIEW(tree_view), tree_path, FALSE);
            gtk_tree_path_free(tree_path);
        }
        g_free(path);
        
        GtkTreeIter child;
        if (gtk_tree_model_iter_children(model, &child, iter)) {
            restore_expanded_state_recursive(model, &child);
        }
    } while (gtk_tree_model_iter_next(model, iter));
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
        
        // Restore expansion state
        if (expanded_paths) {
            GtkTreeIter root_iter;
            if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(tree_store), &root_iter)) {
                restore_expanded_state_recursive(GTK_TREE_MODEL(tree_store), &root_iter);
            }
            g_hash_table_destroy(expanded_paths);
            expanded_paths = NULL; // Clear for next run
        }

        update_git_status(); // Trigger git update once the tree is fully built
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
    if (event_type == G_FILE_MONITOR_EVENT_CREATED || 
        event_type == G_FILE_MONITOR_EVENT_DELETED ||
        event_type == G_FILE_MONITOR_EVENT_RENAMED ||
        event_type == G_FILE_MONITOR_EVENT_MOVED) {
        
        if (refresh_timeout_id > 0) g_source_remove(refresh_timeout_id);
        refresh_timeout_id = g_timeout_add(500, debounced_refresh, NULL);
    }
}

void open_folder(const char *path) {
    if (expanded_paths) {
        g_hash_table_destroy(expanded_paths);
        expanded_paths = NULL;
    }
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
    update_git_status();
    show_editor_view(); // Will show empty state as current_file is empty
    ui_refresh_terminal_paths(path);
    
    if (sidebar_column) {
        char *basename = g_path_get_basename(path);
        if (basename) {
            gtk_tree_view_column_set_title(sidebar_column, basename);
            g_free(basename);
        }
    }
}

void close_folder() {
    if (expanded_paths) {
        g_hash_table_destroy(expanded_paths);
        expanded_paths = NULL;
    }
    if (global_git_status) {
        g_hash_table_destroy(global_git_status);
        global_git_status = NULL;
    }
    stop_population_if_running();
    gtk_tree_store_clear(tree_store);
    g_list_free_full(file_list, g_free);
    file_list = NULL;
    current_folder[0] = '\0';
    if (sidebar_column) {
        gtk_tree_view_column_set_title(sidebar_column, "Files");
    }
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
        if (!expanded_paths) {
            expanded_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
        } else {
            g_hash_table_remove_all(expanded_paths);
        }
        
        GtkTreeIter root_iter;
        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(tree_store), &root_iter)) {
            save_expanded_state_recursive(GTK_TREE_MODEL(tree_store), &root_iter);
        }
        
        stop_population_if_running();
        gtk_tree_store_clear(tree_store);
        g_list_free_full(file_list, g_free);
        file_list = NULL;

        populate_ctx = g_new0(PopulateContext, 1);
        populate_ctx->queue = g_queue_new();

        DirTask *root_task = g_new0(DirTask, 1);
        root_task->path = g_strdup(current_folder);
        root_task->has_parent = FALSE;
        g_queue_push_tail(populate_ctx->queue, root_task);

        populate_ctx->source_id = g_idle_add(populate_step, NULL);
    }
}

void collapse_all_folders() {
    if (tree_view) {
        gtk_tree_view_collapse_all(GTK_TREE_VIEW(tree_view));
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

GtkTreeViewColumn *sidebar_column = NULL;

void init_sidebar() {
    // 0: icon-name, 1: name, 2: path, 3: color, 4: git-status-letter
    tree_store = gtk_tree_store_new(5, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tree_store));
    
    // Enable single-click activation
    gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(tree_view), TRUE);

    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    sidebar_column = column;
    gtk_tree_view_column_set_title(column, "Files");

    GtkCellRenderer *icon_renderer = gtk_cell_renderer_pixbuf_new();
    gtk_tree_view_column_pack_start(column, icon_renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, icon_renderer, "icon-name", 0);

    GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new();
    g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, text_renderer, "text", 1);
    gtk_tree_view_column_add_attribute(column, text_renderer, "foreground", 3);

    // Git Status Letter (right-aligned)
    GtkCellRenderer *status_renderer = gtk_cell_renderer_text_new();
    g_object_set(status_renderer, "xalign", 1.0, NULL);
    gtk_tree_view_column_pack_start(column, status_renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, status_renderer, "text", 4);
    gtk_tree_view_column_add_attribute(column, status_renderer, "foreground", 3);

    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    g_signal_connect(tree_view, "row-activated", G_CALLBACK(on_row_activated), NULL);

    // Initial background git poll
    if (git_poll_timeout_id == 0) {
        git_poll_timeout_id = g_timeout_add_seconds(2, background_git_poll, NULL);
    }
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
