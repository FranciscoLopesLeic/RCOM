#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "file.h"
#include "data_link.h"
#include "application.h"

typedef struct {
    int file_descriptor;
    size_t transfer_file_size;
    ConnectionType status;
    char* transfer_file_name;
    char* transfer_file_dir;
    char* transfer_file_path;
} ApplicationLayer;

ApplicationLayer app_data;

int receiver(char* port_path);
int transmitter(char* port_path);

// RECEIVER AUX
size_t write_data_packet(uint8_t* data_packet, int fd, int* packet_no);
void parse_control_packet(uint8_t* packet);
int open_transfer_file();

// TRANSMITTER AUX
uint8_t* build_control_packet(size_t* control_packet_size, char* file_name);
size_t read_data_packet(int packetNo, uint8_t* data_packet, int fd, int bytes_read);
int read_file(char* file_path, uint8_t* buffer, size_t size);


void init_app(ConnectionType connection_type, char* file_path, char* serial_path) {
    app_data.transfer_file_name = calloc(FILE_NAME_SIZE, sizeof(char));
    app_data.transfer_file_dir = calloc(DIR_PATH_SIZE, sizeof(char));
    app_data.status = connection_type;
    
    path_parser(file_path, app_data.transfer_file_dir, app_data.transfer_file_name);

    if (connection_type == TRANSMITTER) {
        app_data.transfer_file_path = file_path;
        transmitter(serial_path);
    }
    if (connection_type == RECEIVER) {
        app_data.transfer_file_path = calloc(DIR_PATH_SIZE + FILE_NAME_SIZE, sizeof(char));
        receiver(serial_path);
        free(app_data.transfer_file_path);
    }

    free(app_data.transfer_file_dir);
    free(app_data.transfer_file_name);
}

void parse_control_packet(uint8_t* packet) {
    int i = 1;
    if (packet[i++] == 0x00) { // Filesize
        size_t file_size = 0;
        int n_bytes = (int) packet[i++];
        n_bytes--;
        while(n_bytes >= 0) {
            file_size += (size_t) packet[i++] << (n_bytes * 8);
            n_bytes--;
        }
        app_data.transfer_file_size = file_size;
    } else {
        fprintf(stderr, "Error in application - parse_control_packet: unexpected structure of the control_packet\n");
        exit(1);
    }
    if (packet[i++] == 0x01) { // Filename
        int n_bytes = (int) packet[i++];
        if (strlen(app_data.transfer_file_name) == 0) {
            strncpy(app_data.transfer_file_name, (char*) packet + i, n_bytes);
        }
        strncpy(app_data.transfer_file_path, app_data.transfer_file_dir, DIR_PATH_SIZE);
        strcat(app_data.transfer_file_path, app_data.transfer_file_name);
    } else {
        fprintf(stderr, "Error in application - parse_control_packet: unexpected structure of the control_packet\n");
        exit(1);
    }
}

size_t write_data_packet(uint8_t* data_packet, int fd, int* packet_no) {

    size_t packet_size = (size_t) (data_packet[2] << 8) + data_packet[3];
    if (*packet_no == (int) data_packet[1]) {
        (*packet_no)++;
    } else {
        fprintf(stderr, "Packet sequence number incorrect! Missed a packet\n");
        (*packet_no) = (int) data_packet[1] + 1;
    }

    if ((write(fd, data_packet + 4, packet_size * sizeof(uint8_t))) != packet_size) {
        perror("Error in application - write_data_packet");
        exit(1);
    }
    return packet_size;
}

int receiver(char* port_path) {

    int newFileDescriptor, packet_no = 0;
    size_t bytes_received = 0;
    bool fileOpen = false;
    uint8_t *data_packet;
    data_packet = calloc(MAX_PACKET_SIZE, sizeof(uint8_t));

    // OPEN
    if((app_data.file_descriptor = llopen(port_path, RECEIVER)) == -1) exit(1);

    printf("Receiver - connection established\n");
    
    while (true) {
        // READ
        if (llread(app_data.file_descriptor, data_packet, MAX_PACKET_SIZE) == -1) exit(1);
        if (data_packet[0] == 0x01) {
            bytes_received += write_data_packet(data_packet, newFileDescriptor, &packet_no);
            printf("Receiver - packet %d received\n", (int) data_packet[1]);
        } else if (data_packet[0] == 0x02) {
            parse_control_packet(data_packet);
            if (file_exists(app_data.transfer_file_dir, app_data.transfer_file_name)) {
                fprintf(stderr, "Error in application - receiver: file already exists\n");
                exit(1);
            }
            newFileDescriptor = open_transfer_file();
            fileOpen = true;

        } else if (data_packet[0] == 0x03) {
            // CLOSE
            if ((llclose(app_data.file_descriptor)) != 0) exit(1);
            printf("Receiver - connection cut\n");
            break;
        } else {
            fprintf(stderr, "Error in application - receiver: C byte value invalid\n");
            exit(1);
        }
    }

    if (bytes_received != app_data.transfer_file_size) {
        fprintf(stderr, "File size does not correspond to the expected\n");
    }

    if (fileOpen) {
        if ((close(newFileDescriptor) == -1)) {
            perror("Error in application - receiver");
            exit(1);
        }
    }
    free(data_packet);
    
    return 0;
}

