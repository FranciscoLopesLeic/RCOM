#define FLAG_SET 0X7E
#define A 0x03
#define A_RECEIVER 0x01
#define C_SET 0x03
#define C_UA 0x07
#define C_DISC 0x0B
#define BCC_SET (A^C_SET)
#define BCC_UA (A^C_UA)
#define TRUE 1
#define FALSE 0

enum set_state{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    STOP
};

enum packet_state{
    START_PACKET,
    FLAG_RCV_PACKET,
    A_RCV_PACKET,
    C_RCV_PACKET,
    BCC_OK_PACKET,
    DATA_RCV_PACKET,
    STOP_PACKET
};




