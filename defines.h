#define GPRS_TX 11  
#define GPRS_RX 10

#define IN_MESSAGE_LENGTH 20
#define OUT_MESSAGE_LENGTH 100

#define ZONES 2

#define LOOP_DELAY 2000
#define MAX_ON_HOURS 4
#define MAX_ON_LOOPS (long)4 * 60 * 60 * 1000 / LOOP_DELAY

typedef struct Zone {
  uint8_t dht_pin;
  uint8_t relay_pin;
  uint8_t target_temp;
  uint8_t last_temp;
  uint8_t last_hum;
  long on_loops_count;
  int last_read_result;
};

typedef enum Opcode{
  k_op_unknown,
  k_op_heat_on,
  k_op_heat_off,
  k_op_all_off,
  k_op_status
};

typedef struct Command {
  Opcode code;
  uint8_t zone; //1-based, 0 if none/all
  uint8_t arg;  //e.g. temp
};

Command UNKNOWN_COMMAND = {k_op_unknown, 0, 0};


