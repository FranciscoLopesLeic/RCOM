// Link layer protocol implementation

#include "link_layer.h"
#include "tools.h"
// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BufferSize 256

volatile int STOP = FALSE;

int setAlarm = FALSE;
int alarmCounter = 0;

int fd;
struct termios oldtio;
struct termios newtio;
int timeout, tries, prevTries = 1;

int flag = 0;
clock_t start, end;

void handler(int signal)
{
    setAlarm = FALSE;
    alarmCounter++;
}

enum set_state stateMachine(unsigned char b, enum set_state state)
{
    switch (state)
    {
    case START /* constant-expression */:
        if (b == FLAG_SET)
        {
            state = FLAG_RCV;
        }
        break;
    case FLAG_RCV:
        if (b == A)
        {
            state = A_RCV;
        }
        else if (b == FLAG_SET)
        {
            state = FLAG_RCV;
        }
        else
        {
            state = START;
        }
        break;
    case A_RCV:
        if (b == C_UA)
        {
            state = C_RCV;
        }
        else if (b == FLAG_SET)
        {
            state = FLAG_RCV;
        }
        else
        {
            state = START;
        }
        break;
    case C_RCV:
        if (b == BCC_UA)
        {
            state = BCC_OK;
        }
        else if (b == FLAG_SET)
        {
            state = FLAG_RCV;
        }
        else
        {
            state = START;
        }
        break;
    case BCC_OK:
        if (b == FLAG_SET)
        {
            state = STOP;
        }
        else
        {
            state = START;
        }
        break;
    case STOP:
        break;
    default:
        break;
    }
    return state;
}

enum set_state setStateMachine(unsigned char b, enum set_state state)
{
    switch (state)
    {
    case START /* constant-expression */:
        if (b == FLAG_SET)
        {
            state = FLAG_RCV;
        }
        break;
    case FLAG_RCV:
        if (b == A)
        {
            state = A_RCV;
        }
        else if (b == FLAG_SET)
        {
            state = FLAG_RCV;
        }
        else
        {
            state = START;
        }
        break;
    case A_RCV:
        if (b == C_SET)
        {
            state = C_RCV;
        }
        else if (b == FLAG_SET)
        {
            state = FLAG_RCV;
        }
        else
        {
            state = START;
        }
        break;
    case C_RCV:
        if (b == BCC_SET)
        {
            state = BCC_OK;
        }
        else if (b == FLAG_SET)
        {
            state = FLAG_RCV;
        }
        else
        {
            state = START;
        }
        break;
    case BCC_OK:
        if (b == FLAG_SET)
        {
            state = STOP;
        }
        else
        {
            state = START;
        }
        break;
    case STOP:
        break;
    default:
        break;
    }
    return state;
}

enum set_state stateMachineDisc(unsigned char b, enum set_state state){
    switch (state)
    {
    case START:
        if (b == FLAG_SET)
        {
            state = FLAG_RCV;
        }
        break;
    case FLAG_RCV:
        if (b == A)
        {
            state = A_RCV;
        }
        else if (b == A_RCV)
        {
            state = A_RCV;
        }
        else if (b == FLAG_SET)
        {
            state = FLAG_RCV;
        }
        else
        {
            state = START;
        }
        break;
    case A_RCV:
        if (b == C_DISC)
        {
            state = C_RCV;
        }
        else if (b == FLAG_SET)
        {
            state = FLAG_RCV;
        }
        else
        {
            state = START;
        }
        break;
    case C_RCV:
        if (b == BCC_SET)
        {
            state = BCC_OK;
        }
        else if (b == FLAG_SET)
        {
            state = FLAG_RCV;
        }
        else
        {
            state = START;
        }
        break;
    case BCC_OK:
        if (b == FLAG_SET)
        {
            state = STOP;
        }
        else
        {
            state = START;
        }
        break;
    default:
        break;
    }
}

#define _POSIX_SOURCE 1 

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    start = clock();
    int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
        return -1;
    }
    struct termios oldtio;
    struct termios newtio;
    if (tcgetattr(fd, &oldtio) == -1)
    { /* save current port settings */
        perror("tcgetattr");
        return -1;
    }
    memset(&newtio, 0, sizeof(newtio));
    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 1; /* inter-character timer unused */
    newtio.c_cc[VMIN] = 0;  /* blocking read until 5 chars received */
    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        return -1;
    }

    timeout = connectionParameters.timeout;
    tries = connectionParameters.nRetransmissions;
    printf("New termios structure set\n");

    

    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}
