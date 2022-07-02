// M5Stamp C3U
// 32 bit RISC-V シングルコアプロセッサ（動作周波数160 MHz）
// 384 KB ROM、400 KB SRAM、8 KB RTC SRAM、4 MBフラッシュ
// プログラマブル物理ボタン GPIO9
// リセットデバッグボタン
// プログラマブルRGB LED SK6812 GPIO2
// 電源 5V 待機電流 1mA

// ステータス
// 赤:待機 青:稼働中 緑:セッティングモード
// 緑:送信中・処理中

// debug用
#define debug

#define BRIGHTNESS 5 // LEDの明るさセット

#include <Arduino.h>

#include <esp_now.h>
#include <WiFi.h>

#include <EEPROM.h>

#include <FastLED.h>
CRGB leds[1]; // LEDの色情報を入れる変数

/*
// stamp u3c
// ボタンピン
#define Btn_pin 9 // 9pin

// LED データピン
#define LED_pin 2 // 2pin

// 各センサーのピン番号
#define sensor_pow 10
#define sensor1 4
#define sensor2 5
#define sensor3 6
#define sensor4 7
#define sensor5 8
*/

// ATOM

// ボタンピン
#define Btn_pin 39 // 9pin

// LED データピン
#define LED_pin 27 // 2pin

// 各センサーのピン番号
#define sensor_pow 19
#define sensor1 21
#define sensor2 22
#define sensor3 23
#define sensor4 25
#define sensor5 33

// 割り込み用変数
volatile uint8_t Btn_trig = 0;
volatile uint8_t send_res = 0;
volatile uint8_t set_status = 0;
volatile uint32_t sensor_time[5] = {0, 0, 0, 0, 0};
volatile uint8_t send_trig[5] = {0};

// グローバル変数
uint8_t master_set_trig = 0; // マスターセット用

// EEPROMの構造体
struct set_data
{
  uint8_t slave_no;
  uint8_t master_mac_address[6]; // マスターMACアドレス
};
set_data eeprom_date; // 構造体宣言

// EEPROMへ書き込み
void eeprom_write()
{
  EEPROM.put<set_data>(0, eeprom_date); // EEPROMへの書き込み
  EEPROM.commit();                      // これが必要らしい
}

// ESP32 NOW関係
esp_now_peer_info_t slave; // slaveのアドレス入れ

// 送信コールバック
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
#ifdef debug
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
#endif
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    send_res = 1;
  }
  else
  {
    send_res = 2;
  }
}

// 受信コールバック
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
#ifdef debug
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.printf("Last Packet Recv from: %s\n", macStr);
  Serial.printf("Last Packet Recv Data(%d): ", data_len);
  for (int i = 0; i < data_len; i++)
  {
    Serial.print(data[i]);
    Serial.print(" ");
  }
  Serial.println("");
  Serial.println("receve time");
  Serial.println(millis());
#endif

  // マスター登録
  if (master_set_trig == 1 && data[0] == 0)
  {
    // マスターのmacアドレスを格納する
    eeprom_date.slave_no = data[1];
    for (int i = 0; i < 6; i++)
    {
      eeprom_date.master_mac_address[i] = mac_addr[i];
    }
    master_set_trig = 2; // 格納完了のトリガ
  }
  else
  {
    // マスターか確認する
    uint8_t check_trig = 0;
    for (int i = 0; i < 6; i++)
    {
      if (
          eeprom_date.master_mac_address[i] != mac_addr[i])
        check_trig = 1;
    }
    if (check_trig == 0)
    {
      if (data[0] == 30)
      {
        set_status = 1;
      }

      if (data[0] == 31)
      {
        set_status = 2;
      }
    }
  }
}

// 各センサー割り込み処理
void Btn_isr()
{
  if (Btn_trig == 0)
  {
    delayMicroseconds(100); // チャタリング防止
    if (digitalRead(Btn_pin) == LOW)
    {
      Btn_trig = 1;
    }
    else
    {
      Btn_trig = 2;
    }
  }
}

