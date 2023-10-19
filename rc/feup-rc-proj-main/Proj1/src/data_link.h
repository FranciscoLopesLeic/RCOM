#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "macros.h"
#include "state_machine.h"

#define MAX_FRAME_SIZE ((MAX_PACKET_SIZE * 2) + 7)

struct LinkLayerData {
    ConnectionType role;
    char port[MAX_PORT_SIZE];
    unsigned int baud_rate;
    unsigned int sequence_number;
    unsigned int timeout;
    unsigned int num_transmissions;
    uint8_t* frame;
} LinkLayerData;

uint32_t stuff(uint8_t* packet, size_t length, uint8_t* frame, size_t occupied_bytes);
uint32_t destuff(uint8_t* stuffed_packet, size_t stuffed_length, uint8_t* unstuffed_packet, size_t max_unstuffed_packet_size);

int llopen(char* port, ConnectionType role);
int llwrite(int fd, uint8_t* buffer, size_t length);
int llread(int fd, uint8_t* buffer, size_t max_buffer_size);
int llclose(int fd);
