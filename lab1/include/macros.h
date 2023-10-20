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
#define C_REJ0(l) ((l << 7) | 0x01)
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