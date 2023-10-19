#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <strings.h>
#include <termios.h>
#include <stdbool.h>
#include "data_link.h"
#include "alarm.h"
#include "state_machine.h"
#include "macros.h"


static struct termios old_terminal;
static struct LinkLayerData link_layer_data;
static uint8_t* aux_buffer;
static bool alarm_called = false;

int setup_terminal(int fd);
int restore_terminal(int fd);

int assemble_supervision_frame(FrameType frame_type, ConnectionType connection_type, uint8_t* frame_buffer);
uint32_t assemble_information_frame(uint8_t* packet_data, size_t packet_size, uint8_t* frame, uint8_t frame_number);
int write_buffer(int fd, uint8_t* buffer, size_t length, FrameType expected_response);
bool validBCC2(uint8_t bcc2, uint8_t* buffer, size_t buffer_size);
uint8_t compute_bcc2(uint8_t* buffer, size_t buffer_size);

int establish_transmitter_connection(int fd);
int wait_transmitter_connection(int fd);
int disconnect_transmitter(int fd);
int disconnect_receiver(int fd);

void dl_alarm_callback() {
    alarm_called = true;
}

int llopen(char* port, ConnectionType role) {

    int fd = 0;
    alarm_called = false;

    // Configure port
    snprintf(link_layer_data.port, sizeof(char)*MAX_PORT_SIZE, "%s", port);
    link_layer_data.role = role;
    link_layer_data.baud_rate = BAUDRATE;
    link_layer_data.num_transmissions = NUM_TX;
    link_layer_data.timeout = TIMEOUT;

    if ((link_layer_data.frame = calloc(MAX_FRAME_SIZE, sizeof(uint8_t))) == NULL) {
        return -1;
    }
    if ((aux_buffer = calloc(MAX_FRAME_SIZE + 1, sizeof(uint8_t))) == NULL) {
        return -1;
    }

    fd = open(link_layer_data.port, O_RDWR | O_NOCTTY );
    if (fd < 0) {
        perror("Error in data_link - llopen - opening port");
        return -1;
    } 

    if (setup_terminal(fd) == -1) {
        perror("Error in data_link - llopen");
        return -1;
    }

    if (setup_alarm_handler() == -1) {
        fprintf(stderr, "Error in data_link - llopen: setting up alarm handler\n");
        return -1;
    }

    if (subscribe_alarm(dl_alarm_callback) == -1) { // Subscribing a specific function to be called in alarm
        fprintf(stderr, "Error in data_link - llopen: subscribing alarm event\n");
        return -1;
    }

    switch(role) {
        case TRANSMITTER: {
            if ((establish_transmitter_connection(fd)) == -1) return -1;
            break;
        }
        case RECEIVER: {
            if ((wait_transmitter_connection(fd)) == -1) return -1;
            break;
        }
        default: {
            fprintf(stderr, "Error in data_link - llopen: tried to open a connection with invalid role\n");
            return -1;
        }
    }

    return fd;   
}

int llclose(int fd) {
    if (link_layer_data.role == TRANSMITTER) {
        disconnect_transmitter(fd);
    } else if (link_layer_data.role == RECEIVER) {
        disconnect_receiver(fd);
    }
    if (restore_terminal(fd) == -1) {
        perror("Error in data_link - llclose");
        return -1;
    }
    if (restore_alarm_handler() == -1) {
        fprintf(stderr, "Error in data_link - llclose: restoring alarm handlers\n");
        return -1;
    }
    free(aux_buffer);
    free(link_layer_data.frame);
    return close(fd);
}

int llwrite(int fd, uint8_t* packet, size_t packet_length) {
    static uint8_t package_to_send = 0;
    uint8_t frame_length;
    FrameType expected_response;

    // Numero de sequência
    if (package_to_send == 0) {
        expected_response = RR_1;
        frame_length = assemble_information_frame(packet, packet_length, link_layer_data.frame, 0);
    } else {
        expected_response = RR_0;
        frame_length = assemble_information_frame(packet, packet_length, link_layer_data.frame, 1);
    }

    // Writing
    int res = write_buffer(fd, link_layer_data.frame, frame_length, expected_response);
    if ( res == -2) {
        fprintf(stderr, "Error in data_link - llwrite: no. tries exceeded\n");
        return -1;
    } else if ( res == -1) {
        perror("Error in data_link - llwrite");
        return -1;
    } else if ( res != frame_length) {
        fprintf(stderr, "Error in data_link - llwrite: no. bytes written not matching expected\n");
        return -1;
    }
    package_to_send = package_to_send ? 0 : 1;
    return res;
}

