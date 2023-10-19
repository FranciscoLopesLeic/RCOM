// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <sys/stat.h>

int controlPacket(char *filename, int filesize, unsigned char *controlPacket, int controlPacketSize)
{
    if (strlen(filename) > 255)
    {
        printf("Filename too long.\n");
        return -1;
    }

    unsigned char string[20];

    sprintf(string, "%021x", filesize);

    int bytes = strlen(string) / 2;

    controlPacket[1] = 0;
    controlPacket[2] = bytes;

    if (controlPacketSize)
    {
        controlPacket[0] = 0x02;
    }
    else
    {
        controlPacket[0] = 0x03;
    }

    int j = 4;
    for(int i = (bytes - 1); i > -1; i--){
        controlPacket[j] = filesize >> (i * 8);
        j++;
    }

    controlPacket[j] = 0x01;
    j++;

    int file_size = strlen(filename);

    controlPacket[j] = file_size;
    j++;

    for (int i = 0; i < file_size; i++)
    {
        controlPacket[j] = filename[i];
        j++;
    }

    return j;
}

int dataPacket(unsigned char* data, unsigned int nbytes, int index){
    unsigned char dataPacket[1024] = {0};
    dataPacket[0] = 0x01;
    dataPacket[1] = index % 255;
    dataPacket[2] = nbytes/256;
    dataPacket[3] = nbytes%256;
    for(int i = 4; i < nbytes; i++){
        dataPacket[i] = data[i];
    }
    for(int i = 0; i < nbytes + 4; i++){
        data[i] = dataPacket[i];
    }
    return nbytes + 4;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    printf('Already in layer\n');

    LinkLayer linkLayer;
    strcpy(linkLayer.serialPort, serialPort);
    linkLayer.baudRate = baudRate;

    if (strcmp(role, "tx") == 0)
    {
        linkLayer.role = LlTx;
    }
    else if (strcmp(role, "rx") == 0)
    {
        linkLayer.role = LlRx;
    }
    linkLayer.nRetransmissions = nTries;
    linkLayer.timeout = timeout;

    if(llopen(linkLayer) == -1){
        printf("Error opening connection\n");
        return;
    }
    else{
        printf("Connection opened\n");
    }

    if(linkLayer.role == LlTx){
        unsigned char controlPacket[1024] = {0};
        struct stat st;
        stat(filename, &st);
        int file = open(filename, O_RDONLY);

        if(file == -1){
            printf("Error opening file\n");
            return;
        }
        else{
            printf("File opened\n");
        }

        int controlPacketSize = controlPacket(filename, st.st_size, controlPacket, 1);

        if(llwrite(controlPacket, controlPacketSize) < 0){
            llclose(0, linkLayer);
            return;
        }
        unsigned int bytesRead;
        unsigned int index = 0;
        int count = 0;
        while((bytesRead = read(file, controlPacket, 1024-4)) > 0){
            index ++;
            count += bytesRead;
            bytesRead = dataPacket(&controlPacket, bytesRead, index);
            int dataPacketSize = dataPacket(controlPacket, bytesRead, index);
            if(llwrite(controlPacket, bytesRead) < 0){
                llclose(0, linkLayer);
                return;
            }
            printf("Sent %d bytes\n", count);
        }
        bytesRead = controlPacket(filename, st.st_size, &controlPacket, 0);

        if(llwrite(controlPacket, bytesRead) < 0){
            printf("Error writing control packet\n");
        }
        close(file);
    }
   
    
    else if(linkLayer.role == LlRx){
            int *file = -1;
            int STOP = FALSE;
            while (!STOP)
            {
                unsigned char controlPacket[1024] = {0};
                int size = 0;
                
            }
            
    }
}

