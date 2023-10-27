// Link layer protocol implementation

#include "link_layer.h"
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <fcntl.h>

#include <stdio.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source

LinkLayer linkLayer;
LinkLayerRole role;

RXUser receptor;
TXUser transmitter;

DataHolder dh;
AlarmConfigOptions alarm_config;

int receptor_num = 1;
int transmitter_num = 0;

////////////////////////////////////////////////
// ALARMHANDLER
////////////////////////////////////////////////
void ALARMHANDLER(int s)
{
    alarm_config.count++;
    if (write(transmitter.fd, dh.buffer, dh.length) != dh.length)
    {
        printf("Error writing to serial port\n");
        return;
    }
    alarm(alarm_config.timeout);

    if (alarm_config.count <= alarm_config.num_retransmissions)
    {
        printf("Alarm %d\n", alarm_config.count);
    }
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{
    int ret = 1;

    switch (connectionParameters.role)
    {
    case LlTx:
        alarm_config.count = 0;
        alarm_config.timeout = connectionParameters.timeout;
        alarm_config.num_retransmissions = connectionParameters.nRetransmissions;

        (void)signal(SIGALRM, ALARMHANDLER);

        transmitter.fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);

        if (transmitter.fd < 0)
        {
            ret = -1;
        }
        if (tcgetattr(transmitter.fd, &transmitter.oldtio) == -1)
        {
            ret = -1;
        }

        memset(&transmitter.newtio, 0, sizeof(transmitter.newtio));

        transmitter.newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
        transmitter.newtio.c_iflag = IGNPAR;
        transmitter.newtio.c_oflag = 0;

        transmitter.newtio.c_lflag = 0;
        transmitter.newtio.c_cc[VTIME] = 0;
        transmitter.newtio.c_cc[VMIN] = 0;

        tcflush(transmitter.fd, TCIOFLUSH);

        if (tcsetattr(transmitter.fd, TCSANOW, &transmitter.newtio) == -1)
        {
            ret = -1;
        }

        alarm_config.count = 0;

        dh.buffer[0] = FLAG;
        dh.buffer[1] = TX_ADDRESS;
        dh.buffer[2] = SET_CONTROL;
        dh.buffer[3] = TX_ADDRESS ^ SET_CONTROL;
        dh.buffer[4] = FLAG;
        dh.length = 5;

        if (write(transmitter.fd, dh.buffer, dh.length) != dh.length)
        {
            ret = -1;
        }

        alarm(alarm_config.timeout);

        if (readSupervisionAux(transmitter.fd, RX_ADDRESS, UA_CONTROL, NULL) != 0)
        {
            alarm(0);
            ret = -1;
        }
        alarm(0);
        role = LlTx;
        break;
    case LlRx:
        receptor.fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);
        if (receptor.fd < 0)
        {
            ret = -1;
        }
        if (tcgetattr(receptor.fd, &receptor.oldtio) == -1)
        {
            ret = -1;
        }

        memset(&receptor.newtio, 0, sizeof(receptor.newtio));

        receptor.newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
        receptor.newtio.c_iflag = IGNPAR;
        receptor.newtio.c_oflag = 0;

        receptor.newtio.c_lflag = 0;
        receptor.newtio.c_cc[VTIME] = 0;
        receptor.newtio.c_cc[VMIN] = 0;

        tcflush(receptor.fd, TCIOFLUSH);

        if (tcsetattr(receptor.fd, TCSANOW, &receptor.newtio) == -1)
        {
            ret = -1;
        }

        while (readSupervisionAux(receptor.fd, TX_ADDRESS, SET_CONTROL, NULL) != 0)
        {
        }

        dh.buffer[0] = FLAG;
        dh.buffer[1] = RX_ADDRESS;
        dh.buffer[2] = UA_CONTROL;
        dh.buffer[3] = RX_ADDRESS ^ UA_CONTROL;
        dh.buffer[4] = FLAG;
        dh.length = 5;

        if (write(receptor.fd, dh.buffer, dh.length) != dh.length)
        {
            ret = -1;
        }
        role = LlRx;
        break;
    default:
        ret = -1;
        break;
    }

    return ret;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    if (role == LlRx)
    {
        return 1;
    }

    alarm_config.count = 0;

    buildInfoAux(transmitter.fd, TX_ADDRESS, I_CONTROL(transmitter_num), buf, bufSize);
    if (write(transmitter.fd, dh.buffer, dh.length) != dh.length)
    {
        return 1;
    }
    alarm(alarm_config.timeout);

    int res = -1;
    uint8_t rej_ctrl = REJ_CONTROL(1 - transmitter_num);

    while (res != 0)
    {
        res = readSupervisionAux(transmitter.fd, RX_ADDRESS, RR_CONTROL(1 - transmitter_num), &rej_ctrl);
        if (res == 1)
        {
            break;
        }
    }
    alarm(0);
    if (res == 1)
    {
        return 2;
    }

    transmitter_num = 1 - transmitter_num;
    printf("Sending packets... ");
    /*for (int i = 0; i < bufSize; i++) {
        printf("0x%02x ", buf[i]);
    }*/
    printf("\n");

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    if (role == LlTx)
    {
        return 1;
    }

    sleep(1);
    if (readInfoAux(receptor.fd, TX_ADDRESS, I_CONTROL(1 - receptor_num), I_CONTROL(receptor_num)) != 0)
    {
        dh.buffer[0] = FLAG;
        dh.buffer[1] = RX_ADDRESS;
        dh.buffer[2] = RR_CONTROL(1 - receptor_num);
        dh.buffer[3] = RX_ADDRESS ^ RR_CONTROL(1 - receptor_num);
        dh.buffer[4] = FLAG;
        dh.length = 5;

        if (write(receptor.fd, dh.buffer, dh.length) != dh.length)
        {
            return -1;
        }

        return 0;
    }

    uint8_t data[DATA_SIZE];
    uint8_t bcc2_;

    uint8_t destuffed_data[DATA_SIZE + 1];
    size_t idx = 0;

    for (size_t i = 0; i < dh.length; i++)
    {
        if (dh.buffer[i] == ESC)
        {
            i++;
            destuffed_data[idx++] = dh.buffer[i] ^ STUFF_XOR;
        }
        else
        {
            destuffed_data[idx++] = dh.buffer[i];
        }
    }

    bcc2_ = destuffed_data[idx - 1];

    memcpy(data, destuffed_data, idx - 1);

    idx = idx - 1;

    uint8_t tmp_bcc2 = 0;
    for (size_t i = 0; i < idx; i++)
    {
        tmp_bcc2 ^= data[i];
    }

    if (tmp_bcc2 != bcc2_)
    {
        dh.buffer[0] = FLAG;
        dh.buffer[1] = RX_ADDRESS;
        dh.buffer[2] = REJ_CONTROL(1 - receptor_num);
        dh.buffer[3] = RX_ADDRESS ^ REJ_CONTROL(1 - receptor_num);
        dh.buffer[4] = FLAG;
        dh.length = 5;
        if (write(receptor.fd, dh.buffer, dh.length) != dh.length)
        {
            return -1;
        }

        return 0;
    }

    memcpy(packet, data, idx);
    dh.buffer[0] = FLAG;
    dh.buffer[1] = RX_ADDRESS;
    dh.buffer[2] = RR_CONTROL(receptor_num);
    dh.buffer[3] = RX_ADDRESS ^ RR_CONTROL(receptor_num);
    dh.buffer[4] = FLAG;
    dh.length = 5;

    if (write(receptor.fd, dh.buffer, dh.length) != dh.length)
    {
        return -1;
    }

    receptor_num = 1 - receptor_num;

    memcpy(packet, data, idx);

    printf("Receiving packets... ");
    /*for (int i = 0; i < size; i++) {
        printf("0x%02x ", packet[i]);
    }*/
    printf("\n");

    return idx;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    int ret = 1;
    switch (role)
    {
    case LlTx:
        alarm_config.count = 0;

        dh.buffer[0] = FLAG;
        dh.buffer[1] = TX_ADDRESS;
        dh.buffer[2] = DISC_CONTROL;
        dh.buffer[3] = TX_ADDRESS ^ DISC_CONTROL;
        dh.buffer[4] = FLAG;
        dh.length = 5;

        if (write(transmitter.fd, dh.buffer, dh.length) != dh.length)
        {
            ret = -1;
        }

        alarm(alarm_config.timeout);

        int flag = 0;

        for (;;)
        {
            if (readSupervisionAux(transmitter.fd, RX_ADDRESS, DISC_CONTROL, NULL) == 0)
            {
                flag = 1;
                break;
            }

            if (alarm_config.count == alarm_config.num_retransmissions)
            {
                break;
            }
        }

        alarm(0);

        if (!flag)
        {
            ret = -1;
        }

        dh.buffer[0] = FLAG;
        dh.buffer[1] = TX_ADDRESS;
        dh.buffer[2] = UA_CONTROL;
        dh.buffer[3] = TX_ADDRESS ^ UA_CONTROL;
        dh.buffer[4] = FLAG;
        dh.length = 5;

        if (write(transmitter.fd, dh.buffer, dh.length) != dh.length)
        {
            ret = -1;
        }

        if (tcdrain(transmitter.fd) == -1)
        {
            ret = -1;
        }

        if (tcsetattr(transmitter.fd, TCSANOW, &transmitter.oldtio) == -1)
        {
            ret = -1;
        }

        close(transmitter.fd);

        break;
    case LlRx:

        while (readSupervisionAux(receptor.fd, TX_ADDRESS, DISC_CONTROL, NULL) != 0)
        {
        }

        dh.buffer[0] = FLAG;
        dh.buffer[1] = RX_ADDRESS;
        dh.buffer[2] = DISC_CONTROL;
        dh.buffer[3] = RX_ADDRESS ^ DISC_CONTROL;
        dh.buffer[4] = FLAG;
        dh.length = 5;

        if (write(receptor.fd, dh.buffer, dh.length) != dh.length)
        {
            ret = -1;
        }

        while (readSupervisionAux(receptor.fd, TX_ADDRESS, UA_CONTROL, NULL) != 0)
        {
        }

        if (tcdrain(receptor.fd) == -1)
        {
            ret = -1;
        }

        if (tcsetattr(receptor.fd, TCSANOW, &receptor.oldtio) == -1)
        {
            ret = -1;
        }

        close(receptor.fd);
        break;
    default:
        ret = -1;
        break;
    }

    return ret;
}