size_t read_data_packet(int packetNo, uint8_t* data_packet, int fd, int bytes_read) {
    size_t bytes_to_write = (MAX_PACKET_SIZE > (app_data.transfer_file_size - bytes_read + 4)) ? (app_data.transfer_file_size - bytes_read) : MAX_PACKET_SIZE - 4; // In case of the file being in its end
    data_packet[0] = 0x01;
    data_packet[1] = (uint8_t) packetNo;
    data_packet[2] = (uint8_t) (bytes_to_write & 0xFF00) >> 8; // Amount of data
    data_packet[3] = (uint8_t) bytes_to_write & 0x00FF;
    if ((read(fd, data_packet + 4, bytes_to_write * sizeof(uint8_t))) != bytes_to_write) {
        perror("Error in application - read_data_packet");
        exit(1);
    }
    return bytes_to_write + 4;
}

uint8_t* build_control_packet(size_t* control_packet_size, char* file_name) {
    size_t file_name_size = strlen(file_name) + 1; //To actually copy \0

    if (file_name_size > MAX_PACKET_SIZE - 13) {
        fprintf(stderr, "Error in application - build_control_packet: filename too big\n");
        exit(1);
    }

    *control_packet_size = file_name_size + 13;
    uint8_t* buffer = calloc((*control_packet_size), sizeof(uint8_t));

    buffer[0] = 0x02; // C
    buffer[1] = 0; // T
    buffer[2] = (uint8_t) sizeof(app_data.transfer_file_size); // L
    buffer[3] = (app_data.transfer_file_size & 0xFF00000000000000) >> 56; // filesize
    buffer[4] = (app_data.transfer_file_size & 0x00FF000000000000) >> 48;
    buffer[5] = (app_data.transfer_file_size & 0x0000FF0000000000) >> 40;
    buffer[6] = (app_data.transfer_file_size & 0x000000FF00000000) >> 32;
    buffer[7] = (app_data.transfer_file_size & 0x00000000FF000000) >> 24;
    buffer[8] = (app_data.transfer_file_size & 0x0000000000FF0000) >> 16;
    buffer[9] = (app_data.transfer_file_size & 0x000000000000FF00) >> 8;
    buffer[10] = app_data.transfer_file_size & 0x00000000000000FF;
    buffer[11] = 1; // T
    buffer[12] = (uint8_t) (sizeof(char) * file_name_size); // L
    memcpy((char*) buffer + 13, file_name, file_name_size); // filename
    return buffer;
}

int transmitter(char* port_path) {
    uint8_t *control_packet, *data_packet;
    struct stat s;
    size_t control_packet_size, bytes_read = 0, bytes_written;
    int packet_no = 0, oldFileDescriptor;

    if (stat(app_data.transfer_file_path, &s) == -1) {
        perror("Error in application - transmitter - stat");
        exit(1);
    }
    app_data.transfer_file_size = s.st_size;
    data_packet = calloc(MAX_PACKET_SIZE, sizeof(uint8_t));

    oldFileDescriptor = open_transfer_file();

    // OPEN
    if( (app_data.file_descriptor = llopen(port_path, TRANSMITTER)) == -1) exit(1);
    control_packet = build_control_packet(&control_packet_size, app_data.transfer_file_name);

    printf("Transmitter - connection established\n");

    // START
    if ((llwrite(app_data.file_descriptor, control_packet, control_packet_size)) == -1) exit(1); 

    printf("Transmitter - transmition begun\n");

    // WRITING
    while (bytes_read < app_data.transfer_file_size) {
        packet_no++;
        bytes_written = read_data_packet(packet_no, data_packet, oldFileDescriptor, bytes_read);
        bytes_read += bytes_written - 4;
        if ((llwrite(app_data.file_descriptor, data_packet, bytes_written)) == -1) exit(1);
        printf("Transmitter - packet %d sent\n", packet_no);
    }

    // FINISH
    control_packet[0] = 0x03;
    if ((llwrite(app_data.file_descriptor, control_packet, control_packet_size)) == -1) {
        fprintf(stderr, "Error in application - transmitter - fatal error when writting control packet\n");
        exit(1);
    } 

    printf("Transmitter - transmition ended\n");

    // CLOSE
    if ((llclose(app_data.file_descriptor)) != 0) exit(1);

    printf("Transmitter - connection cut\n");

    if ((close(oldFileDescriptor) == -1)) {
        perror("Error in application - transmitter");
        exit(1);
    }

    free(data_packet);
    free(control_packet);  

    return 0;
}

int open_transfer_file() {
    int fd;

    switch (app_data.status) {
        case TRANSMITTER: {
            if ((fd = open(app_data.transfer_file_path, O_RDONLY)) == -1) {
                perror("Error in application - transmitter - open_transfer_file");
                exit(1);
            }
            break;
        }
        case RECEIVER: {
            if ((fd = creat(app_data.transfer_file_path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
                perror("Error in application - receiver - open_transfer_file");
                exit(1);
            }
            break;
        }

        default: {
            fprintf(stderr, "Error in application - open_transfer_file: wrong type of connection\n");
            exit(1);
        }
    }
    return fd;
}