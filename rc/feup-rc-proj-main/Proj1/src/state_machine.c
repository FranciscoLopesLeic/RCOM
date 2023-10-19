#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "error.h"
#include "macros.h"
#include "alarm.h"
#include "state_machine.h"

enum StateType_ {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, DATA, STOP};
typedef enum StateType_ StateType;

/*
    Functions declared here because they are private
*/
bool isA(uint8_t a);
bool isC(uint8_t c);

// State machine to read the serial port and check the input validity
StateMachineResult read_frame(int fd, uint8_t* frame, size_t frame_size) {
    StateMachineResult state_machine_res  = {
        .frame_type = ERROR,
        .packet_size = 0,
    };
    uint8_t byte_rcvd, a, c;
    StateType state = START;
    a = byte_rcvd = c = 0;

    while (state != STOP) {
        if(read(fd, &byte_rcvd, sizeof(uint8_t)) == -1) {
            return state_machine_res;
        }
        switch (state) {
            case START: {
                if (byte_rcvd == FLAG) {
                    state = FLAG_RCV;
                }
                break;
            }
            case FLAG_RCV: {
                if (isA(byte_rcvd)) {
                    a = byte_rcvd;
                    state = A_RCV;
                } else if (byte_rcvd != FLAG) {
                    state = START;
                }
                break;
            }
            case A_RCV: {
                if (isC(byte_rcvd)) {
                    c = byte_rcvd;
                    state = C_RCV;
                } else if (byte_rcvd == FLAG) {
                    state = FLAG_RCV;
                } else {
                    state = START;
                }
                break;
            }
            case C_RCV: {
                if ((c == I_0 || c == I_1) && should_corrupt_header()) {
                    corrupt_header(&a, &c, &byte_rcvd);
                }
                if ((a ^ c ^ byte_rcvd) == 0) {
                    state = BCC_OK;
                } else if (byte_rcvd == FLAG) {
                    state = FLAG_RCV;
                } else {
                    state = START;
                }
                break;
            }
            case BCC_OK: {
                if (byte_rcvd == FLAG) {
                    state = STOP;
                } else {
                    state_machine_res.packet_size++;
                    frame[0] = byte_rcvd;
                    state = DATA;
                }
                break;
            }
            case DATA: {
                if (byte_rcvd == FLAG) {
                    state = STOP;
                } else {
                    state_machine_res.packet_size++;
                    if (state_machine_res.packet_size == frame_size) {
                        fprintf(stderr, "Error in data_link - state_machine: receiving more bytes than the max frame size\n");
                        return state_machine_res;
                    } 
                    frame[state_machine_res.packet_size - 1] = byte_rcvd;
                }
                break;
            }
            case STOP:
                break;
        }
    }
    state_machine_res.frame_type = c;

    // Random errors
    if ((state_machine_res.frame_type == I_0 || state_machine_res.frame_type == I_1) && should_corrupt_data()) {
        corrupt_data_buffer(frame, state_machine_res.packet_size);
    }

    delay(); // T_PROP

    return state_machine_res;
}

bool isA(uint8_t a) {
    return (a == 0x03) || (a == 0x01);
}

bool isC(uint8_t c) {
    c &= 0x0F; // Removes useless package data
    return (c == SET) || (c == DISC) || (c == UA) || (c == RR_0) || (c == REJ_0) || (c == I_0) || (c == I_1);
}




