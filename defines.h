#define GPRS_TX 11  
#define GPRS_RX 10

#define MESSAGE_LENGTH 160

struct Zone {
  uint8_t dht_pin;
  uint8_t relay_pin;
  uint8_t target_temp;
};

typedef enum Opcode{
  k_op_unknown,
  k_op_heat_on,
  k_op_heat_off,
  k_op_all_off,
  k_op_status
};

struct Command {
  Opcode code;
  uint8_t zone;
  uint8_t arg;
};

Command UNKNOWN_COMMAND = {k_op_unknown, 0, 0};


