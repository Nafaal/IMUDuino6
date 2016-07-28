#!/bin/sh

ARHOME=~/Arduino
mkdir -p $ARHOME/libraries

unzip -o SparkFun_BME280_Arduino_Library-master.zip
cd SparkFun_BME280_Arduino_Library-master
patch -p1 < ../bme280.patch
cd ..
cp -r SparkFun_BME280_Arduino_Library-master $ARHOME/libraries

unzip -o i2cdevlib-master.zip
cd i2cdevlib-master
patch -p1 < ../mpu6050.patch
cd ..
cp -r i2cdevlib-master/Arduino/I2Cdev $ARHOME/libraries
cp -r i2cdevlib-master/Arduino/MPU6050 $ARHOME/libraries

unzip -o esp8266-oled-ssd1306-2.0.2.zip
cp -r esp8266-oled-ssd1306-2.0.2 $ARHOME/libraries

unzip -o NeoPixelBus-master.zip
cp -r NeoPixelBus-master $ARHOME/libraries
