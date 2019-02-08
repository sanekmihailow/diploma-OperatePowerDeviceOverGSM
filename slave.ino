 // This file is part of program.
 // 
 //                        ***Important***
 // This file contains instructions for sending control signals from Arduino 
 // (which is connected to the uart with the Relay module) to device.
 // 
 // Copyright (c) 2019 Mikhailov Alexandr <sanekmihailow@gmail.com>
 // This program is free software: you can redistribute it and/or modify
 // it under the terms of the GNU General Public License version 2 as published by
 // the Free Software Foundation.
 // 
 // This program is distributed in the hope that it will be useful,
 // but WITHOUT ANY WARRANTY; without even the implied warranty of
 // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 // GNU General Public License for more details.
 // 
 // You should have received a copy of the GNU General Public License
 // along with program. If not, see <https://www.gnu.org/licenses/>.

#include <string.h>

String SERVER_SSID = "OMGServer"; // в будущем можно через eeprom 
String SERVER_PASS = "1qaz2wsx";
String SERVER_IP = "192.168.3.1";
String SERVER_PORT = "5005";

String AT_CONNECT_TO_AP = "AT+CWJAP";
String AT_CIPMUX = "AT+CIPMUX";
String AT_CIPSTART = "AT+CIPSTART";
String AT_CIPSEND = "AT+CIPSEND";
String AT_CIPSERVER = "AT+CIPSERVER";
String AT_CIPCLOSE = "AT+CIPCLOSE";
String AT_CWMODE = "AT+CWMODE";
String AT_CWLAP = "AT+CWLAP";

String CMD_POWER_ON = "pon";
String CMD_POWER_OFF = "pof";
String CMD_POWER_RST = "rst";

String CIPSERVER_PORT = "5005";

String CMD_STATUS_OK = "+SLAVE0:OK"; // в будущем параметрировать адрес (разные номера Slave: Slave1. Slave2... etc.)
String CMD_STATUS_READY = "+SLAVE0:READY";

// For Debug. comment it if you don't use
/* Коды ошибок */
int ERR_NO_ESP = 1;
int ERR_NO_CW = 2;
int ERR_NO_AP = 3;
int ERR_NO_MUX = 4;
int ERR_NO_TCP = 5;
int ERR_NO_PREPARE = 6;
int ERR_NO_SEND = 7;
int ERR_NO_CLOSE = 8;
int ERR_NO_REMUX = 9;
int ERR_NO_SERVICE = 10;
int ERR_IPD_COL = 11;
//

int latestError = -1; // в будущем будет скидывать себя в eeprom если что-то упадёт

int RELAY_POWER = 2;
int RELAY_RESET = 3;

unsigned long HOLD_TIME_POWER_ON = 500;
unsigned long HOLD_TIME_POWER_OFF = 3000;
unsigned long HOLD_TIME_RESET = 1000;

// For Debug. comment it if you don't use
int ERR_PIN1 = 4;
int ERR_PIN2 = 5;
int ERR_PIN3 = 6;
int ERR_PIN4 = 7;
//

int MIN_IPD_LEN = 10; // +IPD,X,Y:Z

unsigned long DELAY_SCAN = 8000;
unsigned long DELAY_TCP_CONN = 5000;

//output on Monitor
// bool alarmToMonitor() {
//   Serial.print("ERROR: " + latestError);
// }

//функция объявляет строчку
bool checkAnswer(String expected)
{
  int tries = 5;

  while (tries) {
    delay(1000);
    if (Serial.available() > 0) {
      String answer = Serial.readString();
      if (answer.indexOf(expected) >= 0) {
        return true;
      }
    }
    tries--;
  }
  return false;
}

bool checkAnswerOK()
{
  return checkAnswer("OK");
}

bool isESPAlive()
{
  Serial.print("AT\r\n");
  return checkAnswerOK();
}

bool connectToServerAP()
{
  Serial.print(AT_CONNECT_TO_AP + "=\"" + SERVER_SSID + "\",\"" + SERVER_PASS + "\"\r\n");
  return checkAnswerOK();
}

bool setOpMode(String mode)
{
  Serial.print(AT_CWMODE + "=" + mode + "\r\n");
  return checkAnswerOK();
}

void scanWifi()
{
  Serial.print(AT_CWLAP + "\r\n");
  delay(DELAY_SCAN);
}

bool setMuxMode(String mux_mode)
{
  Serial.print(AT_CIPMUX + "=" + mux_mode + "\r\n");
  return checkAnswerOK();
}

bool connectToTCP(String ip, String port)
{
  Serial.print(AT_CIPSTART + "=\"TCP\",\"" + ip + "\"," + port + "\r\n");
  delay(DELAY_TCP_CONN);
  return checkAnswerOK();
}

bool prepareToSendOverTCP(String message)
{
  Serial.print(AT_CIPSEND + "=" + message.length() + "\r\n");
  return checkAnswer(">");
}

