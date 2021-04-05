/*
  ccs811basic.ino - Demo sketch printing results of the CCS811 digital gas sensor for monitoring indoor air quality from ams.
  Created by Maarten Pennings 2017 Dec 11
*/

#include "config.h"

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>

#include <Wire.h>    // I2C library
#include <ccs811.h>  // CCS811 library
#include <Adafruit_GFX.h>    // Core graphics library
#include <ILI9341_SPI.h>
#include <MiniGrafx.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include "ArialRounded.h"

// Wiring for ESP8266 NodeMCU boards: VDD to 3V3, GND to GND, SDA to D2, SCL to D1, nWAKE to D3 (or GND)
CCS811 ccs811(CCS811_WAKE_PIN); // nWAKE on D3

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme;

#define MINI_BLACK 0
#define MINI_WHITE 1
#define MINI_YELLOW 2
#define MINI_BLUE 3
#define MINI_GREEN 4
#define MINI_RED 5
#define MINI_GREY 6
#define MINI_DARKGREY 7

// defines the colors usable in the paletted 16 color frame buffer
uint16_t palette[] = {ILI9341_BLACK,     // 0
                      ILI9341_WHITE,     // 1
                      ILI9341_YELLOW,    // 2
                      ILI9341_BLUE,      // 3
                      ILI9341_GREEN,     // 4
                      ILI9341_RED,       // 5
                      ILI9341_LIGHTGREY, // 6
                      ILI9341_DARKGREY   // 7
                     }; //8

int SCREEN_WIDTH = 240;
int SCREEN_HEIGHT = 320;

int BITS_PER_PIXEL = 4; // 2^4 =  8 colors

ILI9341_SPI tft = ILI9341_SPI(TFT_CS, TFT_DC, TFT_RST);
MiniGrafx gfx = MiniGrafx(&tft, BITS_PER_PIXEL, palette, SCREEN_WIDTH, SCREEN_HEIGHT);
simpleDSTadjust dstAdjusted(StartRule, EndRule);

time_t dstOffset = 0;

void updateData();
void drawProgress(uint8_t percentage, String text);
void drawTime();
void drawWifiQuality();
String getTime(time_t *timestamp);

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  //Manual Wifi
  Serial.print("Connecting to WiFi ");
  Serial.print(WIFI_SSID);
  Serial.print("/");
  Serial.println(WIFI_PASS);
  WiFi.mode(WIFI_STA);
  //WiFi.hostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID,WIFI_PASS);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (i>80) i=0;
    drawProgress(i,"Connecting to WiFi '" + String(WIFI_SSID) + "'");
    i+=10;
    Serial.print(".");
  }
  drawProgress(100,"Connected to WiFi '" + String(WIFI_SSID) + "'");
  Serial.print("Connected...");
}

void setup() {
  // Enable serial
  Serial.begin(115200);
  Serial.println("");
  Serial.println("setup: Starting CCS811 basic demo");
  Serial.print("setup: ccs811 lib  version: "); Serial.println(CCS811_VERSION);

  // switch TFT backlight on
  pinMode(TFT_LED, OUTPUT);
  digitalWrite(TFT_LED, LOW);    // HIGH to Turn on;

  gfx.init();
  gfx.fillBuffer(MINI_BLACK);
  gfx.setRotation(0);
  gfx.commit();

  connectWifi();

  Wire.begin(I2C_SDA, I2C_SCL, 100000);

  // Enable CCS811
  ccs811.set_i2cdelay(50); // Needed for ESP8266 because it doesn't handle I2C clock stretch correctly
  bool ok = ccs811.begin();
  if ( !ok ) Serial.println("setup: CCS811 begin FAILED");

  // Print CCS811 versions
  Serial.print("setup: hardware    version: "); Serial.println(ccs811.hardware_version(), HEX);
  Serial.print("setup: bootloader  version: "); Serial.println(ccs811.bootloader_version(), HEX);
  Serial.print("setup: application version: "); Serial.println(ccs811.application_version(), HEX);


  // Start measuring
  ok = ccs811.start(CCS811_MODE_1SEC);
  if ( !ok ) {
    Serial.println("setup: CCS811 start FAILED");
  }

  // default settings
  // (you can also pass in a Wire library object like &Wire2)
  ok = bme.begin(0x76, &Wire);
  if (!ok) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
  }
  updateData();

}

