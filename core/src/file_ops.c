#include "file_ops.h"
#include <string.h>

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

    // Apply to UI (main thread)
    g_strlcpy(current_file, ctx->path, sizeof(current_file));
    gtk_source_buffer_begin_not_undoable_action(text_buffer);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(text_buffer), contents, (gint)len);
    gtk_source_buffer_end_not_undoable_action(text_buffer);

    if (last_saved_content) g_free(last_saved_content);
    last_saved_content = g_strdup(contents);

    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
    GtkSourceLanguage *language = gtk_source_language_manager_guess_language(lm, ctx->path, NULL);
    gtk_source_buffer_set_language(text_buffer, language);

    mark_unsaved_file(ctx->path, FALSE);
    update_status_with_unsaved_mark(TRUE);
    select_file_in_sidebar(ctx->path);

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
    GError *err = NULL;
    if (!g_file_replace_contents_finish(G_FILE(src), res, NULL, &err)) {
        if (err) {
            set_status_message(err->message);
            g_error_free(err);
        }
        return;
    }
    set_status_message("File saved successfully");
    mark_unsaved_file(current_file, FALSE);
    update_status_with_unsaved_mark(TRUE);
}

void save_file() {
    if (strlen(current_file) == 0) return;

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(text_buffer), &start, &end);
    char *text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(text_buffer), &start, &end, FALSE);

    if (last_saved_content) g_free(last_saved_content);
    last_saved_content = g_strdup(text);

    GFile *gf = g_file_new_for_path(current_file);
    g_file_replace_contents_async(gf,
        text, strlen(text),
        NULL, FALSE, G_FILE_CREATE_NONE,
        NULL, on_file_saved, NULL);
    g_object_unref(gf);

    g_free(text);
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
