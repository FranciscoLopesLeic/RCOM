#include <stdio.h>
#include "ftp.h"
#include "url_path_parser.h"

URLPathData resource_location;

void print_usage() {
    printf("Usage: download resource_location\n");
}

int main(int argc, char* argv[]) {
    setbuf(stdout, NULL);
    if (argc == 2) {
        if (create_url_data(argv[1], &resource_location) != 0) {
            return -1;
        }
        return ftp_download(&resource_location);
    } else {    
        print_usage();
        return -1;
    }
    return 0;
}