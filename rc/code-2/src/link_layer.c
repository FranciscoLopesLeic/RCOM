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

    if (connectionParameters.role == LlTx)
    {
        alarmCounter = 0;
        printf("Emissor\n");
        printf("New termios structure set\n");
        enum setState state;
        unsigned char b;
        unsigned char set[5];
        set[0] = FLAG_SET;
        set[1] = A;
        set[2] = C_SET;
        set[3] = BCC_SET;
        set[4] = FLAG_SET;

                while(alarmCounter < tries){
            state = START;
            int bytes;
            if(setAlarm == FALSE){
                bytes = write(fd, set, 5);
                alarm(timeout); // 3s para escrever
                setAlarm = TRUE;
                if (bytes < 0){
                    printf("Failed to send SET\n");
                }
                else{
                    printf("Sending: %x,%x,%x,%x,%x\n", set[0], set[1], set[2], set[3], set[4]);

                    printf("Sent SET FRAME\n");
 
                }
            }

            while (state != STOP)
            {
                int b_rcv = read(fd, &b, 1);
                if(b_rcv <= 0){
                    break;
                }
                state = stateMachineUA(b, state);
            }
            
           if (state == STOP){
                break;
            }
                  
        }

        if (alarmCounter >= connectionParameters.nRetransmissions){
            printf("Error UA\n");
            return -1;
        } 
        else printf("Received UA successfully\n");
    }

    else if (connectionParameters.role == LlRx)
    {
        printf("I am the Receptor\n");

        enum setState stateR = START;
        unsigned char c;

        while (stateR != STOP)
        {
            int b_rcv = read(fd, &c, 1);
            if (b_rcv > 0)
            {
                printf("Receiving: %x\n", c);

                stateR = stateMachineSET(c, stateR);
            }
        }

        printf("Received SET FRAME\n");

        unsigned char UA[5];
        UA[0] = FLAG_SET;
        UA[1] = A;
        UA[2] = C_UA;
        UA[3] = BCC_UA;
        UA[4] = FLAG_SET;
        int bytesReceptor = write(fd, UA, 5);
        if (bytesReceptor < 0){
            printf("Failed to send UA\n");
        }
        else {
            printf("Sending: %x,%x,%x,%x,%x\n", UA[0], UA[1], UA[2], UA[3], UA[4]);
            printf("Sent UA FRAME\n");
        }

    }
    else {
        printf("Unknown role!\n");
        exit(1);
    }
    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    alarmCounter = 0;

    char bcc2 = 0x00;
    for (int i = 0; i < bufSize; i++) {
        bcc2 = bcc2 ^ buf[i];
    }
    unsigned char infoFrame[1000] = {0};

    infoFrame[0] = FLAG_SET;
    infoFrame[1] = A;
    infoFrame[2] = (flag << 6); // control
    infoFrame[3] = A ^ (flag << 6);

    //byte stuffing
    int index = 4;
    for(int i = 0; i < bufSize; i++) {
        if (buf[i] == 0x7E) {
            infoFrame[index] = 0x7D;
            index++;
            infoFrame[index] = 0x5E;
            index++;
        }
        else if (buf[i] == 0x7D) {
            infoFrame[index] = 0x7D;
            index++;
            infoFrame[index] = 0x5D;
            index++;
        }
        else {
            infoFrame[index] = buf[i];
            index++;
        }
    }

    // Byte stuffing of the bcc2
    if (bcc2 == 0x7E) {
        infoFrame[index] = 0x7D;
        index++;
        infoFrame[index] = 0x5E;
        index++;
    }
    else if (bcc2 == 0x7D) {
        infoFrame[index] = 0x7D;
        index++;
        infoFrame[index] = 0x5D;
        index++;
    }
    else {
        infoFrame[index] = bcc2;
        index++;
    }
    infoFrame[index] = FLAG_SET;
    index++;


    int STOP = FALSE;
    unsigned char rcv[5];
    alarmCounter = 0;
    setAlarm = FALSE;

    while(alarmCounter < tries){
        if(setAlarm == FALSE){
            write(fd, infoFrame, index);
            //usleep(50*1000);
            printf("\nInfo Frame sent Ns = %d\n", flag);
            alarm(timeout);
            setAlarm = TRUE;
        }

        int response = read(fd, rcv, 5);

        if(response > 0){
            if(rcv[2] != (!flag << 7 | 0x05)){
                printf("\nREJ Received\n");
                setAlarm = FALSE;
                continue;
            }
            else if(rcv[3] != (rcv[1]^rcv[2])){
                printf("\nRR not correct\n");
                setAlarm = FALSE;
                continue;
            }
            else{
                printf("\nRR correctly received\n");
                break;
            }
        }
    }
    if(alarmCounter >= tries){
        printf("TIME-OUT\n");
        return -1;
    }
    prevTries = flag;
    if(flag) flag = 0;
    else flag = 1;

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