/*
    This function reads into a buffer a packet that was read from the serial port
    fd -> Serial port file descriptor
    buffer -> Buffer that will receive the packet
    max_buffer_size -> Max size of buffer variable, used to check if a packet can fit into the provided buffer
*/
int llread(int fd, uint8_t* buffer, size_t max_buffer_size) {
    if (max_buffer_size < MAX_PACKET_SIZE) {

        fprintf(stderr, "Error in data_link - llread: provided buffer does not meet size requirements\n");
        return -1;
    }

    size_t frame_size;
    static uint8_t expected_package;
    uint8_t response_frame[SUPV_FRAME_SIZE];
    StateMachineResult new_frame;
    bool valid_frame = false;

    while(!valid_frame) {

        new_frame = read_frame(fd, link_layer_data.frame, MAX_FRAME_SIZE);
        frame_size = destuff(link_layer_data.frame, new_frame.packet_size, aux_buffer, MAX_PACKET_SIZE + 1);

        if (frame_size == 0) {
            fprintf(stderr, "Error in data_link - llread: destuffing packet unsuccessfull\n");
            return -1;
        }
        uint8_t bcc2 = aux_buffer[--frame_size];

        // Error
        if (new_frame.frame_type != expected_package || !validBCC2(bcc2, aux_buffer, frame_size)) {
            if (assemble_supervision_frame(expected_package ? REJ_1 : REJ_0, RECEIVER, response_frame) == -1) return -1;
        } else {
            memcpy(buffer, aux_buffer, frame_size);
            if (assemble_supervision_frame(expected_package ? RR_0 : RR_1, RECEIVER, response_frame) == -1) return -1;
            valid_frame = true;
        }
        if (write(fd, response_frame, SUPV_FRAME_SIZE) != 5) {
            fprintf(stderr, "Error in data_link - llread: did not write response frame correctly");
            return -1;
        }
    }

    expected_package = expected_package ? 0 : 1;
    return frame_size;
}

int setup_terminal(int fd) {
    struct termios new_terminal;

    if ( tcgetattr(fd, &old_terminal) == -1) {
      return -1;
    }

    bzero(&new_terminal, sizeof(new_terminal));
    new_terminal.c_cflag = link_layer_data.baud_rate | CS8 | CLOCAL | CREAD;
    new_terminal.c_iflag = IGNPAR;
    new_terminal.c_oflag = 0;
    new_terminal.c_lflag = 0;

    new_terminal.c_cc[VTIME] = VTIME_VALUE;
    new_terminal.c_cc[VMIN] = VMIN_VALUE;

    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&new_terminal) == -1) {
      return -1;
    }
     
    return 0;
}

int restore_terminal(int fd) {
    if (tcsetattr(fd,TCSANOW,&old_terminal) == -1) {
      return -1;
    }
    return 0;
}

int assemble_supervision_frame(FrameType frame_type, ConnectionType connection_type, uint8_t* frame_buffer) {
    frame_buffer[0] = FLAG;
    switch (connection_type) {
        case TRANSMITTER: {
            frame_buffer[1] = 0x03;
            if (frame_type == UA) {
                frame_buffer[1] = 0x01;
            } 
            if (frame_type == SET || frame_type == DISC || frame_type == UA) {
                frame_buffer[2] = frame_type;
            } else {
                fprintf(stderr, "Error: Data_link - assemble_supervision_frame - invalyd frame_type: %x\n", frame_type);
                return -1; //Invalid frame_type for this function
            }
            break;
        }
        case RECEIVER: {
            frame_buffer[1] = 0x03;
            if (frame_type == DISC) {
                frame_buffer[1] = 0x01;
            } 
            if (frame_type == UA || frame_type == DISC || frame_type == RR_0 || frame_type == RR_1 || frame_type == REJ_0 || frame_type == REJ_1) {
                frame_buffer[2] = frame_type;
            } else {
                fprintf(stderr, "Error: Data_link - assemble_frame - invalyd frame_type\n");
                return -1; //Invalid frame_type for this function
            }
            break;
        }
    }
    frame_buffer[3] = frame_buffer[1] ^ frame_buffer[2];
    frame_buffer[4] = FLAG;
    return 0;
}