// データ送信用
void reference_time_send(uint32_t time_data)
{

  uint8_t data[6];
  data[0] = 32;
  data[1] = eeprom_date.slave_no;
  data[2] = (uint8_t)((time_data & 0xFF000000) >> 24);
  data[3] = (uint8_t)((time_data & 0x00FF0000) >> 16);
  data[4] = (uint8_t)((time_data & 0x0000FF00) >> 8);
  data[5] = (uint8_t)((time_data & 0x000000FF) >> 0);

#ifdef debug
Serial.println("send time");
  Serial.println(millis());
#endif
  esp_err_t result = esp_now_send(slave.peer_addr, data, sizeof(data));
  while (send_res != 1)
  {
    delay(50);
    if (send_res == 2)
    {
      esp_err_t result = esp_now_send(slave.peer_addr, data, sizeof(data));
      send_res = 0;
    }
  }
  send_res = 0;
}

// データ送信用
void send_time(uint8_t *lane, uint32_t time_data)
{
  // 送信中は緑点灯
  leds[0] = CRGB(0, 255, 0);
  FastLED.show();

  uint8_t data[7];
  data[0] = 33;
  data[1] = eeprom_date.slave_no;
  data[2] = *lane;
  data[3] = (uint8_t)((time_data & 0xFF000000) >> 24);
  data[4] = (uint8_t)((time_data & 0x00FF0000) >> 16);
  data[5] = (uint8_t)((time_data & 0x0000FF00) >> 8);
  data[6] = (uint8_t)((time_data & 0x000000FF) >> 0);

  esp_err_t result = esp_now_send(slave.peer_addr, data, sizeof(data));
  while (send_res != 1)
  {
    delay(50);
    if (send_res == 2)
    {
      esp_err_t result = esp_now_send(slave.peer_addr, data, sizeof(data));
      send_res = 0;
    }
  }
  send_res = 0;

  // 送信済み後は青点灯
  leds[0] = CRGB(0, 0, 255);
  FastLED.show();
}

void sensor1_isr()
{
  if (sensor_time[0] == 0)
    sensor_time[0] = millis();
}
void sensor2_isr()
{
  if (sensor_time[1] == 0)
    sensor_time[1] = millis();
}
void sensor3_isr()
{
  if (sensor_time[2] == 0)
    sensor_time[2] = millis();
}
void sensor4_isr()
{
  if (sensor_time[3] == 0)
    sensor_time[3] = millis();
}
void sensor5_isr()
{
  if (sensor_time[4] == 0)
    sensor_time[4] = millis();
}

// スタート処理
void set_start()
{
  leds[0] = CRGB(0, 255, 0);
  FastLED.show();
  reference_time_send(millis()); // 基準となる時間を送信する

  digitalWrite(sensor_pow, HIGH); // センサー電源 ON
  delay(100);
  attachInterrupt(sensor1, sensor1_isr, CHANGE);
  attachInterrupt(sensor2, sensor2_isr, CHANGE);
  attachInterrupt(sensor3, sensor3_isr, CHANGE);
  attachInterrupt(sensor4, sensor4_isr, CHANGE);
  attachInterrupt(sensor5, sensor5_isr, CHANGE);

  // スタート完了したので青を点灯
  leds[0] = CRGB(0, 0, 255);
  FastLED.show();
}

// 終了処理
void set_end()
{
  leds[0] = CRGB(0, 255, 0);
  FastLED.show();

  detachInterrupt(sensor1);
  detachInterrupt(sensor2);
  detachInterrupt(sensor3);
  detachInterrupt(sensor4);
  detachInterrupt(sensor5);
  delay(100);
  digitalWrite(sensor_pow, LOW); // センサー電源 OFF

  // 終了完了したので赤を点灯
  leds[0] = CRGB(255, 0, 0);
  FastLED.show();
}

