#ifndef UI_H
#define UI_H

#include "app_state.h"

void create_main_window();
void set_status_message(const char *message);
void update_status_with_unsaved_mark(gboolean is_same);
void update_status_with_relative_path();
void update_path_bar();
void update_advanced_status_bar();
void ui_refresh_terminal_paths(const char *path);

#endif // UI_H
