#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <SoftwareSerial.h>
#include <Homey.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "CRC16.h"

#define LED 2

//===Change values from here===
const char* ssid = "WIFI_SSID";
const char* password = "SUPER_SECRET_PASSWORD";
const char* hostName = "ESPP1Meter";
const bool outputOnSerial = false;
//===Change values to here===

// Vars to store meter readings
long mEVLT = 0; //Meter reading Electrics - consumption low tariff
long mEVHT = 0; //Meter reading Electrics - consumption high tariff
long mEOLT = 0; //Meter reading Electrics - return low tariff
long mEOHT = 0; //Meter reading Electrics - return high tariff
long mEAV = 0;  //Meter reading Electrics - Actual consumption
long mEAT = 0;  //Meter reading Electrics - Actual return

long mVL1 = 0;  //Meter reading Electrics - Voltage L1
long mIL1 = 0;  //Meter reading Electrics - Current L1
long mVL2 = 0;  //Meter reading Electrics - Voltage L2
long mIL2 = 0;  //Meter reading Electrics - Current L2
long mVL3 = 0;  //Meter reading Electrics - Voltage L3
long mIL3 = 0;  //Meter reading Electrics - Current L3

long mGAS = 0;    //Meter reading Gas
long prevGAS = 0;

#define MAXLINELENGTH 128 // longest normal line is 47 char (+3 for \r\n\0)
char telegram[MAXLINELENGTH];

#define SERIAL_RX     D5  // pin for SoftwareSerial RX
SoftwareSerial mySerial(SERIAL_RX, -1, true, MAXLINELENGTH); // (RX, TX. inverted, buffer)

unsigned int currentCRC=0;