////////////////////////////////////////////////
// READSUPERVISION
////////////////////////////////////////////////
int readSupervisionAux(int fd, uint8_t address, uint8_t control, uint8_t *rej_ctrl)
{
    uint8_t b;

    LinkLayerState state = START;

    uint8_t is_rej;

    while (state != STOP)
    {
        if (alarm_config.count > alarm_config.num_retransmissions)
        {
            return 1;
        }
        if (read(fd, &b, 1) != 1)
        {
            continue;
        }
        if (state == START)
        {
            if (b == FLAG)
            {
                state = FLAG_RCV;
            }
        }
        else if (state == FLAG_RCV)
        {
            is_rej = 0;
            if (b == address)
            {
                state = A_RCV;
            }
            else if (b != FLAG)
            {
                state = START;
            }
        }
        else if (state == A_RCV)
        {
            if (rej_ctrl != NULL)
            {
                if (b == *rej_ctrl)
                {
                    is_rej = 1;
                }
            }
            if (b == control || is_rej)
            {
                state = C_RCV;
            }
            else if (b == FLAG)
            {
                state = FLAG_RCV;
            }
            else
            {
                state = START;
            }
        }
        else if (state == C_RCV)
        {
            if ((!is_rej && b == (address ^ control)) || (is_rej && b == (address ^ *rej_ctrl)))
            {
                state = BCC_OK;
            }
            else if (b == FLAG)
            {
                state = FLAG_RCV;
            }
            else
            {
                state = START;
            }
        }
        else if (state == BCC_OK)
        {
            if (b == FLAG)
            {
                if (is_rej)
                {
                    return 2;
                }
                state = STOP;
            }
            else
            {
                state = START;
            }
        }
    }

    return 0;
}

