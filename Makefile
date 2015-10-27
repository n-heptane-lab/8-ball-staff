ARDUINO_DIR = /Users/stepcut/projects/arduino/Arduino.app/Contents/Resources/Java

BOARD_TAG    = teensy31
ARDUINO_LIBS = Wire Adafruit_LSM303DLHC Adafruit_L3GD20_U Adafruit_9DOF Adafruit_Sensor Adafruit_SSD1351 Adafruit_GFX SPI SD
F_CPU=72000000

include ../Arduino-Makefile/Teensy.mk
