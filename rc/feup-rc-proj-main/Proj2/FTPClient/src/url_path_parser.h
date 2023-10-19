#pragma once

#include <stdint.h>

typedef struct {
    char* username;
    char* password;
    char* url;
    char* sv_addr;
    uint16_t port;
} URLPathData;

int create_url_data(char* url_path, URLPathData* url_data);
char* strptrcpy(char* start, char* end);
void destroy_url_data(URLPathData data);