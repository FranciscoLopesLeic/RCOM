#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define FLAG 0x7E
#define TX_ADDRESS 0x03
#define RX_ADDRESS 0x01
#define SET_CONTROL 0x03
#define UA_CONTROL 0x07
#define RR_CONTROL(n) ((n == 0) ? 0x05 : 0x85)
#define REJ_CONTROL(n) ((n == 0) ? 0x01 : 0x81)
#define DISC_CONTROL 0x0B
#define I_CONTROL(n) ((n == 0) ? 0x00 : 0x40)
#define ESC 0x7D
#define STUFF_XOR 0x20

#define DATA_SIZE 1024
#define STUFFED_DATA_SIZE (DATA_SIZE * 2 + 2)

#define CONTROL_DATA 0x01
#define CONTROL_START 0x02
#define CONTROL_END 0x03

#define FILE_SIZE 0x00
#define FILE_NAME 0x01

#define OCTET_MULT 256
#define MAX_PACKET_SIZE 1024

typedef enum
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP,
} LinkLayerState;

typedef struct
{
    uint8_t buffer[STUFFED_DATA_SIZE + 5];
    size_t length;
} DataHolder;

typedef struct
{
    int timeout;
    int num_retransmissions;
    volatile int count;
} AlarmConfigOptions;

typedef struct
{
    int fd;
    struct termios oldtio, newtio;
} TXUser;

typedef struct
{
    int fd;
    struct termios oldtio, newtio;
} RXUser;