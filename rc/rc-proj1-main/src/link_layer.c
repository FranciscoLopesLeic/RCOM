// Link layer protocol implementation

#include "link_layer.h"
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <termios.h>



#define _POSIX_SOURCE 1 // POSIX compliant source

volatile int STOP = FALSE;
int alarmTriggered = FALSE;
int alarmCount = 0;
int timeout = 0;
int retransmissions = 0;
unsigned char Tx = 0;
unsigned char Rx = 1;
int infoFlag = 0;
int previous = 0;
int fd;

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

void alarmHandler(int signum) {
    alarmTriggered = TRUE;
    alarmCount++;
}

int llopen(LinkLayer connectionParameters){

    LinkLayerState state = START;
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror(connectionParameters.serialPort);
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

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
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

    if (fd < 0) return -1;

    unsigned char byte;
    timeout = connectionParameters.timeout;
    retransmissions = connectionParameters.nRetransmissions;
    switch (connectionParameters.role)
    {
    case LlTx:{

        (void) signal(SIGALRM, alarmHandler);
        while (connectionParameters.nRetransmissions != 0 && state != STOP_R){
            writeCtrlFrame(fd,A_ER, C_SET);
            alarm(connectionParameters.timeout);
            alarmTriggered = FALSE;

            while (alarmTriggered == FALSE && state != STOP_R) {
                if (read(fd, &byte, 1) > 0) {
                    switch (state) {
                        case START:
                            if (byte == FLAG) state = FLAG_RCV;
                            break;
                            case FLAG_RCV:
                                if (byte == A_RE) state = A_RCV;
                                else if (byte != FLAG) state = START;
                                break;
                            case A_RCV:
                                if (byte == C_UA) state = C_RCV;
                                else if (byte == FLAG) state = FLAG_RCV;
                                else state = START;
                                break;
                            case C_RCV:
                                if (byte == (A_RE ^ C_UA)) state = BCC;
                                else if (byte == FLAG) state = FLAG_RCV;
                                else state = START;
                                break;
                            case BCC:
                                if (byte == FLAG) state = STOP_R;
                                else state = START;
                                break;
                            default: 
                                break;
                    }
                }

            }
            connectionParameters.nRetransmissions--;
        }
        if(state != STOP_R) return -1;
        break;
        }
    
    case LlRx: {

            while (state != STOP_R) {
                if (read(fd, &byte, 1) > 0) {
                    switch (state) {
                        case START:
                            if (byte == FLAG) state = FLAG_RCV;
                            break;
                        case FLAG_RCV:
                            if (byte == A_ER) state = A_RCV;
                            else if (byte != FLAG) state = START;
                            break;
                        case A_RCV:
                            if (byte == C_SET) state = C_RCV;
                            else if (byte == FLAG) state = FLAG_RCV;
                            else state = START;
                            break;
                        case C_RCV:
                            if (byte == (A_ER ^ C_SET)) state = BCC;
                            else if (byte == FLAG) state = FLAG_RCV;
                            else state = START;
                            break;
                        case BCC:
                            if (byte == FLAG) state = STOP_R;
                            else state = START;
                            break;
                        default: 
                            break;
                    }
                }
            }  
            writeCtrlFrame(fd, A_RE, C_UA);
            break; 
    }
    default:
        return -1;
        break;
    }


    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize){

    alarmCount = 0;
    (void)signal(SIGALRM,alarmHandler);

    char bcc2 = 0x00;
    for(int i = 0; i < bufSize; i++){
        bcc2 = bcc2 ^buf[i];
    }
    unsigned char frame[10000] = {0};

    frame[0] = FLAG;
    frame[1] = A_ER;
    frame[2] = (infoFlag << 6);
    frame[3] = A_ER ^ (infoFlag << 6);

    int index = 4;
    for(int i = 0; i < bufSize; i++) {
        if (buf[i] == 0x7E) {
            frame[index] = 0x7D;
            index++;
            frame[index] = 0x5E;
            index++;
        }
        else if (buf[i] == 0x7D) {
            frame[index] = 0x7D;
            index++;
            frame[index] = 0x5D;
            index++;
        }
        else {
            frame[index] = buf[i];
            index++;
        }
    }

    if (bcc2 == 0x7E) {
        frame[index] = 0x7D;
        index++;
        frame[index] = 0x5E;
        index++;
    }
    else if (bcc2 == 0x7D) {
        frame[index] = 0x7D;
        index++;
        frame[index] = 0x5D;
        index++;
    }
    else {
        frame[index] = bcc2;
        index++;
    }
    frame[index] = FLAG;
    index++;

    int STOP = FALSE;
    unsigned char rcv[5];
    alarmCount = 0;
    alarmTriggered = FALSE;


    while(alarmCount < retransmissions){
        if(alarmTriggered == FALSE){
            write(fd, frame, index);
            printf("\nInfo Frame sent Ns = %d\n", infoFlag);
            alarm(timeout);
            alarmTriggered = TRUE;
        }

        int response = read(fd, rcv, 5);

        if(response > 0){
            if(rcv[2] != (!infoFlag << 7 | 0x05)){
                printf("\nREJ Received\n");
                alarmTriggered = FALSE;
                continue;
            }
            else if(rcv[3] != (rcv[1]^rcv[2])){
                printf("\nRR not correct\n");
                alarmTriggered = FALSE;
                continue;
            }
            else{
                printf("\nRR correctly received\n");
                break;
            
            }
        }
        
        
    }
    if(alarmCount >= retransmissions){
        printf("TIME-OUT\n");
        return -1;
    }
    previous = infoFlag;
    if(infoFlag) infoFlag = 0;
    else infoFlag = 1;


    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet,int *sizePacket){
    unsigned char c;
    unsigned char info[1000];
    int sizeInfo = 0;
    enum statePacket packetState = packet_START;

    while (packetState != packet_STOP){
        int bytes = read(fd, &c, 1);

        if(bytes < 0){
            continue;;
        }

        switch (packetState){
            case packet_START:
                if(c == FLAG){
                    packetState = packet_FLAG1;
                    info[sizeInfo] = FLAG;
                    sizeInfo++;
                }
            break;
            case packet_FLAG1:
                if(c == FLAG){
                    packetState = packet_FLAG1;
                }
                else {
                    packetState = packet_A;
                    info[sizeInfo] = c;
                    sizeInfo++;
                }
                break;
            case packet_A:
                if (c == FLAG) {
                    packetState = packet_STOP;
                    info[sizeInfo] = c;
                    sizeInfo++;
                }
                else {
                    info[sizeInfo] = c;
                    sizeInfo++;
                }
                break;
        
        default:
            break;
        }
    }
    
    unsigned char  rFrame[5];
    rFrame[0] = FLAG;
    rFrame[1] = A_ER;
    rFrame[4] = FLAG;

    if(info[2] != (infoFlag << 6)){
        printf("\nInfo Frame not received correctly\n");
        rFrame[2] = ((infoFlag << 7) | 0x01);
        rFrame[3] = A_ER ^rFrame[2];
        write(fd, rFrame, 5);

        return -1;
    }
    else if((info[1]^info[2]) != info[3]){
        printf("\nError in the protocol\n");
        rFrame[2] = ((infoFlag << 7) | 0x01);
        rFrame[3] = A_ER ^rFrame[2];
        write(fd, rFrame,5);

        return -1;
    }

    int index = 0;
    for(int i = 0; i < sizeInfo; i++){
        if(info[i] == 0x7D && info[i+1]==0x5e){
            packet[index] = FLAG;
            index++;
            i++;
        }

        else if(info[i] == 0x7D && info[i+1]==0x5d){
            packet[index] = 0x7D;
            index++;
            i++;
        }

        else {
            packet[index] = info[i];
            index++;
        }
    }

    unsigned char bcc2 = 0x00;
    int size = 0;
    if(packet[4] == 0x01){
        size = 256*packet[6] + packet[7] + 4 + 5;
    } else{
        size += packet[6] + 3 + 4;
        size += packet[size+2] + 2 +2;
    }
    for(int i = 4; i < size-1; i++){
        bcc2 = bcc2 ^ packet[i];
    }

    if(packet[size-1] == bcc2){
        if(packet[4]==0x01){
            if(info[5] == previous){
                printf("\nDuplicate Frame. Sending RR.\n");
                rFrame[2] = (!infoFlag << 7) | 0x05;
                rFrame[3] = rFrame[1] ^ rFrame[2];
                write(fd, rFrame, 5);
                if(infoFlag) infoFlag = 0;
                else infoFlag = 1;
                return -1;
            }   
            else{
                previous = info[5];
            }
        }
        printf("\nReceived InfoFrame! Sending RR\n");
        rFrame[2] = (!infoFlag << 7) | 0x05;
        rFrame[3] = rFrame[1] ^ rFrame[2];
        write(fd, rFrame, 5);
    }
    
    else {
        printf("\nError in the data. Sending REJ.\n");
        rFrame[2] = (infoFlag << 7) | 0x01;
        rFrame[3] = rFrame[1] ^ rFrame[2];
        write(fd, rFrame, 5);

        return -1;
    }
    

    index = 0;
    unsigned char packetAux[400];
    
    for(int i = 4; i < size-1; i++){
        packetAux[index] = packet[i];
        index++;
    }
    
    (*sizePacket) = size - 5;
    for(int i=0; i < (*sizePacket); i++){
        packet[i] = 0;
    }
    for(int i=0; i<(*sizePacket); i++){
        packet[i] = packetAux[i];
    }
    previous = infoFlag;
    if(infoFlag) infoFlag = 0;
    else infoFlag = 1;
    return 0;
    
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics){

    LinkLayerState state = START;
    unsigned char byte;
    (void) signal(SIGALRM, alarmHandler);

     while (retransmissions != 0 && state != STOP_R) {
                
        writeCtrlFrame(fd, A_ER, C_DISC);
        alarm(timeout);
        alarmTriggered = FALSE;
                
        while (alarmTriggered == FALSE && state != STOP_R) {
            if (read(fd, &byte, 1) > 0) {
                switch (state) {
                    case START:
                        if (byte == FLAG) state = FLAG_RCV;
                        break;
                    case FLAG_RCV:
                        if (byte == A_RE) state = A_RCV;
                        else if (byte != FLAG) state = START;
                        break;
                    case A_RCV:
                        if (byte == C_DISC) state = C_RCV;
                        else if (byte == FLAG) state = FLAG_RCV;
                        else state = START;
                        break;
                    case C_RCV:
                        if (byte == (A_RE ^ C_DISC)) state = BCC;
                        else if (byte == FLAG) state = FLAG_RCV;
                        else state = START;
                        break;
                    case BCC:
                        if (byte == FLAG) state = STOP_R;
                        else state = START;
                        break;
                    default: 
                        break;
                }
            }
        } 
        retransmissions--;
    }

    if (state != STOP_R) return -1;
    writeCtrlFrame(fd, A_ER, C_UA);
    return close(fd);
}

