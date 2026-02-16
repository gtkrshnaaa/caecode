#ifndef EDITOR_H
#define EDITOR_H

#include "app_state.h"

void init_editor();
void switch_theme();
void on_text_changed(GtkTextBuffer *buffer, gpointer user_data);

#endif // EDITOR_H
