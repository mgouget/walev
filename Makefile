BOARD=generic
FLASH_DEF=1M0
#proto
#23ESP_ADDR=192.168.32.157
#dev
#ESP_ADDR=192.168.32.123
#dev2
ESP_ADDR=192.168.32.147
ESP_PWD=golwen1
HOME=/home/mgouget
ESP_ROOT=$(HOME)/esp8266_arduino
SKETCHDIR=$(HOME)/esp8266/walev
BUILD_ROOT=$(SKETCHDIR)/.build
UPLOAD_PORT=/dev/ttyUSB0
BUILD_EXTRA_FLAGS= 

include $(HOME)/makeEspArduino/makeEspArduino.mk
#include makeEspArduino.mk