// esp now 初期設定
void esp_now_begin()
{

  //  ESP-NOW初期化
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK)
  {
    Serial.println("ESPNow Init Success");
  }
  else
  {
    Serial.println("ESPNow Init Failed");
    ESP.restart();
  }

  // Slave登録
  memset(&slave, 0, sizeof(slave));

  for (int i = 0; i < 6; ++i)
  {
    slave.peer_addr[i] = eeprom_date.master_mac_address[i];
  }
  esp_err_t addStatus = esp_now_add_peer(&slave);

#ifdef debug
  if (addStatus == ESP_OK)
  {
    // Pair success
    Serial.println("Pair success");
  }
#endif

  // ESP-NOWコールバック登録
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
}

// マスター登録
void master_set()
{
#ifdef debug
  Serial.println("master set");
#endif
  // ボタン離すまで消灯
  leds[0] = CRGB(0, 0, 0);
  FastLED.show();

  // ボタン押している間待ち
  while (digitalRead(Btn_pin) == LOW)
  {
    delay(100);
  }

  // 待ち受けモードに入るので緑点灯
  leds[0] = CRGB(0, 255, 0);
  FastLED.show();
  digitalWrite(sensor_pow, HIGH); // センサー電源 ON
  master_set_trig = 1;            // 登録用トリガ
  delay(1000);

  // ボタン押すまで待ち受け
  while (digitalRead(Btn_pin) == HIGH)
  {
    delay(100);
    if (master_set_trig == 2)
    {
      eeprom_write();
      // マスターに返信する
      // uint8_t _send_no = 255;
      // uint32_t _send_time = millis();
      // send_time(&_send_no, _send_time);
      uint8_t data[2] = {1, eeprom_date.slave_no};
      esp_now_send(slave.peer_addr, data, sizeof(data));

      break;
    }
  }

  digitalWrite(sensor_pow, LOW); // センサー電源 OFF
  // リセットするので消灯
  leds[0] = CRGB(0, 0, 0);
  FastLED.show();
  ESP.restart(); // リセットする
  delay(1000);
}

void setup()
{
  setCpuFrequencyMhz(80); // CPU周波数変更
#ifdef debug
  Serial.begin(115200); // デバック用等
#endif
  FastLED.addLeds<NEOPIXEL, LED_pin>(leds, 1); // LED初期化
  FastLED.setBrightness(BRIGHTNESS);

  // EEPROM 読み込み
  EEPROM.begin(1024);                   // EEPROM開始(サイズ指定)
  EEPROM.get<set_data>(0, eeprom_date); // EEPROMを読み込む

  // while(1)delay(100);

  // 各種ピンモード
  pinMode(sensor_pow, OUTPUT);
  pinMode(Btn_pin, INPUT_PULLUP);
  pinMode(sensor1, INPUT_PULLUP);
  pinMode(sensor2, INPUT_PULLUP);
  pinMode(sensor3, INPUT_PULLUP);
  pinMode(sensor4, INPUT_PULLUP);
  pinMode(sensor5, INPUT_PULLUP);

  // ボタン割り込み開始
  attachInterrupt(Btn_pin, Btn_isr, CHANGE);

  esp_now_begin(); // ESP_now 初期化

  // 起動したので赤を点灯
  leds[0] = CRGB(255, 0, 0);
  FastLED.show();

  delay(1000); // 赤を1秒表示

  // 初期設定トリガ
  if (digitalRead(Btn_pin) == LOW)
    master_set();
#ifdef debug
  Serial.println("setup end");
#endif
}

void loop()
{
  if (set_status == 1)
  {
#ifdef debug
    Serial.println("set start");
#endif
    set_start();
    set_status = 0;
  }
  if (set_status == 2)
  {
#ifdef debug
    Serial.println("set end1");
#endif
    set_end();
    set_status = 0;
  }
  if (Btn_trig != 0)
  {
#ifdef debug
    Serial.println("set end2");
#endif
    set_end();
    Btn_trig = 0;
  }

  // センサーが感知したら感知した時間を送信する
  // センサーが感知したか確認する
  for (uint8_t i = 0; i < 5; i++)
  {
    if (sensor_time[i] != 0)
    {
      send_time(&i, sensor_time[i]);
    }
    sensor_time[i] = 0;
  }

  delay(1);
}
