#pragma once

#include "url_path_parser.h"

int f_retrieve_fd(URLPathData* url_data);
unsigned long read_file_size(char* buffer);
void print_progress(unsigned long progress, unsigned long total);