////////////////////////////////////////////////
// BUILDINFORMATION
////////////////////////////////////////////////
void buildInfoAux(int fd, uint8_t address, uint8_t control, const uint8_t *packet, size_t packet_length)
{
    uint8_t bcc2_ = 0;
    for (size_t i = 0; i < packet_length; i++)
    {
        bcc2_ ^= packet[i];
    }

    uint8_t stuffed_data[STUFFED_DATA_SIZE];
    size_t stuffed_length = 0;
    for (int i = 0; i < packet_length; i++)
    {
        if (packet[i] == FLAG || packet[i] == ESC)
        {
            stuffed_data[stuffed_length++] = ESC;
            stuffed_data[stuffed_length++] = packet[i] ^ STUFF_XOR;
        }
        else
        {
            stuffed_data[stuffed_length++] = packet[i];
        }
    }
    if (bcc2_ == FLAG || bcc2_ == ESC)
    {
        stuffed_data[stuffed_length++] = ESC;
        stuffed_data[stuffed_length++] = bcc2_ ^ STUFF_XOR;
    }
    else
    {
        stuffed_data[stuffed_length++] = bcc2_;
    }
    memcpy(dh.buffer + 4, stuffed_data, stuffed_length);

    dh.buffer[0] = FLAG;
    dh.buffer[1] = address;
    dh.buffer[2] = control;
    dh.buffer[3] = address ^ control;
    dh.buffer[4 + stuffed_length] = FLAG;
    dh.length = 4 + stuffed_length + 1;
}

