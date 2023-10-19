#pragma once

#include <stdlib.h>
#include <stdint.h>

typedef enum {SET=0x03, UA=0x07, I_0 = 0x00, I_1 = 0x01, RR_0=0x05, RR_1=0x85, DISC=0x0D, ERROR, REJ_0=0x01, REJ_1=0x81, NONE} FrameType;

typedef struct {
    FrameType frame_type;
    size_t packet_size;
} StateMachineResult;

StateMachineResult read_frame(int fd, uint8_t* frame, size_t frame_size); 
