// M5Stamp C3U
// 32 bit RISC-V シングルコアプロセッサ（動作周波数160 MHz）
// 384 KB ROM、400 KB SRAM、8 KB RTC SRAM、4 MBフラッシュ
// プログラマブル物理ボタン GPIO9
// リセットデバッグボタン
// プログラマブルRGB LED SK6812 GPIO2
// 電源 5V 待機電流 1mA

// ステータス
// 赤点滅:待機 青:稼働中 緑:セッティングモード
// 緑:送信中・処理中

// debug用
#define debug

#define BRIGHTNESS 5 // LEDの明るさセット

#include <Arduino.h>

#include <esp_now.h>
#include <WiFi.h>

#include <EEPROM.h>

#include <FastLED.h>

#define led_quantity 25  // ATOM matrixの時は25にする
CRGB leds[led_quantity]; // LEDの色情報を入れる変数

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

RTC_DATA_ATTR uint8_t boot_count = 0; // sleepしても消えない

// 割り込み用変数
volatile uint8_t Btn_trig = 0;
volatile uint8_t send_res = 0;
volatile int16_t set_status = 0;
volatile uint32_t sensor_time[5] = {0, 0, 0, 0, 0};
volatile uint8_t send_trig[5] = {0};

// esp_nowで受信したタスクを格納する変数
volatile int16_t task_list[10] = {-1};

int16_t task_append(uint8_t task_no)
{
  for (uint8_t i = 0; i < 10; i++)
  {
    if (task_list[i] == -1)
    {
      task_list[i] = task_no;
      Serial.print("task_append");
      return 0;
    }
  }
  return -1;
}

int16_t task_read()
{
  for (uint8_t i = 0; i < 10; i++)
  {
    if (task_list[i] != -1)
    {
      int16_t task_no = task_list[i];
      task_list[i] = -1;
      Serial.print("task_read");
      return task_no;
    }
  }
  return -1;
}

// グローバル変数
uint8_t master_set_trig = 0; // マスターセット用

void led_set(uint8_t R, uint8_t G, uint8_t B)
{
  for (uint8_t i = 0; i < led_quantity; i++)
  {
    leds[i] = CRGB(R, G, B);
    FastLED.show();
  }
}

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

// Sleep 開始
void sleep_start()
{
  // 100000us = 0.1sのタイマー設定
  esp_sleep_enable_timer_wakeup(100000);
  // ディープスリープ
  esp_deep_sleep_start();
  delay(1000); // sleep移行待機
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
      task_append(data[0]);
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
  led_set(0, 255, 0);

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
  led_set(0, 0, 255);
}

void sensor1_isr()
{
  if (send_trig[0] == 0)
  {
    send_trig[0] = 1;
    sensor_time[0] = millis();
  }
}
void sensor2_isr()
{
  if (send_trig[1] == 0)
  {
    send_trig[1] = 1;
    sensor_time[1] = millis();
  }
}
void sensor3_isr()
{
  if (send_trig[2] == 0)
  {
    send_trig[2] = 1;
    sensor_time[2] = millis();
  }
}
void sensor4_isr()
{
  if (send_trig[3] == 0)
  {
    send_trig[3] = 1;
    sensor_time[3] = millis();
  }
}
void sensor5_isr()
{
  if (send_trig[4] == 0)
  {
    send_trig[4] = 1;
    sensor_time[4] = millis();
  }
}

// スタート処理
void set_start()
{
  led_set(0, 255, 0);
  reference_time_send(millis()); // 基準となる時間を送信する

  digitalWrite(sensor_pow, HIGH); // センサー電源 ON
  delay(100);
  attachInterrupt(sensor1, sensor1_isr, CHANGE);
  attachInterrupt(sensor2, sensor2_isr, CHANGE);
  attachInterrupt(sensor3, sensor3_isr, CHANGE);
  attachInterrupt(sensor4, sensor4_isr, CHANGE);
  attachInterrupt(sensor5, sensor5_isr, CHANGE);

  // スタート完了したので青を点灯
  led_set(0, 0, 255);
}

// 終了処理
void set_end()
{
  led_set(0, 255, 0);

  detachInterrupt(sensor1);
  detachInterrupt(sensor2);
  detachInterrupt(sensor3);
  detachInterrupt(sensor4);
  detachInterrupt(sensor5);
  delay(100);
  digitalWrite(sensor_pow, LOW); // センサー電源 OFF

  // 終了完了したので赤を点灯
  led_set(255, 0, 0);
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
  led_set(0, 0, 0);

  // ボタン押している間待ち
  while (digitalRead(Btn_pin) == LOW)
  {
    delay(100);
  }

  // 待ち受けモードに入るので緑点灯
  led_set(0, 255, 0);
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
  led_set(0, 0, 0);
  ESP.restart(); // リセットする
  delay(1000);
}

void setup()
{
  setCpuFrequencyMhz(80); // CPU周波数変更
#ifdef debug
  Serial.begin(115200); // デバック用等
#endif
  FastLED.addLeds<NEOPIXEL, LED_pin>(leds, led_quantity); // LED初期化
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

  // 復帰モード判定
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER)
  {
    // タイマー復帰
    reference_time_send(millis()); // 問合せ、時間送信
    for (uint8_t i = 0; i < 100; i++)
    {
      set_status = task_read();
      delay(10);
      if (set_status != -1)
      {
        break;
      }
    }
    if (set_status == 30)
    {
      // スタートの場合
      boot_count = 0;
    }
    else
    {
      // 待機の場合
      boot_count++;
      if (boot_count == 9)
      {
        led_set(255, 0, 0);
      }
      else if (boot_count == 10)
      {
        boot_count = 0;
        led_set(0, 0, 0);
      }

      sleep_start();
    }
  }
  else
  {
    // その他(電源投入時を想定)
    // 起動したので赤を点灯
    led_set(255, 0, 0);

    delay(1000); // 赤を1秒表示

    // 初期設定トリガ
    if (digitalRead(Btn_pin) == LOW)
      master_set();
#ifdef debug
    Serial.println("setup end");
#endif

    // 起動後、初期設定をしなかったらスリープに入る
    led_set(0, 0, 0);
    sleep_start();
  }
}

void loop()
{
  set_status = task_read();

  switch (set_status)
  {
  case 20:
#ifdef debug
    Serial.println("rec 20");
#endif
    led_set(0, 0, 0);
    set_status = 0;
    break;

  case 21:
#ifdef debug
    Serial.println("rec 21");
#endif
    led_set(255, 255, 0);
    set_status = 0;
    break;

  case 22:
#ifdef debug
    Serial.println("rec 22");
#endif
    led_set(255, 0, 0);
    set_status = 0;

    break;

  case 23:
#ifdef debug
    Serial.println("rec 23");
#endif
    led_set(0, 0, 255);
    set_status = 0;

    break;

  case 30:
#ifdef debug
    Serial.println("rec 30");
#endif
    set_start();
    Btn_trig = 0;
    set_status = 0;

    break;

  case 31:
#ifdef debug
    Serial.println("set end1");
#endif
    set_end();
    sleep_start();
    set_status = 0;
    break;

  default:
    break;
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
    if (send_trig[i] == 1)
    {
      send_time(&i, sensor_time[i]);
    }
    send_trig[i] = 2;
    if (send_trig[i] == 2)
    {
      // 一回感知してからの待機時間 1000ms
      if (millis() - sensor_time[i] > 1000)
        send_trig[i] = 0;
    }
  }

  delay(1);
}