uint32_t assemble_information_frame(uint8_t* packet_data, size_t packet_size, uint8_t* frame, uint8_t frame_number) {
    size_t frame_size = 4;
    frame[0] = FLAG;
    frame[1] = 0x03;
    frame[2] = frame_number;
    frame[3] = frame[1] ^ frame[2];
    frame_size = stuff(packet_data, packet_size, frame, frame_size);
    uint8_t bcc2[2];
    bcc2[0] = compute_bcc2(packet_data, packet_size);
    frame_size = stuff(bcc2, 1, frame, frame_size);
    frame[frame_size++] = FLAG;
    return frame_size;
}

uint32_t stuff(uint8_t* packet, size_t length, uint8_t* frame, size_t occupied_bytes) {
    size_t stuffed_length = occupied_bytes;

    int new_buffer_i = occupied_bytes;

    for (int i = 0; i < length; i++) {
        uint8_t byte = packet[i];
        if (byte == FLAG || byte == ESC_HEX) {
            frame[new_buffer_i++] = ESC_HEX;
            frame[new_buffer_i++] =  byte ^ STUFF_XOR_HEX;
            stuffed_length++;
        } else {
            frame[new_buffer_i++] = byte;
        }
        stuffed_length++;
    }
    return stuffed_length;
}

uint32_t destuff(uint8_t* stuffed_packet, size_t stuffed_length, uint8_t* unstuffed_packet, size_t max_unstuffed_packet_size) {
    size_t unstuffed_length = stuffed_length;

    int64_t new_buffer_i = 0;

    for (int i = 0; i < stuffed_length; i++) {
        uint8_t byte = stuffed_packet[i];
        if (new_buffer_i >= max_unstuffed_packet_size) {
            fprintf(stderr, "Error in data_link - destuffing: unstuffed packet is bigger than the destiny buffer! Tried to access %ld on a %ld bytes buffer\n", new_buffer_i, max_unstuffed_packet_size);
            return 0;
        }
        if (byte == ESC_HEX && i < stuffed_length - 1) {
            if (stuffed_packet[i + 1] == (FLAG ^ STUFF_XOR_HEX)) {
                unstuffed_packet[new_buffer_i++] = FLAG;
            } else if (stuffed_packet[i + 1] == (ESC_HEX ^ STUFF_XOR_HEX)) {
                unstuffed_packet[new_buffer_i++] = ESC_HEX;
            }
            unstuffed_length--;
            i++;
        } else {
            unstuffed_packet[new_buffer_i++] = byte;
        }
    }
    return unstuffed_length;
}

int establish_transmitter_connection(int fd) {
    uint8_t supervision_buffer[SUPV_FRAME_SIZE];
    
    if ((assemble_supervision_frame(SET, TRANSMITTER, supervision_buffer)) != 0) {
        return -1;
    }
    int res = write_buffer(fd, supervision_buffer, SUPV_FRAME_SIZE, UA);
    if (res == -2) {
        fprintf(stderr, "Error in data_link - establish_transmitter_connection: no. tries exceeded\n");
        return -1;
    } else if ( res == -1) {
        perror("Error in data_link - disconnect_receiver");
        return -1;
    } else if (res != 5) {
        fprintf(stderr, "Error in data_link - establish_transmitter_connection: no. bytes written not matching expected\n");
        return -1;
    }
    return 0;
}

