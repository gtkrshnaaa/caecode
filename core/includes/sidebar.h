#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "app_state.h"

void init_sidebar();
void open_folder(const char *path);
void reload_sidebar();
void close_folder();
void collapse_all_folders();
void update_git_status();
const char* get_git_status_letter(const char *path);

#endif // SIDEBAR_H
