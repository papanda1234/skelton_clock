/**
 * LED library :  https://github.com/Makuna/NeoPixelBus
 * OLED library： https://github.com/squix78/esp8266-oled-ssd1306
 * HTTP server sample code : http://qiita.com/exabugs/items/2f67ae363a1387c8967c
 * HTTP client sample code : http://eleclog.quitsq.com/2015/11/esp8266-https.html
 * NTP library： https://github.com/exabugs/sketchLibraryNTP
 * Copyright (c) 2016 papanda@papa.to Released under the MIT license. 
 * https://opensource.org/licenses/mit-license.php
*/

extern "C" {
  #include "user_interface.h"
}

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <Wire.h>
#include <FS.h>
#include <NeoPixelBus.h>
#include <NTP.h>
#include <SSD1306.h>
#include <SSD1306Ui.h>

// TOUTを無効にして電源電圧を測定する
ADC_MODE(ADC_VCC);

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

// メインループのウェイト(ms)
const int waitms = 200;

// ピン配置
const int IO0_PIN = 0; // IO0 pull up
const int I2C_SDA_PIN = 5; // IO5 pull up
const int I2C_SCL_PIN = 4; // IO4 pull up
const int LED_OUT_PIN = 2; // IO2 pull up

// Wi-Fi設定保存ファイル
const char* settings = "/skelton_clock_conf.txt";

// サーバモードでのパスワード
const String default_pass = "esp8266pass";

// MACアドレスを文字列化したもの
String macaddr = "";

// サーバモード
char server_mode = 0;

// LED 
int PixelCount = 1;
NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart800KbpsMethod> strip(PixelCount, LED_OUT_PIN);
RgbColor red(15, 0, 0);
RgbColor green(0, 15, 0);
RgbColor blue(0, 0, 15);
RgbColor white(30, 30, 30);
RgbColor gray(5, 5, 5);
RgbColor cyan(12, 12, 32);
RgbColor black(0, 0, 0);
RgbColor yellow(15, 15, 0);
RgbColor purple(15, 0, 15);

// サーバインスタンス
ESP8266WebServer server(80);

// クライアントインスタンス
WiFiClient client;
const String api_host = "www.papa.to";
const String api_path = "/webapi/esp8266/skelton.pl";
String ssid, pass, host, path, loc;
String errmes;

// 状態カウンタ
unsigned int count = 0;
int dmode = 0;
pInterval iv1(1000);
pInterval iv2(60000);

// OLED
const int oled_addr = 0x3C;
SSD1306 display(oled_addr, I2C_SDA_PIN, I2C_SCL_PIN);

/**
 * WiFi設定画面(サーバーモード)
 */
void handleRootGet() {
  String html = "";
  html += "<html lang=\"ja\"><head><meta charset=\"utf-8\" /><meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\" /><title>WiFi AP Config</title></head><body>";
  html += "<h1>WiFi AP Config</h1>";
  html += "<form method='post'>";
  html += "  <input type='text' name='ssid' placeholder='ssid' value='" + ssid + "'><br>";
  html += "  <input type='text' name='pass' placeholder='pass' value='" + pass + "'><br>";
  html += "  <input type='text' name='api' placeholder='api' value='" + host + path + "'><br>";
  html += "  <input type='text' name='loc' placeholder='Location...' value='" + loc + "'><br>";
  html += "  <input type='checkbox' name='dmode' value='1' />demo mode<br><br>";
  
  html += "  <input type='submit'><br>";
  html += "</form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

/**
 * SSID書き込み画面(サーバーモード)
 */
void handleRootPost() {
  Serial.println("POST");
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  String api = server.arg("api");
  String loc = server.arg("loc");
  String dmode_str = server.arg("dmode");
  Serial.println(ssid + pass + api + loc + dmode_str);

  File f = SPIFFS.open(settings, "w");
  f.println(ssid);
  f.println(pass);
  f.println(api);  
  f.println(loc);
  f.println(dmode_str);
  f.close();

  String html = "";
  html += "<html lang=\"ja\"><head><meta charset=\"utf-8\" /><meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\" /><title>WiFi AP Accepted</title></head><body>";
  html += "<h1>" + macaddr + "</h1>";
  html += ssid + "<br>";
  html += pass + "<br>";
  html += api + "<br>";
  html += loc + "<br>";
  html += dmode_str + "<br>";
  html += "</body></html>";
  Serial.println("POSTOK");
  server.send(200, "text/html", html);
}

/**
 * ホスト名ロード(クライアントモード) 
 */
void setup_host_and_path(String &ssid, String &pass, String &host, String &path) {
  File f = SPIFFS.open(settings, "r");
  ssid = f.readStringUntil('\n');
  pass = f.readStringUntil('\n');
  String api = f.readStringUntil('\n');
  loc = f.readStringUntil('\n');
  String dmode_str = f.readStringUntil('\n');
  f.close();

  ssid.trim();
  pass.trim();
  api.trim();
  loc.trim();
  dmode_str.trim();
  dmode = dmode_str.toInt() + 0;

  host = api_host;
  path = api_path;
  if (api.length() >= 11) {
    int i = api.indexOf("/");
    if (i >= 8) {
      host = api.substring(0, i);
      path = api.substring(i);
    }
  }
}

/**
 * WiFi接続試行(クライアントモード)
 */
boolean wifi_connect(String ssid, String pass) {
  Serial.println("SSID: " + ssid);
  Serial.println("PASS: " + pass);
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
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      strip.SetPixelColor(0, black);
      strip.Show();
      return true;    
    }
    Serial.print(".");
    strip.SetPixelColor(0, black);
    strip.Show();
    delay(100);
  }
  return false;
}

