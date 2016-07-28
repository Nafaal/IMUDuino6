/* ================================================================================================ *
  | Default MotionApps v2.0 42-byte FIFO packet structure:                                           |
  |                                                                                                  |
  | [QUAT W][      ][QUAT X][      ][QUAT Y][      ][QUAT Z][      ][GYRO X][      ][GYRO Y][      ] |
  |   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  |
  |                                                                                                  |
  | [GYRO Z][      ][ACC X ][      ][ACC Y ][      ][ACC Z ][      ][      ]                         |
  |  24  25  26  27  28  29  30  31  32  33  34  35  36  37  38  39  40  41                          |
   ================================================================================================ */
/* ================================================================================================ *
  | UDP packet structure:                                                                            |
  | [num of FIFO Pack][byte of FIFO Pack] [num of seq ] [BME packet] [FIFO1] [FIFO2] ... [FIFO n]    |
  |   1 byte                1 byte            2byte         8 byte   <--- 42 x n --------------->    |
   ================================================================================================ */

extern "C" {
#include "user_interface.h"
}

#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>
#include <Wire.h>
#include <NeoPixelBus.h>
#include <SPI.h>
#include <stdint.h>
#include <SparkFunBME280.h>

#include "SSD1306.h"
#include "SSD1306Ui.h"
#include "images.h"
#include "papanet_url_logo.h"
#include "celsius_logo.h"

//#define ENABLE_TEAPOT

// Wi-Fi設定保存ファイル
const char* settings = "/wifiimu_conf.txt";

// サーバモードでのパスワード
const String default_pass = "esp8266pass";

// MACアドレスを文字列化したもの
String macaddr = "";

// サーバモード
char server_mode = 0; // 0: cilent, 1: server, 2: calibration

// サーバインスタンス
ESP8266WebServer server(80);
String ssid = "angelbeats";
String pass = "12345678";
String ipaddr = "172.20.10.6";
String errmes = "";
int port = 8870, dmode = 1;

// クライアント
const int UDPBUFLEN = 600;
WiFiUDP Udp;
unsigned char udpbuf[UDPBUFLEN];
int udpptr = 0;
int udppackcount = 0;
unsigned short udpseqcount = 0;
const int hdroff = 12;

// i2cアドレス
const int oled_addr = 0x3c;
const int bme280_addr = 0x76;
const int mpu6050_addr = 0x69;

// GPIO
const int IO0_PIN = 0; // IO0 pull up
const int I2C_SDA_PIN = 5; // IO5 pull up
const int I2C_SCL_PIN = 4; // IO4 pull up
const int LED_OUT_PIN = 2; // IO2 pull up
const int MPU_INT_PIN = 15; // IO15 pull down

// MPU-6050 Motion Sensor
MPU6050 mpu(mpu6050_addr);
bool dmpReady = false;  // set true if DMP init was successful
bool sendReady = false;
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector
int ypr1, ypr2, ypr3;
uint8_t teapotPacket[14] = { '$', 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x00, '\r', '\n' };
Quaternion q;           // [w, x, y, z]         quaternion container
VectorFloat gravity;    // [x, y, z]            gravity vector
volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
int volt = 0; // mV
int longpush = 0;

int buffersize = 1000;   //Amount of readings used to average, make it higher to get more precision but sketch will be slower  (default:1000)
int acel_deadzone = 8;   //Acelerometer error allowed, make it lower to get more precision, but sketch may not converge  (default:8)
int giro_deadzone = 1;   //Giro error allowed, make it lower to get more precision, but sketch may not converge  (default:1)
int16_t ax, ay, az, gx, gy, gz;
int mean_ax, mean_ay, mean_az, mean_gx, mean_gy, mean_gz;
int state = 0;
int ax_offset, ay_offset, az_offset, gx_offset, gy_offset, gz_offset;

// SSD1360 OLED Display
SSD1306   display(oled_addr, I2C_SDA_PIN, I2C_SCL_PIN);
SSD1306Ui ui( &display );

