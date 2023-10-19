#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include "url_path_parser.h"

// Prints error message
void print_url_format_error() {
    fprintf(stderr, "Invalid URL format. It should respect the following convention: ftp://<user>:<password>@<host>:<port>/<url-path>\n");
}

// Checks it url is FTP
bool is_ftp(char* url_path) {
    if (strncmp(url_path, "ftp://", 6) == 0) {
        return true;
    }
    return false;
}

// Cheks if path has port
bool has_port(char* url_path) { 
    return (strchr(url_path, ':') != NULL);
}

char* strptrcpy(char* start, char* end) {
    size_t len;
    if (end == NULL) {
        len = strlen(start);
    } else {
        len = (size_t) (end - start);
    }
    char* dest = calloc(len + 1, sizeof(char));
    for(size_t i = 0; i < len && start != end; i++) {
        dest[i] = *start;
        start++;
    }
    dest[len] = '\0';
    return dest; 
}

// Parses the information from the url to a struct
int create_url_data(char* url_path, URLPathData* url_data) {
    char* server_name;
    struct hostent* sv_resesolv;
    url_data->port = 21;

    // Check if ftp
    if (!is_ftp(url_path)) {
        print_url_format_error();
        return -1;
    }

    // User and password
    url_path += strlen("ftp://"); //Removes protocol from url path
    char* at_place = strchr(url_path, '@');
    if (at_place != NULL) {
        char* user_pass_sep = strchr(url_path, ':');
        if (user_pass_sep == NULL || user_pass_sep > at_place) {
            print_url_format_error();
            return -1;
        }
        url_data->username = strptrcpy(url_path, user_pass_sep);
        url_data->password = strptrcpy(user_pass_sep + 1, at_place);
        url_path = at_place + 1;
    }

    char* resource_slash = strchr(url_path, '/');
    if(resource_slash == NULL) {
        print_url_format_error();
        return -1;
    }

    // Port
    if (has_port(url_path)) {
        char* port_tok = strchr(url_path, ':');
        if (port_tok == NULL) {
            print_url_format_error();
            return -1;
        }
        server_name = strptrcpy(url_path, port_tok);
        char* port_str = strptrcpy(port_tok + 1, resource_slash);
        url_data->port = (uint16_t)atoi(port_str);
        free(port_str);
    } else {
        server_name = strptrcpy(url_path, resource_slash);
    }


    // Url
    url_data->url = strptrcpy(resource_slash + 1, NULL);
    if ((sv_resesolv = gethostbyname(server_name)) == NULL) {
        herror("create_url_data");
        return -1;
    }
    
    // Server
    url_data->sv_addr = (char*) calloc(strlen(sv_resesolv->h_addr) + 1, sizeof(char));
    strcpy(url_data->sv_addr, sv_resesolv->h_addr);
    free(server_name);
    return 0;
}

void destroy_url_data(URLPathData data) {
    free(data.username);
    free(data.password);
    free(data.url);
    free(data.sv_addr);
}