void setup() {
  Homey.begin("P1 Smart Meter");
  Homey.setClass("other");
  Homey.addCapability("measure_power.consumed");
  Homey.addCapability("measure_power.produced");
  Homey.addCapability("meter_power.producedPeak");
  Homey.addCapability("meter_power.producedOffPeak");
  Homey.addCapability("meter_power.peak");
  Homey.addCapability("meter_power.offPeak");

  Homey.addCapability("measure_voltage.L1");
  Homey.addCapability("measure_current.L1");
  Homey.addCapability("measure_voltage.L2");
  Homey.addCapability("measure_current.L2");
  Homey.addCapability("measure_voltage.L3");
  Homey.addCapability("measure_current.L3");

  Homey.addCapability("meter_gas");

  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  mySerial.begin(115200);

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(hostName);

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void UpdateGas()
{
  //sends over the gas setting to Homey
  if(prevGAS!=mGAS)
  {
    Homey.setCapabilityValue("meter_gas", (float) (mGAS/1000.0));
    prevGAS=mGAS;
  }
}

void UpdateElectricity()
{
  Homey.setCapabilityValue("measure_power.consumed", (float) (mEAV*1.0));
  Homey.setCapabilityValue("measure_power.produced", (float) (mEAT*1.0));
  Homey.setCapabilityValue("meter_power.producedPeak", (float) (mEOHT*0.001));
  Homey.setCapabilityValue("meter_power.producedOffPeak", (float) (mEOLT*0.001));
  Homey.setCapabilityValue("meter_power.peak", (float) (mEVHT*0.001));
  Homey.setCapabilityValue("meter_power.offPeak", (float) (mEVLT*0.001));

  Homey.setCapabilityValue("measure_voltage.L1", (float) (mVL1*0.001));
  Homey.setCapabilityValue("measure_current.L1", (float) (mIL1*0.001));
  Homey.setCapabilityValue("measure_voltage.L2", (float) (mVL2*0.001));
  Homey.setCapabilityValue("measure_current.L2", (float) (mIL2*0.001));
  Homey.setCapabilityValue("measure_voltage.L3", (float) (mVL3*0.001));
  Homey.setCapabilityValue("measure_current.L3", (float) (mIL3*0.001));
}

bool isNumber(char* res, int len) {
  for (int i = 0; i < len; i++) {
    if (((res[i] < '0') || (res[i] > '9'))  && (res[i] != '.' && res[i] != 0)) {
      return false;
    }
  }
  return true;
}

int FindCharInArrayRev(char array[], char c, int len) {
  for (int i = len - 1; i >= 0; i--) {
    if (array[i] == c) {
      return i;
    }
  }
  return -1;
}

long getValidVal(long valNew, long valOld, long maxDiffer)
{
  //check if the incoming value is valid
  if(valOld > 0 && ((valNew - valOld > maxDiffer) && (valOld - valNew > maxDiffer)))
    return valOld;
  return valNew;
}

long getValue(char* buffer, int maxlen) {
  int s = FindCharInArrayRev(buffer, '(', maxlen - 2);
  if (s < 8) return 0;
  if (s > 32) s = 32;
  int l = FindCharInArrayRev(buffer, '*', maxlen - 2) - s - 1;
  if (l < 3) return 0;
  if (l > 12) return 0;
  char res[16];
  memset(res, 0, sizeof(res));

  if (strncpy(res, buffer + s + 1, l)) {
    if (isNumber(res, l)) {
      return (1000 * atof(res));
    }
  }
  return 0;
}

bool decodeTelegram(int len) {
  //need to check for start
  int startChar = FindCharInArrayRev(telegram, '/', len);
  int endChar = FindCharInArrayRev(telegram, '!', len);
  bool validCRCFound = false;
  if(startChar>=0)
  {
    //start found. Reset CRC calculation
    currentCRC=CRC16(0x0000,(unsigned char *) telegram+startChar, len-startChar);
    if(outputOnSerial)
    {
      for(int cnt=startChar; cnt<len-startChar;cnt++){
        Serial.print(telegram[cnt]);
      }
    }

  }
  else if(endChar>=0)
  {
    //add to crc calc
    currentCRC=CRC16(currentCRC,(unsigned char*)telegram+endChar, 1);
    char messageCRC[5];
    strncpy(messageCRC, telegram + endChar + 1, 4);
    messageCRC[4]=0; //thanks to HarmOtten (issue 5)
    if(outputOnSerial)
    {
      for(int cnt=0; cnt<len;cnt++)
        Serial.print(telegram[cnt]);
    }

    validCRCFound = (strtol(messageCRC, NULL, 16) == currentCRC);
    if(validCRCFound)
      Serial.println("\nVALID CRC FOUND!");
    else
      Serial.println("\n===INVALID CRC FOUND!===");
    currentCRC = 0;
  }
  else
  {
    currentCRC=CRC16(currentCRC, (unsigned char*)telegram, len);
    if(outputOnSerial)
    {
      for(int cnt=0; cnt<len;cnt++)
        Serial.print(telegram[cnt]);
    }
  }

  long val =0;
  long val2=0;
  // 1-0:1.8.1(000992.992*kWh)
  // 1-0:1.8.1 = Elektra verbruik laag tarief (DSMR v4.0)
  if (strncmp(telegram, "1-0:1.8.1", strlen("1-0:1.8.1")) == 0)
    mEVLT =  getValue(telegram, len);

  // 1-0:1.8.2(000560.157*kWh)
  // 1-0:1.8.2 = Elektra verbruik hoog tarief (DSMR v4.0)
  if (strncmp(telegram, "1-0:1.8.2", strlen("1-0:1.8.2")) == 0)
    mEVHT = getValue(telegram, len);

  // 1-0:2.8.1(000348.890*kWh)
  // 1-0:2.8.1 = Elektra opbrengst laag tarief (DSMR v4.0)
  if (strncmp(telegram, "1-0:2.8.1", strlen("1-0:2.8.1")) == 0)
    mEOLT = getValue(telegram, len);

  // 1-0:2.8.2(000859.885*kWh)
  // 1-0:2.8.2 = Elektra opbrengst hoog tarief (DSMR v4.0)
  if (strncmp(telegram, "1-0:2.8.2", strlen("1-0:2.8.2")) == 0)
    mEOHT = getValue(telegram, len);

  // 1-0:1.7.0(00.424*kW) Actueel verbruik
  // 1-0:2.7.0(00.000*kW) Actuele teruglevering
  // 1-0:1.7.x = Electricity consumption actual usage (DSMR v4.0)
  if (strncmp(telegram, "1-0:1.7.0", strlen("1-0:1.7.0")) == 0)
    mEAV = getValue(telegram, len);

  if (strncmp(telegram, "1-0:2.7.0", strlen("1-0:2.7.0")) == 0)
    mEAT = getValue(telegram, len);

  // 0-1:24.2.1(150531200000S)(00811.923*m3)
  // 0-1:24.2.1 = Gas (DSMR v4.0) on Kaifa MA105 meter
  if (strncmp(telegram, "0-1:24.2.1", strlen("0-1:24.2.1")) == 0)
    mGAS = getValue(telegram, len);

  // 1-0:32.7.0 (V)
  // 1-0:32.7.0 = Instantaan voltage L1 (DSMR v4.0)
  if (strncmp(telegram, "1-0:32.7.0", strlen("1-0:32.7.0")) == 0)
    mVL1 = getValue(telegram, len);

  // 1-0:52.7.0 (V)
  // 1-0:52.7.0 = Instantaan voltage L2 (DSMR v4.0)
  if (strncmp(telegram, "1-0:52.7.0", strlen("1-0:52.7.0")) == 0)
    mVL2 = getValue(telegram, len);

  // 1-0:72.7.0 (V)
  // 1-0:72.7.0 = Instantaan voltage L3 (DSMR v4.0)
  if (strncmp(telegram, "1-0:72.7.0", strlen("1-0:72.7.0")) == 0)
    mVL3 = getValue(telegram, len);

  // 1-0:31.7.0 (I)
  // 1-0:31.7.0 = Instantaan current L1 (DSMR v4.0)
  if (strncmp(telegram, "1-0:31.7.0", strlen("1-0:31.7.0")) == 0)
    mIL1 = getValue(telegram, len);

  // 1-0:51.7.0 (I)
  // 1-0:51.7.0 = Instantaan current L2 (DSMR v4.0)
  if (strncmp(telegram, "1-0:51.7.0", strlen("1-0:51.7.0")) == 0)
    mIL2 = getValue(telegram, len);

  // 1-0:71.7.0 (I)
  // 1-0:71.7.0 = Instantaan current L3 (DSMR v4.0)
  if (strncmp(telegram, "1-0:71.7.0", strlen("1-0:71.7.0")) == 0)
    mIL3 = getValue(telegram, len);

  return validCRCFound;
}

void readTelegram() {
  if (mySerial.available()) {
    memset(telegram, 0, sizeof(telegram));
    while (mySerial.available()) {
      int len = mySerial.readBytesUntil('\n', telegram, MAXLINELENGTH);
      telegram[len] = '\n';
      telegram[len+1] = 0;
      yield();

      if(decodeTelegram(len+1))
      {
         UpdateElectricity();
         UpdateGas();
      }
    }
  }
}

void wifi()
{
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      digitalWrite(LED,!digitalRead(LED));
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      //Print IP address
      Serial.print("Connected to WiFi! (");
      Serial.print(WiFi.localIP());
      Serial.println(")");
    }
  }
  digitalWrite(LED, 1);
}

void loop() {
  wifi();

  readTelegram();
  Homey.loop();

  ArduinoOTA.handle();
}
