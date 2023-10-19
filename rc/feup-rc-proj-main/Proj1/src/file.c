#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "file.h"

// Create Dir and Path for file
int path_parser(char* path, char* dir, char* file_name) {
    file_name[0] = '\0';
    dir[0] = '\0';

    char* last_dir_sep = strrchr(path, '/');
    char* cur_ptr = path;

    // Dir separation
    if (last_dir_sep == NULL) {
        strcpy(dir, "./");
        strcpy(file_name, path);
    } else {    
        char* cur_ptr = path;
        size_t dir_idx = 0;
        while (cur_ptr != last_dir_sep) {
            dir[dir_idx] = *cur_ptr;
            cur_ptr++;
            dir_idx++;
        }
        dir[dir_idx] = '/';
        dir[dir_idx + 1] = '\0';
        cur_ptr = ++last_dir_sep;
    }
    
    // Path separation
    size_t file_name_idx = 0;
    while (*cur_ptr != '\0') {
        file_name[file_name_idx] = *cur_ptr;
        cur_ptr++;
        file_name_idx++;
    }
    *cur_ptr = '\0';

    struct stat stat_buf;
    int status;
    if ((status = stat(dir, &stat_buf)) == -1) {
        perror("Error in path_parser");
        return -1;
    }
    if (S_ISDIR(stat_buf.st_mode)) {
        return 0;
    } else if (S_ISREG(stat_buf.st_mode)) {
        fprintf(stderr, "Error in path_parser - file already exists!\n");
        return -1;
    } else {
        fprintf(stderr, "Error in path_parser - nvalid file path\n");
        return -1;
    }
    
}

bool file_exists(char* dir, char* file_name) {
    char* file_path = calloc(DIR_PATH_SIZE + FILE_NAME_SIZE, sizeof(uint8_t));
    strncpy(file_path, dir, DIR_PATH_SIZE);
    strcat(file_path, file_name);

    struct stat stat_buf;

    if ((stat(file_path, &stat_buf) == -1) && errno == ENOENT) {
        free(file_path);
        return false;
    }
    free(file_path);
    return true;
}
