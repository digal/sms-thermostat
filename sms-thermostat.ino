#include <SoftwareSerial.h>
#include <TroykaDHT11.h>
#include <GPRS_Shield_Arduino.h>
#include <sim900.h>

#include "defines.h"

const char* k_cmd_heat_on = "heat";
const char* k_cmd_heat_off = "off";
const char* k_cmd_status = "status";
const char* k_delimiters = " ";
const uint8_t k_default_temp = 20;


GPRS gprs(11, 10, 1200);
int messageIndex = 0;         // номер сообщения в памяти сим-карты
char message[MESSAGE_LENGTH]; // текст сообщения
char phone[16];               // номер, с которого пришло сообщение
char datetime[24];            // дата отправки сообщения

struct Zone zones[2];
DHT11 * dhts[2];

void setup() {
  zones[0] = {4, 5, 0, 0, 0, };
  zones[1] = {6, 7, 0, 0, 0};
  
  for (int z = 0; z < 2; z++) {
    Zone zone = zones[z];
    dhts[z] = new DHT11(zone.dht_pin);
    dhts[z]->begin();
    pinMode(zone.relay_pin, OUTPUT);
  }
  
  Serial.begin(9600);

  while (!Serial) {
    delay(1000);
  }
  
//  delay(1000);

//  test();
}

void loop() {
  for (int z = 0; z < 2; z++) {
    Serial.print("Zone ");
    Serial.print(z);
    Serial.print(":\n  ");
    printZone(&zones[z]);
    int updateResult = updateZone(&zones[z], dhts[z]);    
    
  }
  
  delay(5000);
   
  
   // messageIndex = gprs.isSMSunread();
   
   // if (messageIndex > 0) {
   //   gprs.readSMS(messageIndex, message, MESSAGE_LENGTH, phone, datetime);

   //   // выводим номер, с которого пришло смс
   //   Serial.print("From number: ");
   //   Serial.println(phone);
  
   //   // выводим дату, когда пришло смс
   //   Serial.print("Datetime: ");
   //   Serial.println(datetime);
  
   //   // выводим текст сообщения
   //   Serial.print("Recieved Message: ");
   //   Serial.println(message); 
   // }
}

void test() {
  Command cmd;

  cmd = parseSms("heat 1 25");
  printCmd(cmd);

  cmd = parseSms("heat 1");
  printCmd(cmd);

  cmd = parseSms("off 1");
  printCmd(cmd);

  cmd = parseSms("off");
  printCmd(cmd);

  cmd = parseSms("status");
  printCmd(cmd);

//malformed messages
  cmd = parseSms("abracadabra");
  printCmd(cmd);

  cmd = parseSms("heat");
  printCmd(cmd);

  cmd = parseSms("heat lol");
  printCmd(cmd);
}

//SMS stuff
Command parseSms(char* message) {  
  char * cmdStr = strtok(message, k_delimiters);
  
  //HEAT ON
  if (sizeof(cmdStr) >= sizeof(k_cmd_heat_on)
      && memcmp(cmdStr, k_cmd_heat_on, sizeof(k_cmd_heat_on)) == 0) {
        
    uint8_t zone = 0;
    uint8_t temp = k_default_temp;  
    char * zoneStr = strtok(NULL, k_delimiters);
    
    if (zoneStr) {
      zone = atoi(zoneStr);    
      char * tempStr = strtok(NULL, k_delimiters);
      if (tempStr) {
        //TODO: zone and temp bounds
         temp = atoi(tempStr); 
      }
      
      return {k_op_heat_on, zone, temp};
    } else {
      return UNKNOWN_COMMAND; 
    }
  } else if (sizeof(cmdStr) >= sizeof(k_cmd_heat_off)
      && memcmp(cmdStr, k_cmd_heat_off, sizeof(k_cmd_heat_off)) == 0) {
        
      uint8_t zone = 0;
      char * zoneStr = strtok(NULL, k_delimiters);
      if (zoneStr) {
        zone = atoi(zoneStr);    
        return {k_op_heat_off, zone, 0};      
      } else {
        return {k_op_all_off, 0, 0};      
      }
  } else if (sizeof(cmdStr) >= sizeof(k_cmd_status)
      && memcmp(cmdStr, k_cmd_status, sizeof(k_cmd_status)) == 0) {
      return {k_op_status, 0, 0};
  } else {
    return UNKNOWN_COMMAND;
  }  
}

void printZone(Zone * z) {
   // выводим показания влажности и температуры
  Serial.print("Target temp = ");
  Serial.print(z->target_temp);
  Serial.print("C \t");
  Serial.print("Temperature = ");
  Serial.print(z->last_temp);
  Serial.print("C \t");
  Serial.print("Humidity = ");
  Serial.print(z->last_hum);
  Serial.println("%");
 
}

void printCmd(Command cmd) {
  switch (cmd.code) {
      case k_op_heat_on:
        Serial.print("Heat on for ");
        Serial.print(cmd.arg);
        Serial.print(" in zone ");
        Serial.println(cmd.zone);
        break;
      case k_op_heat_off:
        Serial.print("Heat off in zone ");
        Serial.println(cmd.zone);
        break;
      case k_op_all_off:
        Serial.println("Heat off in all zones");
        break;
      case k_op_status:
        Serial.println("Status");
        break;
      case k_op_unknown:
      default:
        Serial.println("Uknown command");
        break;
  };
}

int updateZone(Zone * zone, DHT11 * dhtP) {
    int check;
     
    DHT11 dht = *dhtP;
    // считывание данных с датчика DHT11
    check = dht.read();
    switch (check) {
      // всё OK
      case DHT_OK:
        zone->last_temp = dht.getTemperatureC();
        zone->last_hum = dht.getHumidity();
        break;
      // ошибка контрольной суммы
      case DHT_ERROR_CHECKSUM:
        Serial.println("Checksum error");
        break;
      // превышение времени ожидания
      case DHT_ERROR_TIMEOUT:
        Serial.println("Time out error");
        break;
      // неизвестная ошибка
      default:
        Serial.println("Unknown error");
        break;
    }     
    
    //есть актуальная температура, 
    //установлена целевая температура
    //актуальная температура меньше целевой
    if (check == DHT_OK
        && zone->target_temp > 0
        && zone->target_temp > zone->last_temp) {
        digitalWrite(zone->relay_pin, HIGH);
    } else {
        digitalWrite(zone->relay_pin, LOW);
    }

    return check;
}


