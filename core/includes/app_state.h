#ifndef APP_STATE_H
#define APP_STATE_H

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>

// Version and Constants
#define VERSION "0.0.5"

// Global UI Widgets (externed for module access)
extern GtkWidget *window;
extern GtkWidget *sidebar;
extern GtkWidget *status_bar;
extern GtkSourceView *source_view;
extern GtkSourceBuffer *text_buffer;
extern GtkTreeStore *tree_store;
extern GtkWidget *tree_view;
extern GtkSourceStyleSchemeManager *theme_manager;
extern GtkTreeRowReference *current_file_row_ref;
extern GList *file_list;
extern GtkCssProvider *app_css_provider;
extern GtkWidget *editor_stack;
extern GtkWidget *welcome_screen;

// Global State
extern char current_file[1024];
extern char current_folder[1024];
extern char *last_saved_content;

// Context Structs
typedef struct {
    char *path;
} LoadCtx;

typedef struct {
    GQueue *queue;
    guint idle_id;
} PopulateCtx;

typedef struct {
    char *path;
    GtkTreeIter *parent;
} DirEntry;

// Function Prototypes (forward declarations for cross-module calls)
void set_status_message(const char *message);
void update_status_with_unsaved_mark(gboolean is_same);
void mark_unsaved_file(const char *filepath, gboolean unsaved);
void select_file_in_sidebar(const char *filepath);
void show_welcome_screen();
void show_editor_view();
void save_recent_folder(const char *path);
GList* get_recent_folders();

#endif // APP_STATE_H
