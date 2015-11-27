#include <LiquidCrystal.h>

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


GPRS gprs(2, 3, 10, 11, 9600);
int messageIndex = 0;         // номер сообщения в памяти сим-карты
char message[MESSAGE_LENGTH]; // текст сообщения
char phone[16];               // номер, с которого пришло сообщение
char datetime[24];            // дата отправки сообщения
LiquidCrystal lcd(A5, A4, A3, A2, A1, A0);

struct Zone zones[ZONES];
DHT11 * dhts[ZONES];

//status stuff
bool blink;

void setupLCD() {
    lcd.begin(20, 4);
}

void setupGPRS() {
  gprs.powerUpDown();
  lcd.print(".");
  
  Serial.println("initializing GPRS");
  while (gprs.init() != true) {
    lcd.print(".");
    Serial.println("error initializing GPRS");
    delay(1000);
  }
}


void setupZones() {
  zones[0] = {4, 5, 0, 0, 0};
  zones[1] = {6, 7, 0, 0, 0};
  
  for (int z = 0; z < ZONES; z++) {
    Zone zone = zones[z];
    dhts[z] = new DHT11(zone.dht_pin);
    dhts[z]->begin();
    pinMode(zone.relay_pin, OUTPUT);
  }
}

void setupSerialAndWait() {
  Serial.begin(9600);

  // while (!Serial) {
  //   delay(1000);
  // }
}

bool checkSMS() {
  
  messageIndex = gprs.isSMSunread();
   
  Serial.print("messageIndex: ");
  Serial.println(messageIndex);

   if (messageIndex > 0) {
     if (gprs.readSMS(messageIndex, message, MESSAGE_LENGTH, phone, datetime)) {
        for (char *p = message; *p; ++p) {
          *p = tolower(*p);
        }

        // выводим номер, с которого пришло смс
        Serial.print("From number: ");
        Serial.println(phone);

        // выводим дату, когда пришло смс
        Serial.print("Datetime: ");
        Serial.println(datetime);

        // выводим текст сообщения
        Serial.print("Recieved Message: ");
        Serial.println(message); 

        Command cmdFromSms = parseSms(message);
        printCmd(cmdFromSms);
        processCommand(cmdFromSms);
      } else {
        Serial.println("did not read message");
      }

     
     gprs.deleteSMS(messageIndex);
     return true;
   } else {
     return false;
   }


}

// void test() {
//   Command cmd;

//   cmd = parseSms("heat 1 25");
//   printCmd(cmd);

//   cmd = parseSms("heat 1");
//   printCmd(cmd);

//   cmd = parseSms("off 1");
//   printCmd(cmd);

//   cmd = parseSms("off");
//   printCmd(cmd);

//   cmd = parseSms("status");
//   printCmd(cmd);

// //malformed messages
//   cmd = parseSms("abracadabra");
//   printCmd(cmd);

//   cmd = parseSms("heat");
//   printCmd(cmd);

//   cmd = parseSms("heat lol");
//   printCmd(cmd);
// }

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

void status(char * buf) {
  int offset = 0;
  for (int i = 0; i < ZONES; i++) {
    Zone * z = &zones[i];

    offset += sprintf(
      buf + offset, 
      "Zone %d: T=%dC; H=%d%% %s\n", 
      (i + 1), 
      z->last_temp, 
      z->last_hum, 
      (z->target_temp > 0 ? "ON" : "OFF"));
  }
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

void processCommand(Command cmd) {
  Zone * z = NULL;
  char reply[160];

  if (cmd.zone > ZONES) {
    sprintf(reply, "Specify a zone between 1 and %d", ZONES);
    gprs.sendSMS(phone, reply);
    return;
  }else if (cmd.zone > 0) {
    z = &zones[cmd.zone - 1];
  }

  switch (cmd.code) {
      case k_op_heat_on:
        if (z == NULL) {
          sprintf(reply, "Specify a zone between 1 and %d", ZONES);
        } else  if (cmd.arg <= 0 || cmd.arg > 30) { 
          sprintf(reply, "Specify temperature between %d and %d C", 1, 30);
        } else {
          z->target_temp = cmd.arg;
          sprintf(reply, "Heating zone %d up to %d C", (cmd.zone), cmd.arg);
        }
        break;
      case k_op_heat_off:
        if (z == NULL) {
          sprintf(reply, "Specify a zone between 1 and %d", ZONES);
        } else {
          z->target_temp = 0;
          sprintf(reply, "Stopping heating in zone %d", (cmd.zone));
        }
        break;
      case k_op_all_off:
        for (int i = 0; i < ZONES; i++) {
          (&zones[i])->target_temp = 0;
        }
        sprintf(reply, "Stopping heating in all zones");
        break;
      case k_op_status:
        status(reply);
        break;
      case k_op_unknown:
      default:
        sprintf(reply, "Unknown command");
        break;
  };

  gprs.sendSMS(phone, reply);
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
        zone->last_temp = 0;
        zone->last_hum = 0;
        break;
      // превышение времени ожидания
      case DHT_ERROR_TIMEOUT:
        Serial.println("Time out error");
        zone->last_temp = 0;
        zone->last_hum = 0;
        break;
      // неизвестная ошибка
      default:
        Serial.println("Unknown error");
        zone->last_temp = 0;
        zone->last_hum = 0;
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
    
    zone->last_read_result = check;

    return check;
}

void printZoneLCD(uint8_t index, Zone * zone) {
  lcd.setCursor(0, index + 2);
  lcd.print(index + 1);
  lcd.print(": ");
  if (zone->last_read_result != DHT_OK) {
     lcd.print("\xED"); 
  } else if (zone->target_temp > 0) {
    if (zone->target_temp > zone->last_temp) {
      lcd.print("\x94"); 
    } else {
      lcd.print("\x2F"); 
    }
  } else {
    lcd.print("\x8A");
  } 

  lcd.print(" t=");
  lcd.print(zone->last_temp);
  lcd.print("\x99""C ");

  lcd.setCursor(12, index+2);
  lcd.print("H=");
  lcd.print(zone->last_hum);
  lcd.print("%");

  lcd.print("   ");
}

void setup() {
  setupLCD();
  lcd.setCursor(0, 0);
  lcd.print("       Hello!");
  lcd.setCursor(0, 1);
  lcd.print("Zones ...");
  setupZones();
  lcd.print(" OK");

  lcd.setCursor(0, 2);
  lcd.print("Serial ...");
  setupSerialAndWait();
  lcd.print(" OK");

  lcd.setCursor(0, 3);
  lcd.print("GSM .");
  setupGPRS();
  lcd.print(" OK");

  lcd.clear();
}

void loop() {

 Serial.print("signal: ");
 byte strength = gprs.getSignalStrength();
 Serial.println(strength);

 bool sms = checkSMS();
  
 blink = !blink;

 lcd.setCursor(0, 0);
 lcd.print(blink ? "\x92" : "\x93");
 lcd.print(" ");

 if (strength == 99) {
   lcd.print("?");
 } else if (strength >= 20) {
   lcd.print("\x83");
 } else if (strength >= 15) {
   lcd.print("\x82");
 } else if (strength >= 10) {
   lcd.print("\x81");
 } else {
   lcd.print("\x80");
 }

 if (sms) {
   lcd.print(" \xED");
 } else {
   lcd.print("  ");
 }

 for (int z = 0; z < ZONES; z++) {
   int updateResult = updateZone(&zones[z], dhts[z]);    
   printZoneLCD(z, &zones[z]);
 }

 char buf[160];
 status(buf);
 Serial.print(buf);

 delay(5000);
}


