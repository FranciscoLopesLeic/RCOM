#pragma once

#include <stdint.h>
#include "url_path_parser.h"

typedef struct {
    char ip[17];
    uint16_t port;
} DataConData;

int ftp_download(URLPathData* path_data);