int writeCtrlFrame(int fd, unsigned char addr, unsigned char crtl){
    unsigned char FRAME[5] = {FLAG,addr,crtl,addr ^ crtl, FLAG};
    return write(fd, FRAME, 5);
}

unsigned char readCtrlFrame(int fd){
    unsigned char byte, f = 0;
    LinkLayerState state = START;
    
    while (state != STOP_R && alarmTriggered == FALSE) {  
        if (read(fd, &byte, 1) > 0 || 1) {
            switch (state) {
                case START:
                    if (byte == FLAG) state = FLAG_RCV;
                    break;
                case FLAG_RCV:
                    if (byte == A_RE) state = A_RCV;
                    else if (byte != FLAG) state = START;
                    break;
                case A_RCV:
                    if (byte == ((0<<7)|0x05) || byte == ((1<<7)|0x05) || byte == ((0<<7)|0x01) || byte == ((1<<7)|0x01) || byte == C_DISC){
                        state = C_RCV;
                        f = byte;   
                    }
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;
                case C_RCV:
                    if (byte == (A_RE ^ f)) state = BCC;
                    else if (byte == FLAG) state = FLAG_RCV;
                    else state = START;
                    break;
                case BCC:
                    if (byte == FLAG){
                        state = STOP_R;
                    }
                    else state = START;
                    break;
                default: 
                    break;
            }
        } 
    } 
    return f;
}