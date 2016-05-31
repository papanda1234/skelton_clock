# skelton_clock
a simple OLED clock application for ESP8266.

 * Full Color LED library :  https://github.com/Makuna/NeoPixelBus
 * OLED library： https://github.com/squix78/esp8266-oled-ssd1306
 * HTTP server sample code : http://qiita.com/exabugs/items/2f67ae363a1387c8967c
 * HTTP client sample code : http://eleclog.quitsq.com/2015/11/esp8266-https.html
 * NTP library： https://github.com/exabugs/sketchLibraryNTP
 * Copyright (c) 2016 papanda@papa.to Released under the MIT license. https://opensource.org/licenses/mit-license.php

ESP8266で、ある程度実用的なWiFi表示デバイスを作ろうとしたときに必要になるコードの詰め合わせです。

 * IO0にtactile switch1を接続してください
 * RSTにtactile switch2を接続してください
 * IO2に適当なレベル変換をかませてFull Color LED WS2812Bを接続してください
 * IO5にOLEDのI2C SDAを接続してください（3.3Vでプルアップしてください）
 * IO4にOLEDのI2C SDLを接続してください（3.3Vでプルアップしてください）
 * IO16は適当な制限抵抗をかませてRSTラインに接続してください
 * TOUT(ADC)はHigh-Z開放としてください

RSTボタンを押して離してすぐにIO0ボタンを押すとWiFi AP Modeに入ります。
この状態でESP8266のMACアドレスをAP NameとしてアクセスポイントとしたAPが見えると思うので
スマホなどで接続します。その後Webブラウザで「192.168.4.1」にアクセスすると設定画面に入れます。
設定画面でSSID, WPA PASSなどを設定することができます。

IO0を押さずに起動すると、AP Modeで設定したSSIDを元にクライアント接続を行い、
インターネットのNTPサーバから時刻をとってきて時計として機能します。
