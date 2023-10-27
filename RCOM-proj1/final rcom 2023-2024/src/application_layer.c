// Application layer protocol implementation

#include "application_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer ll;
    strcpy(ll.serialPort, serialPort);
    ll.role = (strcmp(role, "tx") == 0) ? LlTx : LlRx;
    ll.baudRate = baudRate;
    ll.nRetransmissions = nTries;
    ll.timeout = timeout;

    int fd = llopen(ll);
    if (fd < 0)
    {
        perror("Connection error\n");
        return;
    }

    switch (ll.role)
    {
    case LlTx:
    {
        FILE *fs = fopen(filename, "rb");
        if (!fs)
        {
            perror("Failed to open file");
            return;
        }
        fseek(fs, 0, SEEK_END);
        size_t file_size = ftell(fs);
        fseek(fs, 0, SEEK_SET);

        uint8_t buf[MAX_PACKET_SIZE];
        size_t size = strlen(filename);

        if (size > 0xff || (size + sizeof(size_t) + 1) > MAX_PACKET_SIZE)
        {
            fprintf(stderr, "Invalid control packet parameters\n");
            fclose(fs);
            return;
        }

        buf[0] = CONTROL_START;
        buf[1] = FILE_SIZE;
        buf[2] = sizeof(size_t);
        memcpy(buf + 3, &file_size, sizeof(size_t));
        buf[3 + sizeof(size_t)] = FILE_NAME;
        buf[4 + sizeof(size_t)] = (uint8_t)size;
        memcpy(buf + 5 + sizeof(size_t), filename, size);

        size_t plength = 5 + sizeof(size_t) + size;

        if (plength == 0)
        {
            printf("Error creating control packet\n");
            fclose(fs);
            return;
        }

        if (llwrite(buf, plength) == -1)
        {
            fclose(fs);
            return;
        }

        uint8_t buf1[MAX_PACKET_SIZE - 3];
        size_t bytes_read;

        while ((bytes_read = fread(buf1, 1, sizeof(buf1), fs)) > 0)
        {
            uint8_t packet[MAX_PACKET_SIZE];

            packet[0] = CONTROL_DATA;
            packet[1] = (uint8_t)(bytes_read / OCTET_MULT);
            packet[2] = (uint8_t)(bytes_read % OCTET_MULT);
            memcpy(packet + 3, buf1, bytes_read);
            if (llwrite(packet, bytes_read + 3) == -1)
            {
                fclose(fs);
                printf("Error sending data packet\n");
                return;
            }
        }

        fclose(fs);

        uint8_t buf2[MAX_PACKET_SIZE];

        size_t size2 = strlen(filename);

        if (size2 > 0xff || (size2 + sizeof(size_t) + 1) > MAX_PACKET_SIZE)
        {
            fprintf(stderr, "Invalid control packet parameters\n");
            return;
        }

        buf2[0] = CONTROL_END;
        buf2[1] = FILE_NAME;
        buf2[2] = sizeof(size_t);
        memcpy(buf2 + 3, &file_size, sizeof(size_t));
        buf2[3 + sizeof(size_t)] = FILE_NAME;
        buf2[4 + sizeof(size_t)] = (uint8_t)size2;
        memcpy(buf2 + 5 + sizeof(size_t), filename, size2);

        plength = 5 + sizeof(size_t) + size2;

        if (plength == 0)
        {
            printf("Error creating control packet\n");
            fclose(fs);
            return;
        }

        if (llwrite(buf2, plength) == -1)
        {
            printf("Error sending control packet\n");
            fclose(fs);
            return;
        }

        fclose(fs);
        break;
    }
    case LlRx:
    {
        uint8_t buf[MAX_PACKET_SIZE];
        int size = llread(buf);

        if (size < 0 || buf[0] != CONTROL_START)
        {
            fprintf(stderr, "Failed to receive file\n");
            return;
        }

        uint8_t type;
        size_t length;
        size_t offset = 1;
        size_t file_size;
        char received_filename[0xff];

        while (offset < size)
        {
            type = buf[offset++];
            length = buf[offset++];

            if (type == FILE_SIZE && length == sizeof(size_t))
            {
                memcpy(&file_size, buf + offset, sizeof(size_t));
                offset += sizeof(size_t);
            }
            else if (type == FILE_NAME && length <= MAX_PACKET_SIZE - offset)
            {
                memcpy(received_filename, buf + offset, length);
                offset += length;
            }
            else
            {
                fprintf(stderr, "Failed to receive file\n");
                return;
            }
        }
        FILE *fs = fopen(filename, "wb");

        if (!fs)
        {
            perror("Failed to open file");
            return;
        }

        while (llread(buf) > 0)
        {
            if (buf[0] == CONTROL_END)
            {
                break;
            }
            else if (buf[0] != CONTROL_DATA)
            {
                fclose(fs);
                fprintf(stderr, "Failed to receive file\n");
                return;
            }

            size_t length = buf[1] * OCTET_MULT + buf[2];
            fwrite(buf + 3, sizeof(uint8_t), length, fs);
        }

        fclose(fs);
        break;
    }
    }
    llclose(0);
}
