#pragma once

#include <stdbool.h>

#define DIR_PATH_SIZE (4096 - FILE_NAME_SIZE)
#define FILE_NAME_SIZE 255

int path_parser(char* path, char* dir, char* file_name);
bool file_exists(char* dir, char* file_name);
