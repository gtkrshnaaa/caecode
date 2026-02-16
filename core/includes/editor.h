#ifndef EDITOR_H
#define EDITOR_H

#include "app_state.h"

void init_editor();
void apply_theme(int index);
void switch_theme();
void on_text_changed(GtkTextBuffer *buffer, gpointer user_data);
void update_git_gutter();

#endif // EDITOR_H
