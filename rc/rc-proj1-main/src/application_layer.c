// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>



void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer linkLayer;
    strcpy(linkLayer.serialPort,serialPort);
    if (strcmp(role, "tx") == 0) linkLayer.role = LlTx;
    else if (strcmp(role, "rx") == 0) linkLayer.role = LlRx;
    linkLayer.baudRate = baudRate;
    linkLayer.nRetransmissions = nTries;
    linkLayer.timeout = timeout;

    if (llopen(linkLayer) == -1) {
        printf("\nCouldn't estabilish the connection\n");
        return;
    } else {
        printf("\nConnection estabilished\n");
    }

    switch (linkLayer.role)
    {
    case LlTx: {
        unsigned char packet[500];
        struct stat st;
        stat(filename, &st);
        int file = open(filename, O_RDONLY);

        if(!file){
            printf("Could not open file\n");
            return;
        }
        else {
            printf("Open file successfully\n");
        }

        unsigned int sizePac = writeCtrl(filename, st.st_size, 1, &packet);
        if(llwrite(packet, sizePac) < 0){
            llclose(0);
            return;
        }
        unsigned int bytes;
        unsigned int index = 0;
        int count = 0;
        while ((bytes = read(file, packet, 500-4)) > 0) {
            index++;
            count += bytes;
            bytes = writeData(&packet, bytes, index);
            if (llwrite(packet, bytes) < 0) {
                printf("Failed to send information frame\n");
                llclose(0);
                return;
            }
            printf("Sending: %d/%d (%d%%)\n", count, st.st_size, (int) (((double)count / (double)st.st_size) *100));
        }
        bytes = writeCtrl(filename, st.st_size, 0, &packet);
        
        if (llwrite(packet, bytes) < 0) {
            printf("Failed to send information frame\n");
        }
        close(file);
        break;
    }
    case LlRx: {
        FILE *file = -1;
        int STOP = FALSE;
        while(!STOP){
            unsigned char packet[600] = {0};
            int sizePacket = 0;
            int response = llread(&packet,&sizePacket);
            if(response < 0){
                continue;
            }
            if(packet[0] == 0x02){
                printf("\nStart control\n");
                file = fopen(filename, "wb"); 
            }
            else if(packet[0]==0x03){
                printf("\nEnd control\n");
                fclose(file);
                STOP = TRUE;  
            }
            else{
                for(int i=4; i<sizePacket; i++){
                    fputc(packet[i], file);
                }
            }
        }
    }
    default:
        break;
    }
    if(llclose(1)<0){
        printf("Couldn't close\n");    
    }

}


int writeCtrl(char* filename, int fileSize, int start, unsigned char* packet) {
    unsigned char size_s[20];
    sprintf(size_s, "%02lx", fileSize);
    int sizeBytes = strlen(size_s) / 2;

    if (start == 1) packet[0] = 0x02;
    else packet[0] = 0x03;

    packet[1] = 0;
    packet[2] = sizeBytes;

    int i = 4;
    for (int j = (sizeBytes)-1; j > -1; j--){
        packet[i] = fileSize >> (j*8);
        i++;
    }
    
    packet[i] = 1;
    i++;

    int filename_size = strlen(filename);

    packet[i] = filename_size;
    i++;
    for (int j = 0; j < filename_size; j++){
        packet[i] = filename[i];
        i++;
    }
    
    return i; 
}

int writeData(unsigned char* packet, unsigned int nBytes, int index){
    unsigned char buf[500] = {0};

    buf[0] = 0x01;
	buf[1] = index%255;
    buf[2] = nBytes/256;
    buf[3] = nBytes%256;

    for(int i = 4; i < nBytes; i++){ 
        buf[i] = packet[i];
    }

    for (int j = 0; j < (nBytes+4); j++) {
        packet[j] = buf[j];
    }

	return (nBytes+4);
}