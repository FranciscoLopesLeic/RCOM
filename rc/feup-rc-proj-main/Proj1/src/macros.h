#pragma once

typedef enum { TRANSMITTER, RECEIVER } ConnectionType;

#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FLAG 0x7E
#define BAUDRATE B38400
#define NUM_TX 3 //Number of tries
#define VMIN_VALUE 5
#define VTIME_VALUE 0
#define TIMEOUT 2 //Timeout in seconds
#define FALSE 0
#define TRUE 1
#define MAX_PACKET_SIZE 150
#define MAX_PORT_SIZE 20
#define SUPV_FRAME_SIZE 5
#define INF_DATA_LAYER_SIZE 6
#define STUFF_XOR_HEX 0x20
#define ESC_HEX 0x7d