// NeoPixelBus
int PixelCount = 1;
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> strip(PixelCount, LED_OUT_PIN);
RgbColor red(15, 0, 0);
RgbColor green(0, 15, 0);
RgbColor blue(0, 0, 15);
RgbColor white(30, 30, 30);
RgbColor gray(5, 5, 5);
RgbColor cyan(12, 12, 32);
RgbColor black(0, 0, 0);
RgbColor purple(15, 1, 15);
RgbColor yellow(15, 15, 0);

// BME280
BME280 bme280;
float temperture, pressure, humidity;

/**
 * Save
 */
void save_setting()
{
  File f = SPIFFS.open(settings, "w");
  f.println(ssid);
  f.println(pass);
  f.println(ipaddr);
  f.println(String(port));
  f.println(String(gx_offset));
  f.println(String(gy_offset));
  f.println(String(gz_offset));
  f.println(String(ax_offset));
  f.println(String(ay_offset));
  f.println(String(az_offset));
  f.println(String(dmode));
  f.close();
}

/**
 * Load
 */
void load_setting()
{
  File f = SPIFFS.open(settings, "r");
  String tmp;
  tmp = String(f.readStringUntil('\n')); tmp.trim();
  ssid = tmp;
  tmp = String(f.readStringUntil('\n')); tmp.trim();
  pass = tmp;
  tmp = String(f.readStringUntil('\n')); tmp.trim();
  ipaddr = tmp;
  tmp = String(f.readStringUntil('\n')); tmp.trim();
  port = tmp.toInt();
  tmp = String(f.readStringUntil('\n')); tmp.trim();
  gx_offset = tmp.toInt();
  tmp = String(f.readStringUntil('\n')); tmp.trim();
  gy_offset = tmp.toInt();
  tmp = String(f.readStringUntil('\n')); tmp.trim();
  gz_offset = tmp.toInt();
  tmp = String(f.readStringUntil('\n')); tmp.trim();
  ax_offset = tmp.toInt();
  tmp = String(f.readStringUntil('\n')); tmp.trim();
  ay_offset = tmp.toInt();
  tmp = String(f.readStringUntil('\n')); tmp.trim();
  az_offset = tmp.toInt();
  tmp = String(f.readStringUntil('\n')); tmp.trim();
  dmode = tmp.toInt();
  f.close();
}

