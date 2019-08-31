ARDUINO_DIR   = /opt/arduino-1.8.9
ARDMK_DIR     = /opt/Arduino-Makefile
BOARDS_TXT    = ~/.arduino15/packages/esp8266/hardware/esp8266/2.5.1/boards.txt

ARDUINO_LIBS  = Wire SPI Adafruit_Unified_Sensor Adafruit_BME280_Library
BOARD_TAG     = d1_mini_pro
MONITOR_PORT  = /dev/ttyACM0

include $(ARDMK_DIR)/Arduino.mk