uint16_t eco2, etvoc, errstat, raw;
uint16_t temperature, pressure, humidity;

void loop() {
  gfx.fillBuffer(MINI_BLACK);
  drawTime();
  drawWifiQuality();
  
  // Read CCS811
  ccs811.read(&eco2, &etvoc, &errstat, &raw);
  printCCS811();
  drawCCS811();
  readBME280(&temperature, &pressure, &humidity);
  printBME280();
  drawBME280();
  
  gfx.commit();
  // Wait
  delay(1000);
}

// Update the internet based information and update screen
void updateData() {

  gfx.fillBuffer(MINI_BLACK);
  gfx.setFont(ArialRoundedMTBold_14);

  drawProgress(10, "Updating time...");
  configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);
  while(!time(nullptr)) {
    Serial.print("#");
    delay(100);
  }
  // calculate for time calculation how much the dst class adds.
  dstOffset = UTC_OFFSET * 3600 + dstAdjusted.time(nullptr) - time(nullptr);
  Serial.printf("Time difference for DST: %d", dstOffset);
}

// Progress bar helper
void drawProgress(uint8_t percentage, String text) {
  gfx.fillBuffer(MINI_BLACK);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setColor(MINI_YELLOW);

  gfx.drawString(120, 146, text);
  gfx.setColor(MINI_WHITE);
  gfx.drawRect(10, 168, 240 - 20, 15);
  gfx.setColor(MINI_BLUE);
  gfx.fillRect(12, 170, 216 * percentage / 100, 11);

  gfx.commit();
}
// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if(dbm <= -100) {
      return 0;
  } else if(dbm >= -50) {
      return 100;
  } else {
      return 2 * (dbm + 100);
  }
}

void drawWifiQuality() {
  int8_t quality = getWifiQuality();
  gfx.setColor(MINI_WHITE);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);  
  //gfx.drawString(228, 6, String(quality) + "%");
  for (int8_t i = 0; i < 4; i++) {
    for (int8_t j = 0; j < 2 * (i + 1); j++) {
      if (quality > i * 25 || j == 0) {
        gfx.setPixel(230 + 2 * i, 18 - j);
      }
    }
  }
}

