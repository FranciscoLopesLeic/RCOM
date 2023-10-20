// Link layer protocol implementation

#include "link_layer.h"

#define SOURCE 1
#define RATE 38400

#define BUFFER_SIZE 256

#define FLAG_SET 0x7E
#define ESCAPE 0x7D
#define A_ER 0x03
#define A_RE 0x01
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x07
#define C_RR0(l) ((l << 7) | 0x05)
#define C_REJ0(l) ((l <<7) | 0x01)
#define C_N(l) (l << 6)

typedef enum
{
   START,
   FLAG,
   A_RCV,
   C_RCV,
   BCC1,
   STOP,
   DATA_ESCAPE,
   READING_DATA,
   DISCONNECTED,
   BCC2
} LlState;


// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source


volatile int STOP1 = FALSE;
int setAlarm = FALSE;
int alarmCounter = 0;
int timeout = 0;
int retransmitions = 0;
unsigned char tramaTx = 0;
unsigned char tramaRx = 1;


void handler(int signal) {
    setAlarm = TRUE;
    alarmCounter++;
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    
    LlState state = START;
    int fd = connection(connectionParameters.serialPort);
    if (fd < 0) return -1;

    unsigned char byte;
    timeout = connectionParameters.timeout;
    retransmitions = connectionParameters.nRetransmissions;
    switch (connectionParameters.role) {

        case LlTx: {

            (void) signal(SIGALRM, handler);
            while (connectionParameters.nRetransmissions != 0 && state != STOP) {
                
                sendSupervisionFrame(fd, A_ER, C_SET);
                alarm(connectionParameters.timeout);
                setAlarm = FALSE;
                
                while (setAlarm == FALSE && state != STOP) {
                    if (read(fd, &byte, 1) > 0) {
                        switch (state) {
                            case START:
                                if (byte == FLAG_SET) state = FLAG;
                                break;
                            case FLAG:
                                if (byte == A_RE) state = A_RCV;
                                else if (byte != FLAG_SET) state = START;
                                break;
                            case A_RCV:
                                if (byte == C_UA) state = C_RCV;
                                else if (byte == FLAG_SET) state = FLAG;
                                else state = START;
                                break;
                            case C_RCV:
                                if (byte == (A_RE ^ C_UA)) state = BCC1;
                                else if (byte == FLAG_SET) state = FLAG;
                                else state = START;
                                break;
                            case BCC1:
                                if (byte == FLAG_SET) state = STOP;
                                else state = START;
                                break;
                            default: 
                                break;
                        }
                    }
                } 
                connectionParameters.nRetransmissions--;
            }
            if (state != STOP) return -1;
            break;  
        }

        case LlRx: {

            while (state != STOP_R) {
                if (read(fd, &byte, 1) > 0) {
                    switch (state) {
                        case START:
                            if (byte == FLAG_SET) state = FLAG;
                            break;
                        case FLAG_RCV:
                            if (byte == A_ER) state = A_RCV;
                            else if (byte != FLAG_SET) state = START;
                            break;
                        case A_RCV:
                            if (byte == C_SET) state = C_RCV;
                            else if (byte == FLAG_SET) state = FLAG;
                            else state = START;
                            break;
                        case C_RCV:
                            if (byte == (A_ER ^ C_SET)) state = BCC1;
                            else if (byte == FLAG_SET) state = FLAG;
                            else state = START;
                            break;
                        case BCC1:
                            if (byte == FLAG_SET) state = STOP;
                            else state = START;
                            break;
                        default: 
                            break;
                    }
                }
            }  
            sendSupervisionFrame(fd, A_RE, C_UA);
            break; 
        }
        default:
            return -1;
            break;
    }
    return fd;
}

int connection(const char *serialPort) {

    int fd = open(serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(serialPort);
        return -1; 
    }

    struct termios oldtio;
    struct termios newtio;

    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = RATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0;
    newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);

    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        return -1;
    }

    return fd;
}