/**
 * HTTP POST実行(クライアントモード) 
 */
String http_post(String host, String path, String data) {
  Serial.println("HOST: " + host);
  Serial.println("PATH: " + path);
  if (client.connect(host.c_str(), 80)) {
    client.println("POST " + path + " HTTP/1.1");
    client.println("Host: " + (String)host);
    client.println("User-Agent: ESP8266/1.0");
    client.println("Connection: close");
    client.println("Content-Type: application/x-www-form-urlencoded;");
    client.print("Content-Length: ");
    client.println(data.length());
    client.println();
    client.println(data);
    int tcount = 100;
    while (!client.available()) {
      delay(50);
      tcount--;
      if (tcount < 0) {
        Serial.println("recv error");
        return "ERROR";
      }
    }
    String response = client.readString();
    int bodypos =  response.indexOf("\r\n\r\n") + 4;
    return response.substring(bodypos);
  }
  else {
    return "ERROR";
  }
}

/**
 * 初期化(サーバモード)
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
 * OTA初期化
 */
void setup_ota(String macaddr)
{
  ArduinoOTA.setHostname(macaddr.c_str());
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
  MDNS.begin(macaddr.c_str());  
}

/**
 * 初期化(クライアントモード) 
 */
void setup_client() {
  count = 0;

  delay(800);

  if (wifi_connect(ssid, pass)) {
    Serial.println("NTP Starting...");
    ntp_begin(2390);
  }
}   

void power_down(unsigned long sec)
{
  Serial.println("PwrSave...");
  strip.SetPixelColor(0, black);
  strip.Show();
  display.displayOff();
  ESP.deepSleep(sec*1000*1000, WAKE_RF_DEFAULT);
  delay(1000);
}

/* セットアップ */
void setup() {
  //シリアル初期化
  Serial.begin(115200);
  Serial.println();
  // LED初期化
  strip.Begin();
  // OLED初期化
  display.init();
  display.flipScreenVertically();
  display.displayOn();
  display.clear();
  // デジタルI/O初期化
  pinMode(IO0_PIN, INPUT);
  // ファイルシステム初期化
  SPIFFS.begin();
  setup_host_and_path(ssid, pass, host, path);
  // 1秒以内にMODEを切り替えることにする(IO0押下)
  delay(1000);
  
  // Macアドレスを文字列に
  byte mac[6];
  WiFi.macAddress(mac);
  macaddr = "";
  for (int i = 0; i < 6; i++) {
    macaddr += String(mac[i], HEX);
  }
  
  if (digitalRead(IO0_PIN) == 0) {
    // サーバモード初期化
    setup_server();
  } else {
    // クライアントモード初期化
    setup_client();
  }
}

/* メインループ */
void loop() {
  char s[20];
  if (server_mode) {
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
      display.drawString(4, 50, "<<PASS>>"+default_pass);
      display.display();
    }
  } else {
    ArduinoOTA.handle();
    if (iv1.check()) {
      // 毎秒1回実行
      count++;
      
      time_t t = now();
      t = localtime(t, 9);
      display.clear();
      sprintf(s, "%02d:%02d:%02d", hour(t), minute(t), second(t));
      display.setFont(ArialMT_Plain_24);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawString(16, 18, s);
      sprintf(s, "%04d-%02d-%02d", year(t), month(t), day(t));
      display.setFont(ArialMT_Plain_10);
      display.drawString(36, 50, s);

      int v = ESP.getVcc();
      display.drawString(4, 4, String(v) + "mV");
      display.display();
      if (digitalRead(IO0_PIN) == 0) {
        power_down(3600);        
      }
    }
    if (iv2.check()) {
        //毎分1回実行
        if (wifi_connect(ssid, pass)) {
          String data = "mac=" + macaddr;
          String res = http_post(host, path, data);
          strip.SetPixelColor(0, blue);
          strip.Show();
        } else {
          strip.SetPixelColor(0, red);
          strip.Show();        
        }        
    }
    delay(waitms);
  }
  ESP.wdtFeed();
}