int wait_transmitter_connection(int fd) {
    StateMachineResult res = read_frame(fd, link_layer_data.frame, MAX_FRAME_SIZE);
    if (res.frame_type == SET) {
        uint8_t supervision_buffer[SUPV_FRAME_SIZE];
        assemble_supervision_frame(UA, RECEIVER, supervision_buffer);
        if (write(fd, supervision_buffer, SUPV_FRAME_SIZE) != 5) {
            fprintf(stderr, "Error in data_link - wait_transmiiter_connection: could no write ACK to transmitter\n");
            return -1;
        }
        return 0;
    } else {
        fprintf(stderr, "Error in data_link - wait_transmiiter_connection: received wrong type of frame\n");
        return -1;
    }
}

int disconnect_transmitter(int fd) {
    uint8_t supervision_buffer[SUPV_FRAME_SIZE];
    assemble_supervision_frame(DISC, TRANSMITTER, supervision_buffer);
    int res = write_buffer(fd, supervision_buffer, SUPV_FRAME_SIZE, DISC); 
    if ( res == -2) {
        fprintf(stderr, "Error in data_link - disconnect_transmitter: no. tries exceeded\n");
        return -1;
    } else if ( res == -1) {
        perror("Error in data_link - disconnect_receiver");
        return -1;
    } else if ( res != 5) {
        fprintf(stderr, "Error in data_link - disconnect_transmitter: no. bytes written not matching expected\n");
        return -1;
    }
    assemble_supervision_frame(UA, TRANSMITTER, supervision_buffer);
    if (write(fd, supervision_buffer, SUPV_FRAME_SIZE) != 5) {
        return -1;
    }
    return 0;
}

int disconnect_receiver(int fd) {

    StateMachineResult res = read_frame(fd, link_layer_data.frame, MAX_FRAME_SIZE);
    if (res.frame_type == DISC) {
        uint8_t supervision_buffer[SUPV_FRAME_SIZE];
        assemble_supervision_frame(DISC, TRANSMITTER, supervision_buffer);
        int res = write_buffer(fd, supervision_buffer, SUPV_FRAME_SIZE, UA); 
        if ( res == -2) {
            fprintf(stderr, "Error in data_link - disconnect_receiver: no. tries exceeded\n");
            return -1;
        } else if ( res == -1) {
            perror("Error in data_link - disconnect_receiver");
            return -1;
        } else if ( res != 5) {
            fprintf(stderr, "Error in data_link - disconnect_receiver: no. bytes written not matching expected\n");
            return -1;
        }
        return 0;
    } else {
        fprintf(stderr, "Error in data_link - disconnect_receiver: unexpected frame type\n");
        return -1;
    }
}

/* Writes a buffer to the serial port and awaits a certain response
    fd -> Serial port file desciptor
    buffer -> buffer to write
    length -> length of the buffer to be written
    expected_response -> expected response sent by the other end of the serial port
*/
int write_buffer(int fd, uint8_t* buffer, size_t length, FrameType expected_response) {
    StateMachineResult res_frame = {};
    int tries = NUM_TX;
    int res = 0;
    while (tries > 0) {
        if((res = write(fd, buffer, length * sizeof(uint8_t))) == -1) return -1; // Writes buffer to serial port
        alarm(3);
        res_frame = read_frame(fd, link_layer_data.frame, MAX_FRAME_SIZE); // Reads response
        if (alarm_called) { // Timeout
            tries--;
            alarm_called = !alarm_called;
            fprintf(stderr, "Alarm called, response timed out, %d tries left\n", tries);
        } else {
            alarm(0);
            if (res_frame.frame_type == expected_response) {
                //Only break if frame received and data was correct
                break;
            }
        }
    }
    if (tries <= 0) { // Timeout limit 
        return -2;
    } else {
        return res;
    }
}

bool validBCC2(uint8_t bcc2, uint8_t* buffer, size_t buffer_size) {
    uint8_t side_bcc2 = compute_bcc2(buffer, buffer_size);
    return ((side_bcc2 ^ bcc2) == 0);
}

uint8_t compute_bcc2(uint8_t* buffer, size_t buffer_size) {
    uint8_t bcc2 = buffer[0];
    for (int i = 1; i < buffer_size; i++) {
        bcc2 ^= buffer[i];
    }
    return bcc2;
}
