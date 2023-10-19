#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include "macros.h"
#include "ftp.h"
#include "utils.h"

static URLPathData* resource_location;
static int server_socket;
static int data_socket;
static FILE* server_socket_file;
static char* buffer;
unsigned long file_size = 0;
int ftp_control_connect(URLPathData* path_data);
int ftp_data_connect(DataConData* data_con);
void ftp_close();
int ftp_dump_data(int fd);
int ftp_read_resp_code();
int ftp_login();
int ftp_retrieve_passive_mode(DataConData* data_con_data);
int ftp_change_dir_to_res(URLPathData* url_data);


// Main function to execute download
int ftp_download(URLPathData* path_data) {
    int fd;
    DataConData con_data;

    if (ftp_control_connect(path_data) == -1) {
        return -1;
    }
    if (ftp_login() == -1){
        return -1;
    }
    if (ftp_retrieve_passive_mode(&con_data) == -1) {
        return -1;
    }
    if (ftp_data_connect(&con_data) == -1) {
        return -1;
    }
    if (ftp_change_dir_to_res(path_data) == -1) {
        return -1;
    }
    if((fd = f_retrieve_fd(path_data)) == -1) {
        return -1;
    }
    if (ftp_dump_data(fd) == -1) {
        return -1;
    }
    ftp_close();
    return 0;
}

// Connect to server
int ftp_control_connect(URLPathData* path_data) {
    resource_location = path_data;
    buffer = calloc(MAX_CTRL_SIZE, sizeof(char));
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *) resource_location->sv_addr))),
        .sin_port = htons(path_data->port),
    };
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("ftp_control_connect_socket");
        return -1;
    } 
    if (connect(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        perror("ftp_control_connect_connect");
        return -1;
    }
    if ((server_socket_file = fdopen(server_socket, "r")) == NULL) {
        perror("ftp_control_connect_fdopen");
        return -1;
    }
    if (ftp_read_resp_code() != 220) {
        fprintf(stderr, "Error: Server responded with unexpected code!\n");
        return -1;
    }
    return 0;
}

// Establishes data connection
int ftp_data_connect(DataConData* data_con) {
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = inet_addr(data_con->ip),
        .sin_port = htons(data_con->port),
    };
    if ((data_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
        perror("ftp_data_connect_socket");
        return -1;
    } 
    if ((connect(data_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) && (errno != EINPROGRESS)) {
        perror("ftp_data_connect_connect");
        return -1;
    }
    return 0;
}

// Close connection
void ftp_close() {
    close(server_socket);
    close(data_socket);
}

// Download
int ftp_dump_data(int fd) {
    int downloaded_bytes = 0;
    struct timeval tv;
    fd_set socket_set;
    uint8_t byte_stream[1024];
    ssize_t read_size;
    FD_ZERO(&socket_set);
    FD_SET(data_socket, &socket_set);
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    int select_retval = select(data_socket + 1, &socket_set, NULL, NULL, &tv);
    if (select_retval == -1) {
        perror("select");
        return -1;
    } else if (select_retval == 0) {
        fprintf(stderr, "Error: Couldn't establish connection with data link.\n");
        return -1;
    }
    printf("Starting download...\n");


    while((read_size = read(data_socket, byte_stream, 1024)) != 0) {
        write(fd, byte_stream, read_size);
        if (read_size > 0) downloaded_bytes += read_size;
        print_progress(downloaded_bytes, file_size);
    }
    printf("\n");
    write(fd, byte_stream, read_size);
    if (ftp_read_resp_code() != 226) {
        fprintf(stderr, "Error: Unexpected response after end of transfer!\n");
        return -1;
    }
    return 0;
}

// Gets server resonse
int ftp_read_resp_code() {
    do {
        memset(buffer, 0, MAX_CTRL_SIZE);
        fgets(buffer, MAX_CTRL_SIZE, server_socket_file);
    } while (buffer[0] < '1' || buffer[0] > '5' || buffer[3] != ' ');
    printf("%s", buffer);
    buffer[3] = '\0';
    int response = atoi(buffer);
    if (response == 150) file_size = read_file_size(buffer + 4); // To print the progress percentage
    return response;
}

// Login
int ftp_login() {

    // User
    if (resource_location->username != NULL) {
        strcpy(buffer, "user ");
        strcat(buffer, resource_location->username);
    } else {
        strcpy(buffer, "user anonymous");
    }
    strcat(buffer, "\r\n");
    write(server_socket, buffer, strlen(buffer));
    printf("%s", buffer);
    if (ftp_read_resp_code() != 331) {
        fprintf(stderr, "Error: Unexpected response after username was sent!\n");
        return -1;
    }

    // Password
    if (resource_location->password != NULL) {
        strcpy(buffer, "pass ");
        strcat(buffer, resource_location->password);
    } else {
        strcpy(buffer, "pass teste");
    }
    strcat(buffer, "\r\n");
    write(server_socket, buffer, strlen(buffer));
    printf("%s", buffer);
    if (ftp_read_resp_code() != 230) {
        fprintf(stderr, "Error: Unexpected response after password was sent!\n");
        return -1;
    }
    return 0;
}

// Request port for data connection (passive mode FTP)
int ftp_retrieve_passive_mode(DataConData* data_con_data) {
    unsigned ip[4];
    unsigned msb_port;
    unsigned lsb_port;
    strcpy(buffer, "pasv\r\n");
    write(server_socket, buffer, strlen(buffer));
    fscanf(server_socket_file, "227 Entering Passive Mode (%u,%u,%u,%u,%u,%u).", 
        &ip[0],
        &ip[1],
        &ip[2],
        &ip[3],
        &msb_port,
        &lsb_port
    );
    snprintf(data_con_data->ip, 100, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    data_con_data->port = msb_port * 256 + lsb_port;
    return 0;
}

// Changes the current directory to the directory of the file to download
int ftp_change_dir_to_res(URLPathData* url_data) {
    size_t url_size = strlen(url_data->url) + 1;
    char* url_cpy = calloc(url_size, sizeof(char));
    strncpy(url_cpy, url_data->url, url_size);

    char* last_token = strrchr(url_cpy, '/') + 1;
    char* token = strtok(url_cpy, "/");
    while(token != NULL) {
        if (token == last_token) {
            break;
        }
        strcpy(buffer, "CWD ");
        strcat(buffer, token);
        strcat(buffer, "\r\n");
        write(server_socket, buffer, strlen(buffer));
        if (ftp_read_resp_code() != 250) {
            fprintf(stderr, "Error: Invalid path to resource!\n");
            return -1;
        }
        token = strtok(NULL, "/");
    }
    strcpy(buffer, "RETR ");
    strcat(buffer, last_token);
    strcat(buffer, "\r\n");
    write(server_socket, buffer, strlen(buffer));
    if (ftp_read_resp_code() != 150) {
        fprintf(stderr, "Error: Unexpected response to RETR command!\n");
        return -1;
    }
    free(url_cpy);
    return 0;
}