////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(int fd, const unsigned char *buf, int bufSize) {

    int frameSize = 6+bufSize;
    unsigned char *frame = (unsigned char *) malloc(frameSize);
    frame[0] = FLAG_SET;
    frame[1] = A_ER;
    frame[2] = C_N(tramaTx);
    frame[3] = frame[1] ^frame[2];
    memcpy(frame+4,buf, bufSize);
    unsigned char BCC3 = buf[0];
    for (unsigned int i = 1 ; i < bufSize ; i++) BCC3 ^= buf[i];

    int j = 4;
    for (unsigned int i = 0 ; i < bufSize ; i++) {
        if(buf[i] == FLAG_SET || buf[i] == ESCAPE) {
            frame = realloc(frame,++frameSize);
            frame[j++] = ESCAPE;
        }
        frame[j++] = buf[i];
    }
    frame[j++] = BCC3;
    frame[j++] = FLAG_SET;

    int currentTransmition = 0;
    int rejected = 0, accepted = 0;

    while (currentTransmition < retransmitions) { 
        setAlarm = FALSE;
        alarm(timeout);
        rejected = 0;
        accepted = 0;
        while (setAlarm == FALSE && !rejected && !accepted) {

            write(fd, frame, j);
            unsigned char result = readControlFrame(fd);
            
            if(!result){
                continue;
            }
            else if(result == C_REJ0(0) || result == C_REJ0(1)) {
                rejected = 1;
            }
            else if(result == C_RR0(0) || result == C_RR0(1)) {
                accepted = 1;
                tramaTx = (tramaTx+1) % 2;
            }
            else continue;

        }
        if (accepted) break;
        currentTransmition++;
    }
    
    free(frame);
    if(accepted) return frameSize;
    else{
        llclose(fd);
        return -1;
    }
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
unsigned char byte, cField;
    int i = 0;
    LlState state = START;

    while (state != STOP) {  
        if (read(fd, &byte, 1) > 0) {
            switch (state) {
                case START:
                    if (byte == FLAG_SET) state = FLAG;
                    break;
                case FLAG:
                    if (byte == A_ER) state = A_RCV;
                    else if (byte != FLAG_SET) state = START;
                    break;
                case A_RCV:
                    if (byte == C_N(0) || byte == C_N(1)){
                        state = C_RCV;
                        cField = byte;   
                    }
                    else if (byte == FLAG_SET) state = FLAG;
                    else if (byte == C_DISC) {
                        sendSupervisionFrame(fd, A_RE, C_DISC);
                        return 0;
                    }
                    else state = START;
                    break;
                case C_RCV:
                    if (byte == (A_ER ^ cField)) state = READING_DATA;
                    else if (byte == FLAG_SET) state = FLAG;
                    else state = START;
                    break;
                case READING_DATA:
                    if (byte == ESCAPE) state = DATA_ESCAPE;
                    else if (byte == FLAG_SET){
                        unsigned char bcc2 = packet[i-1];
                        i--;
                        packet[i] = '\0';
                        unsigned char acc = packet[0];

                        for (unsigned int j = 1; j < i; j++)
                            acc ^= packet[j];

                        if (bcc2 == acc){
                            state = STOP;
                            sendSupervisionFrame(fd, A_RE, C_RR0(tramaRx));
                            tramaRx = (tramaRx + 1)%2;
                            return i; 
                        }
                        else{
                            printf("Error: retransmition\n");
                            sendSupervisionFrame(fd, A_RE, C_REJ0(tramaRx));
                            return -1;
                        };

                    }
                    else{
                        packet[i++] = byte;
                    }
                    break;
                case DATA_ESCAPE:
                    state = READING_DATA;
                    if (byte == ESCAPE || byte == FLAG_SET) packet[i++] = byte;
                    else{
                        packet[i++] = ESCAPE;
                        packet[i++] = byte;
                    }
                    break;
                default: 
                    break;
            }
        }
    }
    return -1;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    LlState state = START;
    unsigned char byte;
    (void) signal(SIGALRM, handler);
    
    while (retransmitions != 0 && state != STOP) {
                
        sendSupervisionFrame(fd, A_ER, C_DISC);
        alarm(timeout);
        setAlarm = FALSE;
                
        while (setAlarm == FALSE && state != STOP) {
            if (read(fd, &byte, 1) > 0) {
                switch (state) {
                    case START:
                        if (byte == FLAG_SET) state = FLAG;
                        break;
                    case FLAG:
                        if (byte == A_RE) state = A_RCV;
                        else if (byte != FLAG_SET) state = START;
                        break;
                    case A_RCV:
                        if (byte == C_DISC) state = C_RCV;
                        else if (byte == FLAG_SET) state = FLAG;
                        else state = START;
                        break;
                    case C_RCV:
                        if (byte == (A_RE ^ C_DISC)) state = BCC1;
                        else if (byte == FLAG_SET) state = FLAG;
                        else state = START;
                        break;
                    case BCC1:
                        if (byte == FLAG_SET) state = STOP;
                        else state = START;
                        break;
                    default: 
                        break;
                }
            }
        } 
        retransmitions--;
    }

    if (state != STOP) return -1;
    sendSupervisionFrame(fd, A_ER, C_UA);
    return close(fd);
}









unsigned char readControlFrame(int fd){

    unsigned char byte, cField = 0;
    LlState state = START;
    
    while (state != STOP && setAlarm == FALSE) {  
        if (read(fd, &byte, 1) > 0 || 1) {
            switch (state) {
                case START:
                    if (byte == FLAG_SET) state = FLAG;
                    break;
                case FLAG:
                    if (byte == A_RE) state = A_RCV;
                    else if (byte != FLAG_SET) state = START;
                    break;
                case A_RCV:
                    if (byte == C_RR0(0) || byte == C_RR0(1) || byte == C_REJ0(0) || byte == C_REJ0(1) || byte == C_DISC){
                        state = C_RCV;
                        cField = byte;   
                    }
                    else if (byte == FLAG_SET) state = FLAG;
                    else state = START;
                    break;
                case C_RCV:
                    if (byte == (A_RE ^ cField)) state = BCC1;
                    else if (byte == FLAG_SET) state = FLAG;
                    else state = START;
                    break;
                case BCC1:
                    if (byte == FLAG_SET){
                        state = STOP;
                    }
                    else state = START;
                    break;
                default: 
                    break;
            }
        } 
    } 
    return cField;
}

int sendSupervisionFrame(int fd, unsigned char A, unsigned char C){
    unsigned char FRAME[5] = {FLAG_SET, A, C, A ^ C, FLAG_SET};
    return write(fd, FRAME, 5);
}