int readInfoAux(int fd, uint8_t address, uint8_t control, uint8_t repeatedCtrl)
{
    uint8_t b;
    LinkLayerState state = START;

    uint8_t isRepeated = 0;
    dh.length = 0;
    memset(dh.buffer, 0, STUFFED_DATA_SIZE + 5);

    while (state != STOP)
    {

        if (alarm_config.count > alarm_config.num_retransmissions)
        {
            return 1; // Transmission failed
        }

        if (read(fd, &b, 1) != 1)
        {
            continue; // Wait for data to be available
        }

        switch (state)
        {
        case START:

            if (b == FLAG)
            {
                state = FLAG_RCV;
            }

            break;

        case FLAG_RCV:

            isRepeated = 0;

            if (b == address)
            {
                state = A_RCV;
            }

            else if (b != FLAG)
            {
                state = START;
            }

            break;

        case A_RCV:

            if (b == repeatedCtrl)
            {
                isRepeated = 1;
            }

            if (b == control || isRepeated)
            {
                state = C_RCV;
            }

            else if (b == FLAG)
            {
                state = FLAG_RCV;
            }

            else
            {
                state = START;
            }

            break;

        case C_RCV:

            if ((!isRepeated && b == (address ^ control)) || (isRepeated && b == (address ^ repeatedCtrl)))
            {
                state = BCC_OK;
            }

            else if (b == FLAG)
            {
                state = FLAG_RCV;
            }

            else
            {
                state = START;
            }

            break;

        case BCC_OK:

            if (b == FLAG)
            {
                state = STOP;
                if (isRepeated)
                {
                    return 2; // Frame received is a repeated control frame
                }
            }

            else
            {
                dh.buffer[dh.length++] = b;
            }

            break;

        case STOP:
            // Nothing to do
            break;
        }
    }

    return 0; // Successful frame reception
}