int sendOverTCP(String message)
{
  Serial.print(message);
  return checkAnswerOK() ? 0 : ERR_NO_SEND;
}

bool closeTCP()
{
  Serial.print(AT_CIPCLOSE + "\r\n");
  return checkAnswerOK();
}

int sendReadyToServer()
{
  // For Debug. comment it if you don't use
  if (!setMuxMode("0")) {
    return ERR_NO_MUX;
  }
  if (!connectToTCP(SERVER_IP, SERVER_PORT)) {
    return ERR_NO_TCP;
  }
  if (!prepareToSendOverTCP(CMD_STATUS_READY)) {
    return ERR_NO_PREPARE;
  }
  //
  return sendOverTCP(CMD_STATUS_READY);
}

bool initTCPServer()
{
  Serial.print(AT_CIPSERVER + "=1," + CIPSERVER_PORT + "\r\n");
  return checkAnswerOK();
}

String getNewIPD()
{
  String answer = String(20);
  if (Serial.available() >= MIN_IPD_LEN) {
    answer = Serial.readString();
    if (answer.indexOf("IPD") >= 0) {
      return answer;
    } else {
      latestError = ERR_IPD_COL;
    }
  }
  return "";
}

void commandClick(int addr, unsigned long hold_time)
{
  digitalWrite(addr, LOW);
  delay(hold_time);
  digitalWrite(addr, HIGH);
}

void commandPowerOn()
{
  commandClick(RELAY_POWER, HOLD_TIME_POWER_ON);
}

void commandPowerOff()
{
  commandClick(RELAY_POWER, HOLD_TIME_POWER_OFF);
}

void commandReset()
{
  commandClick(RELAY_RESET, HOLD_TIME_RESET);
}

int parseIPD(String message)
{
  int data_idx;
  String data;

  data_idx = message.indexOf(":");
  if (data_idx < 0) {
    return -1;
  }
  data = message.substring(data_idx + 1);
  if (data.length() <= 0) {
    return -1;
  }
  if (data.indexOf(CMD_POWER_ON) == 0) {
    commandPowerOn();
    return 1;
  } else if (data.indexOf(CMD_POWER_OFF) == 0) {
    commandPowerOff();
    return 1;
  } else if (data.indexOf(CMD_POWER_RST) == 0) {
    commandReset();
    return 1;
  }
  return -1;
}
// For Debug. comment it if you don't use
//Example: 9/2=4(1)/2=2(0)/2=1  1001 = 1 + 8 = 9
void displayErrorCode(int code)
{
  digitalWrite(ERR_PIN1, bitRead(code, 0));
  digitalWrite(ERR_PIN2, bitRead(code, 1));
  digitalWrite(ERR_PIN3, bitRead(code, 2));
  digitalWrite(ERR_PIN4, bitRead(code, 3));
}
//

void setup()
{
  int rc = 0;

  pinMode(RELAY_POWER, OUTPUT);
  pinMode(RELAY_RESET, OUTPUT);
  digitalWrite(RELAY_POWER, HIGH);
  digitalWrite(RELAY_RESET, HIGH);

  // For Debug. comment it if you don't use
  pinMode(ERR_PIN1, OUTPUT);
  pinMode(ERR_PIN2, OUTPUT);
  pinMode(ERR_PIN3, OUTPUT);
  pinMode(ERR_PIN4, OUTPUT);
  //
  
  Serial.begin(115200);
  delay(3000);
  // For Debug. comment it if you don't use
  if (!isESPAlive()) {
    latestError = ERR_NO_ESP;
    return;
  }
  if (!setOpMode("1")) {
    latestError = ERR_NO_CW;
    return;
  }
  scanWifi();
  if (!connectToServerAP()) {
    latestError = ERR_NO_AP;
    return;
  }
  rc = sendReadyToServer();
  if (rc > 0) {
    latestError = rc;
    return;
  }
  if (!closeTCP()) {
    latestError = ERR_NO_CLOSE;
    return;
  }
  if (!setMuxMode("1")) {
    latestError = ERR_NO_REMUX;
    return;
  }
  if (!initTCPServer()) {
    latestError = ERR_NO_SERVICE;
    return;
  }
  //
}

unsigned long blink_tm = 0;
int blink_code = 0;
String ipd = String(20);

void loop() {
  unsigned long check_tm = millis();
  // millis каждые 50 дней сбрасывается в 0. Защита от этого
  if (blink_tm > check_tm) {
    blink_tm = check_tm;
  }

  ipd.remove(0);

  if (latestError != -1) {
    displayErrorCode(latestError);
    return;
  }

  if (check_tm - blink_tm > 800) {
    blink_tm = check_tm;
    blink_code = blink_code == 8 ? 0 : 8;
    displayErrorCode(blink_code);
  }

  ipd = getNewIPD();
  if (ipd.length() == 0) {
    return;
  }
  parseIPD(ipd);
}
