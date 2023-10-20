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
    (void)signal(SIGALRM, alarmHandler);
    
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
    unsigned char c;
    unsigned char infoFrame[1000];
    int sizeInfoFrame = 0;
    enum statePacket statePac = START_PACKET;
    
    while (statePac != STOP_PACKET) {
        int bytes = read(fd, &c, 1);
        
        if(bytes < 0){
            continue;
        }

        //state machine pra cabeçalho
        switch(statePac){
            case START_PACKET:
                if (c == FLAG_SET) {
                    statePac = FLAG_RCV_PACKET;
                    infoFrame[sizeInfoFrame] = FLAG_SET;
                    sizeInfoFrame++;
                }
                break;
            case FLAG_RCV_PACKET:
                if (c == FLAG_SET){
                    statePac = FLAG_RCV_PACKET;
                }
                else {
                    statePac = A_RCV_PACKET;
                    infoFrame[sizeInfoFrame] = c;
                    sizeInfoFrame++;
                }
                break;
            case A_RCV_PACKET:
                if (c == FLAG_SET) {
                    statePac = STOP_PACKET;
                    infoFrame[sizeInfoFrame] = c;
                    sizeInfoFrame++;
                }
                else {
                    infoFrame[sizeInfoFrame] = c;
                    sizeInfoFrame++;
                }
                break;
            default:
                break;
        }
    }

    unsigned char rFrame[5];
    rFrame[0] = FLAG_SET;
    rFrame[1] = A;
    rFrame[4] = FLAG_SET;

    if(infoFrame[2] != (flag << 6)){ //campo de controlo

        printf("\nInfo Frame not received correctly\nSending REJ\n");
        rFrame[2] = (flag << 7) | 0x01;
        rFrame[3] = A ^ rFrame[2];
        write(fd, rFrame, 5);

        printf("return on line 540\n");
        return -1;
    }
    else if ((infoFrame[1]^infoFrame[2]) != infoFrame[3]){ //bcc1
        printf("\nError in the protocol\nSending REJ\n");
        rFrame[2] = (flag << 7) | 0x01;
        rFrame[3] = A ^ rFrame[2];
        write(fd, rFrame, 5);

        return -1;
    }

    //destuffing
    int index = 0;
    for(int i = 0; i < sizeInfoFrame; i++){
        if(infoFrame[i] == 0x7D && infoFrame[i+1]==0x5e){
            packet[index] = FLAG_SET;
            index++;
            i++;
        }

        else if(infoFrame[i] == 0x7D && infoFrame[i+1]==0x5d){
            packet[index] = 0x7D;
            index++;
            i++;
        }

        else {
            packet[index] = infoFrame[i];
            index++;
        }
    }
    unsigned char bcc2 = 0x00;
    int size = 0;
    if(packet[4] == 0x01){ //pacote de dados
        size = 256*packet[6] + packet[7] + 4 + 5;
    } else{ //pacote de controlo
        size += packet[6] + 3 + 4; //C, T1, L1, FLAG, A, C, BCC
        size += packet[size+2] + 2 +2; //2 para contar com T2 e L2 //+2 para contar com BCC2 e FLAG
    }
    for(int i = 4; i < size-1; i++){
        bcc2 = bcc2 ^ packet[i];
    }

    //confirmar bcc2
    if(packet[size-1] == bcc2){
        if(packet[4]==0x01){ // se for dados
            if(infoFrame[5] == prevTries){  // conferir numero de sequencia
                printf("\nDuplicate Frame. Sending RR.\n");
                rFrame[2] = (!flag << 7) | 0x05;
                rFrame[3] = rFrame[1] ^ rFrame[2];
                write(fd, rFrame, 5);
                if(flag) flag = 0;
                else flag = 1;
                return -1;
            }   
            else{
                prevTries = infoFrame[5];
            }
        }
        printf("\nReceived InfoFrame! Sending RR\n");
        rFrame[2] = (!flag << 7) | 0x05;
        rFrame[3] = rFrame[1] ^ rFrame[2];
        write(fd, rFrame, 5);
    }
    
    else {//erro no bcc2
        printf("\nError in the data. Sending REJ.\n");
        rFrame[2] = (flag << 7) | 0x01;
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
    
    (*packet) = size - 5;
    //esvaziar packet
    for(int i=0; i < (*packet); i++){
        packet[i] = 0;
    }
    //guardar dados
    for(int i=0; i<(*packet); i++){
        packet[i] = packetAux[i];
    }
    prevTries = flag;
    if(infoFlag) infoFlag = 0;
    else infoFlag = 1;
    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    if(showStatistics){
        printf("\n---STATISTICS---\n");
        double time = ((double) (clock() - start)) / CLOCKS_PER_SEC * 1000;
        printf("\nExecution time: %f miliseconds\n", time);

    }  
    sleep(1);
    printf("\n----LLCLOSE----\n");
    alarmCounter = 0;
    setAlarm = FALSE;
    printf("ROLE: %d\n", connectionParameters.role);

    if(connectionParameters.role == LlTx){
        
        unsigned char array[5];
        array[0] = FLAG;
        array[1] = A;
        array[2] = C_DISC;
        array[3] = A^C_DISC;
        array[4] = FLAG;

        while(alarmCount < connectionParameters.nRetransmissions){

            enum setState state = START;
            unsigned char b;
            int bytes;
            if(setAlarm == FALSE){
                bytes = write(fd, array, 5);
                alarm(timeout);
                alarmEnabled = TRUE;
                if (bytes < 0){
                    printf("Emissor: Failed to send DISC\n");
                }
                else{
                    printf("Emissor: Sent DISC\n");
                }
            }
            
            //receber DISC
            while (state != STOP)
            {
                int bytesR= read(fd, &b, 1);
                if(bytesR <= 0){      
                    break;
                }
                printf("Reading: %x\n", b); 

                state = stateMachineDISC(b, state);
            }
            if (state == STOP){
                break;
            }
        }
        if (alarmCounter >= connectionParameters.nRetransmissions){
            printf("Didn't receive DISC\n");
            printf("TIME-OUT\n");

            return -1;
        } 
        else printf("Emissor: Received DISC\n");

        //mandar UA
        unsigned char UA[5];
        UA[0] = FLAG;
        UA[1] = A;
        UA[2] = C_UA;
        UA[3] = BCC_UA;
        UA[4] = FLAG;
        int bytesUA = write(fd, UA, 5);
        sleep(1);
        if (bytesUA < 0){
            printf("Emissor: Failed to send UA\n");
        }
        else {
            printf("Emissor: Sending UA: %x,%x,%x,%x,%x\n", UA[0], UA[1], UA[2], UA[3], UA[4]);

            printf("Sent UA FRAME\n");
        }

    }     

    if(connectionParameters.role == LlRx){

        printf("Receiving DISC:\n");
        
        enum setState stateR = START;
        unsigned char a;
        
        while (stateR != STOP)
        {
            int b_rcv = read(fd, &a, 1);
            if (b_rcv > 0)
            {
                printf("Reading: %x\n", a);

                stateR = stateMachineDISC(a, stateR);

            }

        }
        printf("Receptor: Received DISC!\n");
    
        
        unsigned char array[5];
        array[0] = FLAG_SET;
        array[1] = A;
        array[2] = C_DISC;
        array[3] = A^C_DISC;
        array[4] = FLAG_SET;
        
        alarmCounter = 0;
        unsigned char ua_rcv[5] = {0};
        while(alarmCounter < connectionParameters.nRetransmissions){

            enum setState state = START;
            unsigned char d;
            int bytes;
            if(setAlarm == FALSE){
                bytes = write(fd, array, 5);
                alarm(timeout); // 3s para escrever
                setAlarm = TRUE;
                if (bytes < 0){
                    printf("Receptor: Failed to send DISC\n");
                }
                else{
                    printf("Receptor: Sent DISC\n");
                }
                
                printf("Receptor: Receiving UA\n");
            }

            while (state != STOP)
            {
                int bytesR = read(fd, &d, 1);
                if(bytesR <= 0){
                    break;
                }
                printf("Reading: %02x\n", d); 

                state = stateMachineUA(d, state);
                
            }
            if(state == STOP){
                break;
            }
        }      
        

    }
    
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 1;
}
