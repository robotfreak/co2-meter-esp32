#include <simpleDSTadjust.h>

// Pin Setup for the ILI9341 and ESP32
#define TFT_DC 4
#define TFT_CS 5
#define TFT_LED 15
#define TFT_RST 22

// I2C Pins
#define I2C_SDA 25
#define I2C_SCL 26

#define CCS811_WAKE_PIN 32

// WiFi Setup
#define WIFI_SSID "ssid"
#define WIFI_PASS "password"
#define WIFI_HOSTNAME "hostname"

// change for different ntp (time servers)
#define NTP_SERVERS "0.de.pool.ntp.org", "1.de.pool.ntp.org", "2.de.pool.ntp.org"

// Change for 12 Hour/ 24 hour style clock
bool IS_STYLE_12HR = false;

#define UTC_OFFSET +1
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600}; // Central European Summer Time = UTC/GMT +2 hours
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};       // Central European Time = UTC/GMT +1 hour