// draws the clock
void drawTime() {

  char time_str[11];
  char *dstAbbrev;
  time_t now = dstAdjusted.time(&dstAbbrev);
  struct tm * timeinfo = localtime (&now);

  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setColor(MINI_WHITE);
  String date = ctime(&now);
  date = date.substring(0,11) + String(1900 + timeinfo->tm_year);
  gfx.drawString(20, 6, date);

  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  if (IS_STYLE_12HR) {
    int hour = (timeinfo->tm_hour+11)%12+1;  // take care of noon and midnight
    sprintf(time_str, "%2d:%02d:%02d\n",hour, timeinfo->tm_min, timeinfo->tm_sec);
    gfx.drawString(160, 6, time_str);
  } else {
    sprintf(time_str, "%02d:%02d:%02d\n",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    gfx.drawString(160, 6, time_str);
  }
}

void drawCCS811() {
  int col;
  gfx.setFont(ArialMT_Plain_24);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setColor(MINI_WHITE);
  //  gfx.drawString(120, 2, "CO2");
  if ( errstat == CCS811_ERRSTAT_OK ) {

    if (eco2 < 600) col=MINI_GREEN; 
    else if (eco2 < 800) col=MINI_YELLOW;
    else col=MINI_RED;
    drawCO2(col, "CO²", String(eco2), "ppm");
    //drawLabelValueUnit(0, MINI_GREEN, "VOC", String(etvoc), "ppb");
  }
}

void drawBME280() {
  gfx.setFont(ArialMT_Plain_24);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setColor(MINI_WHITE);
  //  gfx.drawString(120, 2, "CO2");
  if ( errstat == CCS811_ERRSTAT_OK ) {

    //drawLabelValueUnit(1, MINI_GREEN, "Pres", String(pressure), "hPa");
    drawLabelValueUnit(0, MINI_GREEN, "Temperature", String(temperature), "°C");
    drawLabelValueUnit(1, MINI_GREEN, "Humidity", String(humidity), "%");
  }
}

void drawLabelValueUnit(uint8_t line, uint16_t color, String label, String value, String unit) {
  const uint8_t labelY = 210;
  const uint8_t valueY = 230;
  const uint8_t unitY = 230;

  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(60+line*120, labelY, label);
  gfx.setFont(ArialRoundedMTBold_36);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(40+line*120, valueY, value);
  gfx.setFont(ArialRoundedMTBold_36);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(80+line*120, unitY, unit);
}

void drawCO2(uint16_t color, String label, String value, String unit) {
  const uint8_t labelY = 80;
  const uint8_t valueY = 100;
  const uint8_t unitY = 150;

  gfx.setColor(color);
  for(int i=0; i < 10; i++) gfx.drawCircle(SCREEN_WIDTH/2, valueY+20, 60 + i);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(SCREEN_WIDTH/2, labelY, label);
  gfx.setFont(ArialRoundedMTBold_36);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(SCREEN_WIDTH/2, valueY, value);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setColor(MINI_WHITE);
  gfx.drawString(SCREEN_WIDTH/2, unitY, unit);
}

void printCCS811() {
  // Print measurement results based on status
  if ( errstat == CCS811_ERRSTAT_OK ) {
    Serial.print("CCS811: ");
    Serial.print("eco2=");  Serial.print(eco2);     Serial.print(" ppm  ");
    Serial.print("etvoc="); Serial.print(etvoc);    Serial.print(" ppb  ");
    //Serial.print("raw6=");  Serial.print(raw/1024); Serial.print(" uA  ");
    //Serial.print("raw10="); Serial.print(raw%1024); Serial.print(" ADC  ");
    //Serial.print("R="); Serial.print((1650*1000L/1023)*(raw%1024)/(raw/1024)); Serial.print(" ohm");
    Serial.println();
  } else if ( errstat == CCS811_ERRSTAT_OK_NODATA ) {
    Serial.println("CCS811: waiting for (new) data");
  } else if ( errstat & CCS811_ERRSTAT_I2CFAIL ) {
    Serial.println("CCS811: I2C error");
  } else {
    Serial.print("CCS811: errstat="); Serial.print(errstat, HEX);
    Serial.print("="); Serial.println( ccs811.errstat_str(errstat) );
  }
}

void readBME280(uint16_t *temperature, uint16_t *pressure, uint16_t *humidity) {
  if (temperature) *temperature = bme.readTemperature();
  if (pressure) *pressure = bme.readPressure() / 100.0F;
  if (humidity) *humidity = bme.readHumidity(); 
}

void printBME280() {
  Serial.print("BME280: ");
  Serial.print("Tmp=");
  Serial.print(bme.readTemperature());
  Serial.print(" *C ");
  
  // Convert temperature to Fahrenheit
  /*Serial.print("Temperature = ");
  Serial.print(1.8 * bme.readTemperature() + 32);
  Serial.println(" *F");*/
  
  Serial.print("Psr = ");
  Serial.print(bme.readPressure() / 100.0F);
  Serial.print(" hPa ");

  Serial.print("Alt=");
  Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  Serial.print(" m ");

  Serial.print("Hum=");
  Serial.print(bme.readHumidity());
  Serial.print(" %");

  Serial.println();
}
