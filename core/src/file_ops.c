#include "file_ops.h"
#include <string.h>
#include <sys/stat.h>

typedef struct {
    char *path;
    char *content;
} SaveCtx;

static void on_file_loaded(GObject *src, GAsyncResult *res, gpointer user_data) {
    LoadCtx *ctx = (LoadCtx *)user_data;
    gsize len = 0;
    char *contents = NULL;
    GError *err = NULL;
    gboolean ok = g_file_load_contents_finish(G_FILE(src), res, &contents, &len, NULL, &err);
    if (!ok) {
        if (err) {
            set_status_message(err->message);
            g_error_free(err);
        }
        g_free(ctx->path);
        g_free(ctx);
        return;
    }

    // 1. Update metadata first to be ready for signals
    g_strlcpy(current_file, ctx->path, sizeof(current_file));
    if (last_saved_content) g_free(last_saved_content);
    last_saved_content = g_strdup(contents);
    
    // 2. Select in sidebar to update current_file_row_ref
    select_file_in_sidebar(ctx->path);

    // 3. Update buffer (triggers "changed" signal)
    gtk_source_buffer_begin_not_undoable_action(text_buffer);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(text_buffer), contents, (gint)len);
    gtk_text_buffer_set_modified(GTK_TEXT_BUFFER(text_buffer), FALSE);
    gtk_source_buffer_end_not_undoable_action(text_buffer);

    // 4. Update language and final UI state
    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
    GtkSourceLanguage *language = gtk_source_language_manager_guess_language(lm, ctx->path, NULL);
    
    if (!language) {
        // Smart fallback for custom user languages (C-like)
        const char *ext = strrchr(ctx->path, '.');
        if (ext && (strcmp(ext, ".unna") == 0 || strcmp(ext, ".nva") == 0 || 
                    strcmp(ext, ".sna") == 0 || strcmp(ext, ".vana") == 0)) {
            language = gtk_source_language_manager_get_language(lm, "js");
        }
    }

    gtk_source_buffer_set_language(text_buffer, language);

    mark_unsaved_file(ctx->path, FALSE);
    update_status_with_unsaved_mark(TRUE);
    show_editor_view();

    g_free(contents);
    g_free(ctx->path);
    g_free(ctx);
}

void load_file_async(const char *filepath) {
    GFile *gf = g_file_new_for_path(filepath);
    LoadCtx *ctx = g_new0(LoadCtx, 1);
    ctx->path = g_strdup(filepath);

    g_file_load_contents_async(gf, NULL, on_file_loaded, ctx);
    g_object_unref(gf);
}

static void on_file_saved(GObject *src, GAsyncResult *res, gpointer user_data) {
    SaveCtx *ctx = (SaveCtx *)user_data;
    GError *err = NULL;
    if (!g_file_replace_contents_finish(G_FILE(src), res, NULL, &err)) {
        if (err) {
            set_status_message(err->message);
            g_error_free(err);
        }
        g_free(ctx->path);
        g_free(ctx->content);
        g_free(ctx);
        return;
    }
    set_status_message("File saved successfully");
    
    // Use the path from the context to ensure the correct file is marked
    mark_unsaved_file(ctx->path, FALSE);
    
    // Only update status if the saved file is the currently active one
    if (strcmp(ctx->path, current_file) == 0) {
        update_status_with_unsaved_mark(TRUE);
    }

    g_free(ctx->path);
    g_free(ctx->content);
    g_free(ctx);
}

void save_file() {
    if (strlen(current_file) == 0) return;

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(text_buffer), &start, &end);
    char *text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(text_buffer), &start, &end, FALSE);

    if (last_saved_content) g_free(last_saved_content);
    last_saved_content = g_strdup(text);
    gtk_text_buffer_set_modified(GTK_TEXT_BUFFER(text_buffer), FALSE);

    SaveCtx *ctx = g_new0(SaveCtx, 1);
    ctx->path = g_strdup(current_file);
    ctx->content = text; // Take ownership of the text buffer string

    GFile *gf = g_file_new_for_path(ctx->path);
    g_file_replace_contents_async(gf,
        ctx->content, strlen(ctx->content),
        NULL, FALSE, G_FILE_CREATE_NONE,
        NULL, on_file_saved, ctx);
    g_object_unref(gf);
}

void save_file_as() {
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save File As",
        GTK_WINDOW(window),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Save", GTK_RESPONSE_ACCEPT,
        NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        g_strlcpy(current_file, filename, sizeof(current_file));
        save_file();
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}
void save_recent_folder(const char *path) {
    if (!path || strlen(path) == 0) return;

    char *cache_dir = g_build_filename(g_get_user_cache_dir(), "caecode", NULL);
    g_mkdir_with_parents(cache_dir, 0755);
    char *recent_file = g_build_filename(cache_dir, "recent_folders.txt", NULL);

    GList *recent = get_recent_folders();
    
    // Check if path already exists, remove it to move it to top
    GList *found = g_list_find_custom(recent, path, (GCompareFunc)g_strcmp0);
    if (found) {
        g_free(found->data);
        recent = g_list_delete_link(recent, found);
    }

    recent = g_list_prepend(recent, g_strdup(path));

    // Limit to top 10 recent folders
    if (g_list_length(recent) > 10) {
        GList *last = g_list_last(recent);
        g_free(last->data);
        recent = g_list_delete_link(recent, last);
    }

    FILE *f = fopen(recent_file, "w");
    if (f) {
        for (GList *l = recent; l != NULL; l = l->next) {
            fprintf(f, "%s\n", (char *)l->data);
        }
        fclose(f);
    }

    g_list_free_full(recent, g_free);
    g_free(recent_file);
    g_free(cache_dir);
}

GList* get_recent_folders() {
    GList *recent = NULL;
    char *recent_file = g_build_filename(g_get_user_cache_dir(), "caecode", "recent_folders.txt", NULL);
    
    FILE *f = fopen(recent_file, "r");
    if (f) {
        char line[1024];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = 0;
            if (strlen(line) > 0) {
                recent = g_list_append(recent, g_strdup(line));
            }
        }
        fclose(f);
    }
    
    g_free(recent_file);
    return recent;
}
