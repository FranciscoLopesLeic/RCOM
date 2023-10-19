#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>
#include "utils.h"



int f_retrieve_fd(URLPathData* url_data) {
    if (url_data == NULL) {
        fprintf(stderr, "Error: Invalid path!\n");
        return -1;
    }
    char* file_name = strrchr(url_data->url, '/') + 1 ?: url_data->url;
    if (access(file_name, F_OK) != -1) {
        fprintf(stderr, "Missing permissions or file already exists\n");
        return -1;
    }
    return creat(file_name, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
}


// Reads file size from server message
unsigned long read_file_size(char* buffer) {
    int i = 0;
    char* file_size_str;
    while(buffer[i] != 40) i++;

    i++;
    file_size_str = buffer + i;
    while(buffer[i] != 32) i++;

    buffer[i] = '\0';

    return atoll(file_size_str);
}

// Prints progress message
void print_progress(const unsigned long progress, const unsigned long total) {
    if (total == 0) {
        fprintf(stderr, "\rDownload file size not defined\n");
        return;
    }
    double percentage = ((double) progress) / ((double) total) * 100;
    printf("\rDownloaded %lf%%                     ", percentage);
}