/**
   WiFi設定画面(サーバーモード)
*/
void handleRootGet() {
  String html = "";
  html += "<html lang=\"ja\"><head><meta charset=\"utf-8\" /><meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\" /><title>WiFi AP Config</title></head><body>";
  html += "<h1>WiFi AP Config</h1>";
  html += "<form method='post'>";
  html += "  <label>SSID:<input type='text' name='ssid' placeholder='ssid' value='" + ssid + "'></label><br>";
  html += "  <label>PASS:<input type='text' name='pass' placeholder='pass' value='" + pass + "'></label><br>";
  html += "  <label>UDPADDR:<input type='text' name='ipaddr' placeholder='UDP Dst addr' value='" + ipaddr + "'></label><br>";
  html += "  <label>UDPPORT:<input type='text' name='port' placeholder='UDP Dst port' value='" + String(port) + "'></label><br>";
  html += "  <label>XGyro:<input type='text' name='gx_offset' placeholder='XGyroOffset' value='" + String(gx_offset) + "'></label><br>";
  html += "  <label>YGyro:<input type='text' name='gy_offset' placeholder='YGyroOffset' value='" + String(gy_offset) + "'></label><br>";
  html += "  <label>ZGyro:<input type='text' name='gz_offset' placeholder='ZGyroOffset' value='" + String(gz_offset) + "'></label><br>";
  html += "  <label>XAccel:<input type='text' name='az_offset' placeholder='XAccelOffset' value='" + String(ax_offset) + "'></label><br>";
  html += "  <label>YAccel:<input type='text' name='ay_offset' placeholder='YAccelOffset' value='" + String(ay_offset) + "'></label><br>";
  html += "  <label>ZAccel:<input type='text' name='az_offset' placeholder='ZAccelOffset' value='" + String(az_offset) + "'></label><br>";
  html += "  <label>Demo:<input type='checkbox' name='dmode' value='1' checked /></label><br>";

  html += "  <input type='submit'><br>";
  html += "</form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

/**
   SSID書き込み画面(サーバーモード)
*/
void handleRootPost() {
#ifndef ENABLE_TEAPOT
  Serial.println("POST");
#endif
  ssid = server.arg("ssid");
  pass = server.arg("pass");
  ipaddr = server.arg("ipaddr");
  port = String(server.arg("port")).toInt();
  gx_offset = String(server.arg("gx_offset")).toInt();
  gy_offset = String(server.arg("gy_offset")).toInt();
  gz_offset = String(server.arg("gz_offset")).toInt();
  ax_offset = String(server.arg("ax_offset")).toInt();
  ay_offset = String(server.arg("ay_offset")).toInt();
  az_offset = String(server.arg("az_offset")).toInt();
  dmode = String(server.arg("dmode")).toInt();
  save_setting();
  
  String html = "";
  html += "<html lang=\"ja\"><head><meta charset=\"utf-8\" /><meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\" /><title>WiFi AP Accepted</title></head><body>";
  html += "<h1>" + macaddr + "</h1>";
  html += ssid + "<br>";
  html += pass + "<br>";
  html += ipaddr + "<br>";
  html += String(port) + "<br>";
  html += String(gx_offset) + "<br>";
  html += String(gy_offset) + "<br>";
  html += String(gz_offset) + "<br>";
  html += String(ax_offset) + "<br>";
  html += String(ay_offset) + "<br>";
  html += String(az_offset) + "<br>";
  html += String(dmode) + "<br>";
  html += "</body></html>";
#ifndef ENABLE_TEAPOT
  Serial.println("POSTOK");
#endif
server.send(200, "text/html", html);
}

/**
   ホスト名ロード(クライアントモード)
*/
bool setup_conf() {
  bool ret = true;
  // Macアドレスを文字列に
  byte mac[6];
  WiFi.macAddress(mac);
  macaddr = "";
  for (int i = 0; i < 6; i++) {
    macaddr += String(mac[i], HEX);
  }
  // 未設定なら作成
  if (!SPIFFS.exists(settings)) {
    save_setting();
    ret = false;
  }
  // 設定を読み込む
  load_setting();
  if (gx_offset == 0 && gy_offset == 0 && gz_offset == 0 && ax_offset == 0 && ay_offset == 0 && az_offset == 0) {
    ret = false;
  }
  return ret;
}

/**
   WiFi接続試行(クライアントモード)
*/
boolean wifi_connect(String ssid, String pass) {
#ifndef ENABLE_TEAPOT
  Serial.println("SSID: " + ssid);
  Serial.println("PASS: " + pass);
#endif
  WiFi.begin(ssid.c_str(), pass.c_str());

  char timeout = 30;
  for (; timeout > 0; timeout--) {
    strip.SetPixelColor(0, gray);
    strip.Show();
    delay(100);
    if (WiFi.status() == WL_CONNECTED) {
      strip.SetPixelColor(0, cyan);
      strip.Show();
      delay(100);
#ifndef ENABLE_TEAPOT
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
#endif
      strip.SetPixelColor(0, black);
      strip.Show();
      return true;
    }
#ifndef ENABLE_TEAPOT
    Serial.print(".");
#endif
    strip.SetPixelColor(0, black);
    strip.Show();
    delay(100);
  }
  return false;
}

// IO15 int handler
void dmpDataReady() {
  mpuInterrupt = true;
}

// Simple Timer LibraryがLGPL2のため
class pInterval {
  public:
    uint32_t start_;
    uint32_t ms_;
    bool flip_;
    pInterval(uint32_t ms  = 1000, bool flip = false) : ms_(ms), start_(millis()), flip_(flip) {};
    bool check() {
      uint32_t now = millis();
      if ((now - start_) > ms_) {
        start_ = now;
        flip_ = !flip_;
        return true;
      } else {
        return false;
      }
    }
};
pInterval iv1(5000);
pInterval iv2(1000);
pInterval iv3(1000);

int count = 0;
char strbuf[10];
bool drawFrame1(SSD1306 *display, int x, int y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(x + 0, 54, "[BME]");
  if (temperture > 0) {
    display->drawXbm(x + 68, 0, celsius_width, celsius_height, (const char*)celsius_bits);
    display->drawString(x + 68, 18, "hPa");
    display->drawString(x + 68, 36, "%");

    display->setFont(ArialMT_Plain_16);
    dtostrf(temperture, 4, 1, strbuf);
    display->drawString(x + 10, 0, strbuf);
    dtostrf(pressure, 4, 1, strbuf);
    display->drawString(x + 10, 18, strbuf);
    dtostrf(humidity, 4, 1, strbuf);
    display->drawString(x + 10, 36, strbuf);
  } else {
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(x + 12, 30, "Please wait.");
  }
  return false;
}

bool drawFrame2(SSD1306 * display, int x, int y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(x + 0, 54, "[MPU]");
  if (dmpReady) {
    display->drawString(x + 68, 0, "Yaw");
    display->drawString(x + 68, 18, "Pitch");
    display->drawString(x + 68, 36, "Roll");

    display->setFont(ArialMT_Plain_16);
    display->drawString(x + 10, 0, String(ypr1));
    display->drawString(x + 10, 18, String(ypr2));
    display->drawString(x + 10, 36, String(ypr3));
  } else {
    display->setFont(ArialMT_Plain_16);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    if (errmes.length() > 0) {
      display->drawString(x + 12, 30, errmes);
    } else {
      display->drawString(x + 12, 30, "Press IO0 Btn.");
    }
  }
  return false;
}

bool drawFrame3(SSD1306 * display, int x, int y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(x + 0, 10, String(volt));
  display->drawString(x + 12, 20, "mV");
  display->drawXbm(x + 32, y, papanet_url_width, papanet_url_height, papanet_url_bits);
  return false;
}

/**
   初期化(キャリブレーションモード)
*/
void setup_calibration()
{
  server_mode = 2;
  strip.SetPixelColor(0, purple);
  strip.Show();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(12, 30, "Calibration");
  display.display();
}

/**
   初期化(サーバモード)
*/
void setup_server() {
  Serial.println("SSID: " + macaddr);
  Serial.println("PASS: " + default_pass);
  server_mode = 1;
  strip.SetPixelColor(0, white);
  strip.Show();

  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.softAP(macaddr.c_str(), default_pass.c_str());

  server.on("/", HTTP_GET, handleRootGet);
  server.on("/", HTTP_POST, handleRootPost);
  server.begin();
  Serial.println("HTTP server started.");
}

/**
   初期化(クライアントモード)
*/
void setup_client(String &macaddr) {
  count = 0;

  delay(700);

  if (wifi_connect(ssid, pass)) {
#ifndef ENABLE_TEAPOT
    Serial.println("Connected...");
#endif
  }
}

void setup() {
  // I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);

  // MPU-6050
  mpu.initialize();
  pinMode(MPU_INT_PIN, INPUT);

  // Serial
  Serial.begin(115200);
  // NeoPixelBuss
  strip.Begin();

  // OLED
  display.init();
  display.flipScreenVertically();
  display.displayOn();
  display.clear();

  // Push Switch
  pinMode(IO0_PIN, INPUT);

  // BME280
  bme280.settings.commInterface = I2C_MODE;
  bme280.settings.I2CAddress = bme280_addr;
  bme280.settings.I2CNotInit = 1;
  bme280.settings.runMode = 3;
  bme280.settings.tStandby = 0;
  bme280.settings.filter = 0;
  bme280.settings.tempOverSample = 1;
  bme280.settings.pressOverSample = 1;
  bme280.settings.humidOverSample = 1;
  bme280.begin();

  // ファイルシステム初期化
  SPIFFS.begin();
  if (setup_conf() == false) {
    setup_calibration();
    return;
  }

  // 1秒以内にMODEを切り替えることにする(IO0押下)
  delay(1000);

  if (digitalRead(IO0_PIN) == 0) {
    // サーバモード初期化
    setup_server();
  } else {
    // クライアントモード初期化
    setup_client(macaddr);
  }
}

void loop_server() {
  server.handleClient();
  if (iv1.check()) {
    count++;
    display.clear();
    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    if ((count % 2) == 0) {
      display.drawString(20, 16, "*AP Config*");
    } else {
      display.drawString(20, 16, "192.168.4.1");
    }
    display.drawString(12, 35, macaddr);
    display.setFont(ArialMT_Plain_10);
    display.drawString(4, 50, "<<PASS>>" + default_pass);
    display.display();
  }
  delay(100);
  ESP.wdtFeed();
}

void loop_client() {
  long t0 = millis();

  while (dmpReady && mpuInterrupt) {
    mpuInterrupt = false;
    mpuIntStatus = mpu.getIntStatus();
    fifoCount = mpu.getFIFOCount();
    if ((mpuIntStatus & 0x10) || fifoCount == 1024) {
      mpu.resetFIFO();
      strip.SetPixelColor(0, yellow);
      strip.Show();

    } else if (mpuIntStatus & 0x02) {
      int to2 = 15;
      while (fifoCount < packetSize && to2 > 0) {
        fifoCount = mpu.getFIFOCount();
        to2--;
        delay(1);
      }
      if (to2 <= 0) {
        return;
      }
      mpu.getFIFOBytes(fifoBuffer, packetSize);
      fifoCount -= packetSize;
      mpu.dmpGetQuaternion(&q, fifoBuffer);
      mpu.dmpGetGravity(&gravity, &q);
      mpu.dmpGetYawPitchRoll2(ypr, &q, &gravity);
      ypr1 = ypr[0] * 180 / M_PI;
      ypr2 = ypr[1] * 180 / M_PI;
      ypr3 = ypr[2] * 180 / M_PI;
#ifdef ENABLE_TEAPOT
      teapotPacket[2] = fifoBuffer[0]; // QUAT W (Quaternion = rotation of object)
      teapotPacket[3] = fifoBuffer[1];
      teapotPacket[4] = fifoBuffer[4]; // QUAT X
      teapotPacket[5] = fifoBuffer[5];
      teapotPacket[6] = fifoBuffer[8]; // QUAT Y
      teapotPacket[7] = fifoBuffer[9];
      teapotPacket[8] = fifoBuffer[12]; // QUAT Z
      teapotPacket[9] = fifoBuffer[13];
#endif
      if (sendReady) {
#ifdef ENABLE_TEAPOT
        Serial.write(teapotPacket, 14);
        teapotPacket[11]++; // packetCount, loops at 0xFF on purpose
#endif
        if ((UDPBUFLEN - udpptr - hdroff) >= packetSize) {
          memcpy((char *)&udpbuf + udpptr + hdroff, (char *)&fifoBuffer, packetSize);
          udpptr += packetSize;
          udppackcount++;
          udpbuf[0] = udppackcount;
          udpbuf[1] = packetSize;
          strip.SetPixelColor(0, green);
          strip.Show();
        } else {
          break;
        }
      }
    }
  }
  
  if (udpptr > (UDPBUFLEN / 2)) {
    short t2 = (short)(temperture * 10.0);
    short p2 = (short)(pressure * 10.0);
    short h2 = (short)(humidity * 10.0);
    udpbuf[2] = (unsigned char)(udpseqcount >> 8);
    udpbuf[3] = (unsigned char)(udpseqcount & 0xff);
    udpbuf[4] = (unsigned char)(t2 >> 8);
    udpbuf[5] = (unsigned char)(t2 & 0xff);
    udpbuf[6] = (unsigned char)(p2 >> 8);
    udpbuf[7] = (unsigned char)(p2 & 0xff);
    udpbuf[8] = (unsigned char)(h2 >> 8);
    udpbuf[9] = (unsigned char)(h2 & 0xff);
    if (sendReady == true) {
      Udp.beginPacket(ipaddr.c_str(), port);
      Udp.write((char *)&udpbuf, udpptr + hdroff);
      if (Udp.endPacket() == 0) {
        strip.SetPixelColor(0, red);
        strip.Show();
        sendReady = false;
      }
    }
    udpptr = 0;
    udppackcount = 0;
    udpseqcount++;
  }
  
  if (iv1.check()) {
    if (dmode) {
      int adcsample = system_adc_read();
      volt = adcsample * 5566 / 1000;
      temperture = bme280.readTempC();
      pressure = bme280.readFloatPressure() / 100.0; // x100 ... bug?
      humidity = bme280.readFloatHumidity();
    }
  }

  if (iv2.check()) {
    count++;
    int state = (int)(count / 10) % 3;
    int divi = (int)(count % 5);
    if (dmode) {
      switch (state) {
        case 0:
          if (divi == 0) {
            display.clear();
            drawFrame1(&display, 0, 0);
            display.display();
          }
          break;
        case 1:
          display.clear();
          drawFrame2(&display, 0, 0);
          display.display();
          break;
        case 2:
          if (divi == 0) {
            display.clear();
            drawFrame3(&display, 0, 0);
            display.display();
          }
          break;
      }
    }
    if (digitalRead(IO0_PIN) == 0 && longpush < 2) {
      errmes = "";
      if (sendReady == false && wifi_connect(ssid, pass)) {
        sendReady = true; 
        strip.SetPixelColor(0, cyan);
        strip.Show();
      } else {
        errmes = "WiFi Error";
        strip.SetPixelColor(0, red);  
        strip.Show(); 
      }
      if (dmpReady == false) {        
        devStatus = mpu.dmpInitialize();
        delay(500);
        mpu.setXGyroOffset(gx_offset);
        mpu.setYGyroOffset(gy_offset);
        mpu.setZGyroOffset(gz_offset);
        mpu.setXAccelOffset(ax_offset);
        mpu.setYAccelOffset(ay_offset);
        mpu.setZAccelOffset(az_offset);
        if (devStatus == 0) {
          mpu.setDMPEnabled(true);
          attachInterrupt(digitalPinToInterrupt(MPU_INT_PIN), dmpDataReady, RISING);
          mpuIntStatus = mpu.getIntStatus();
          dmpReady = true;
          packetSize = mpu.dmpGetFIFOPacketSize();
          strip.SetPixelColor(0, green);
          strip.Show();
        } else {
          errmes = "MPU Init Error";
          strip.SetPixelColor(0, red);
          strip.Show();
        }
      }
    }
  }
  if (iv3.check()) {
    if (digitalRead(IO0_PIN) == 0) {
      longpush++;
      if (longpush >= 20) {
        SPIFFS.remove(settings);
        longpush = 0;
        for (int i = 0; i < 10; i++) {
          strip.SetPixelColor(0, purple);
          strip.Show();
          delay(100);      
          strip.SetPixelColor(0, black);
          strip.Show();
          delay(100);      
        }
      }
    } else {
      longpush = 0;
    }
  }

  long t1 = millis();
  long span = t1 - t0;
  if (span < 10) {
    delay(10);
  }
  ESP.wdtFeed();
}

void disp_calib(String mes) {
      display.clear();
      display.setFont(ArialMT_Plain_10);
      dtostrf(gx_offset, 5, 1, strbuf);
      display.drawString(0, 10, String("GX:") + String(strbuf));

      dtostrf(gy_offset, 5, 1, strbuf);
      display.drawString(0, 22, String("GY:") + String(strbuf));
      
      dtostrf(gz_offset, 5, 1, strbuf);
      display.drawString(0, 34, String("GZ:") + String(strbuf));
      
      dtostrf(ax_offset, 5, 1, strbuf);
      display.drawString(75, 10, String("AX:") + String(strbuf));
      
      dtostrf(ay_offset, 5, 1, strbuf);
      display.drawString(75, 22, String("AY:") + String(strbuf));
      
      dtostrf(az_offset, 5, 1, strbuf);
      display.drawString(75, 34, String("AZ:") + String(strbuf));

      display.drawString(8, 54, mes);
      display.display();

}

void disp_mean(String mes) {
      display.clear();
      display.setFont(ArialMT_Plain_10);
      dtostrf(mean_gx, 5, 1, strbuf);
      display.drawString(0, 10, String("gx:") + String(strbuf));

      dtostrf(mean_gy, 5, 1, strbuf);
      display.drawString(0, 22, String("gy:") + String(strbuf));
      
      dtostrf(mean_gz, 5, 1, strbuf);
      display.drawString(0, 34, String("gz:") + String(strbuf));
      
      dtostrf(mean_ax, 5, 1, strbuf);
      display.drawString(75, 10, String("ax:") + String(strbuf));
      
      dtostrf(mean_ay, 5, 1, strbuf);
      display.drawString(75, 22, String("ay:") + String(strbuf));
      
      dtostrf(mean_az, 5, 1, strbuf);
      display.drawString(75, 34, String("az:") + String(strbuf));

      display.drawString(8, 54, mes);
      display.display();

}



void meansensors() {
  long i = 0, buff_ax = 0, buff_ay = 0, buff_az = 0, buff_gx = 0, buff_gy = 0, buff_gz = 0;

  while (i < (buffersize + 101)) {
    // read raw accel/gyro measurements from device
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    if (i > 100 && i <= (buffersize + 100)) { //First 100 measures are discarded
      buff_ax = buff_ax + ax;
      buff_ay = buff_ay + ay;
      buff_az = buff_az + az;
      buff_gx = buff_gx + gx;
      buff_gy = buff_gy + gy;
      buff_gz = buff_gz + gz;
    }
    if (i == (buffersize + 100)) {
      mean_ax = buff_ax / buffersize;
      mean_ay = buff_ay / buffersize;
      mean_az = buff_az / buffersize;
      mean_gx = buff_gx / buffersize;
      mean_gy = buff_gy / buffersize;
      mean_gz = buff_gz / buffersize;
    }
    i++;
    delay(2); //Needed so we don't get repeated measures
  }
}

void calibration() {
  ax_offset = -mean_ax / 8;
  ay_offset = -mean_ay / 8;
  az_offset = (-1 * mean_az + 16384) / 8;

  gx_offset = -mean_gx / 4;
  gy_offset = -mean_gy / 4;
  gz_offset = -mean_gz / 4;
  int loop = 1;
  while (1) {
    int ready = 0;
    mpu.setXAccelOffset(ax_offset);
    mpu.setYAccelOffset(ay_offset);
    mpu.setZAccelOffset(az_offset);

    mpu.setXGyroOffset(gx_offset);
    mpu.setYGyroOffset(gy_offset);
    mpu.setZGyroOffset(gz_offset);

    meansensors();
    if (abs(mean_ax) <= acel_deadzone) ready++;
    else ax_offset = ax_offset - mean_ax / acel_deadzone;

    if (abs(mean_ay) <= acel_deadzone) ready++;
    else ay_offset = ay_offset - mean_ay / acel_deadzone;

    if (abs(-1 * mean_az + 16384) <= acel_deadzone) ready++;
    else az_offset = az_offset + (-1 * mean_az + 16384) / acel_deadzone;

    if (abs(mean_gx) <= giro_deadzone) ready++;
    else gx_offset = gx_offset - mean_gx / (giro_deadzone + 1);

    if (abs(mean_gy) <= giro_deadzone) ready++;
    else gy_offset = gy_offset - mean_gy / (giro_deadzone + 1);

    if (abs(mean_gz) <= giro_deadzone) ready++;
    else gz_offset = gz_offset - mean_gz / (giro_deadzone + 1);

    sprintf(strbuf, "<<READY:%d>>", ready);
    disp_mean(String(strbuf));
    if (ready == 6) break;
  }
}

void loop_calibration()
{
  if (dmpReady == false) {
    if (digitalRead(IO0_PIN) == 0) {
      dmpReady = true;
      mpu.initialize();
      display.clear();
      display.drawString(32, 30, "Start...");
      display.display();
      delay(2000);
      if (mpu.testConnection()) {
        strip.SetPixelColor(0, cyan);
        strip.Show();
      } else {
        strip.SetPixelColor(0, red);
        strip.Show();
        state = 3;
        return;        
      }
      mpu.setXAccelOffset(0);
      mpu.setYAccelOffset(0);
      mpu.setZAccelOffset(0);
      mpu.setXGyroOffset(0);
      mpu.setYGyroOffset(0);
      mpu.setZGyroOffset(0);
    }
  } else {
    if (state == 0) {
      meansensors();
      disp_mean("<<calc mean>>");
      state++;
      delay(1000);
    }
    if (state == 1) {
      calibration();
      state++;
      delay(1000);
    }
    if (state == 2) {
      meansensors();
      disp_mean("<<completed>>");
      save_setting();
      state++;
      delay(1000);
    }
    if (state == 3) {
      disp_calib("<<please reset>>");
      strip.SetPixelColor(0, blue);
      strip.Show();
      delay(1000);
    }
  }
}

void loop() {
  switch (server_mode) {
    case 0:
      loop_client();
      break;
    case 1:
      loop_server();
      break;
    default:
      loop_calibration();
  }
}
