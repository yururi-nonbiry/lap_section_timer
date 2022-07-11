// 320 * 280
// 320 * 40

// 機種切り替え
// M5Core2を使用する場合はコメントアウトを外す
#define _M5Core2

#ifdef _M5Core2
#include <M5Core2.h>
#include <AXP192.h>
AXP192 power;
#endif

#include <esp_now.h>
#include <WiFi.h>
#include <EEPROM.h>

// デバック用
#define debug

// モード判定
// 1:スタート準備待機
volatile uint8_t device_status = 0;

// EEPROMの構造体
struct set_data
{
  // 20の内ブロードキャストで1個使用する
  // 19は遠隔操作用アドレス(リモコン)
  // 18は遠隔操作用アドレス(LEDバー)
  uint8_t slave_mac_address1[19]; // slave_MACアドレス
  uint8_t slave_mac_address2[19]; // slave_MACアドレス
  uint8_t slave_mac_address3[19]; // slave_MACアドレス
  uint8_t slave_mac_address4[19]; // slave_MACアドレス
  uint8_t slave_mac_address5[19]; // slave_MACアドレス
  uint8_t slave_mac_address6[19]; // slave_MACアドレス
};

set_data eeprom_date; // 構造体宣言

// 受信した時間関係
volatile uint8_t time_list_number = 0;

struct receive_time
{
  uint8_t unit;
  uint8_t lane;
  uint32_t time;
};

volatile receive_time time_list[100];

void receive_time_reset()
{
  for (uint8_t i = 0; i < 100; i++)
  {
    time_list[i].unit = 255;
    time_list[i].lane = 0;
    time_list[i].time = 0;
  }
}

volatile uint8_t slave_count_temp = 0;

// 各ユニットのステータス収納
volatile uint8_t unit_status[18] = {0};

// 各ユニットの時間収納
volatile uint32_t comp_time[18] = {0};

// 各ユニットのレスポンスタイム収納
volatile uint32_t send_time = 0;
volatile uint32_t receve_time = 0;
volatile uint32_t response_time[18] = {0};

// スタートした時間の格納
volatile uint32_t start_time = 0;

// スタートトリガを開始した時間
volatile uint32_t start_trig_time = 0;

// 各種トリガ (受信したときにルーチンを解放する為)
volatile bool start_trig = false;
volatile bool stop_trig = false;
volatile bool signal_trig = false;
volatile bool response_trig[17] = {false};

// 受信した結果の構造体
struct receive_data
{
  // 20の内ブロードキャストで1個使用する
  uint8_t number; // slave_MACアドレス
  uint8_t lane;   // slave_MACアドレス
  uint32_t time;  // slave_MACアドレス
};
volatile receive_data receive_data_list[5];
// uint8_t receive_data_list[10]

// EEPROMリセット
void eeprom_reset()
{
  delay(1000); // 一定時間待ち(連続処理防止)

  for (size_t i = 0; i < 19; i++)
  {
    eeprom_date.slave_mac_address1[i] = 0xff;
    eeprom_date.slave_mac_address2[i] = 0xff;
    eeprom_date.slave_mac_address3[i] = 0xff;
    eeprom_date.slave_mac_address4[i] = 0xff;
    eeprom_date.slave_mac_address5[i] = 0xff;
    eeprom_date.slave_mac_address6[i] = 0xff;
  }
  EEPROM.put<set_data>(0, eeprom_date); // EEPROMへの書き込み
  EEPROM.commit();                      // これが必要らしい
}

// EEPROMへ書き込み
void eeprom_write()
{
  EEPROM.put<set_data>(0, eeprom_date); // EEPROMへの書き込み
  EEPROM.commit();                      // これが必要らしい
}

#ifdef _M5Core2
// タッチパネル関係
ButtonColors cl_on = {0x7BEF, WHITE, WHITE};   // タップした時の色 (背景, 文字列, ボーダー)
ButtonColors cl_off = {BLACK, 0xC618, 0xC618}; // 指を離した時の色 (背景, 文字列, ボーダー)

// ボタン定義名( X軸, Y軸, 横幅, 高さ, 回転, ボタンのラベル, 指を離した時の色指定, タッチした時の色指定）
Button btn_start(0, 0, 0, 0, false, "Start", cl_off, cl_on);
Button btn_stop(0, 0, 0, 0, false, "Stop", cl_off, cl_on);
Button btn_signal(0, 0, 0, 0, false, "Signal", cl_off, cl_on);
Button btn_up(0, 0, 0, 0, false, "Up", cl_off, cl_on);
Button btn_down(0, 0, 0, 0, false, "Down", cl_off, cl_on);
Button btn_set1(0, 0, 0, 0, false, "Set1", cl_off, cl_on);
Button btn_set2(0, 0, 0, 0, false, "Set2", cl_off, cl_on);
Button btn_main(0, 0, 0, 0, false, "main", cl_off, cl_on);
Button btn_reset(0, 0, 0, 0, false, "reset", cl_off, cl_on);
Button btn_register_01(0, 0, 0, 0, false, "1", cl_off, cl_on);
Button btn_register_02(0, 0, 0, 0, false, "2", cl_off, cl_on);
Button btn_register_03(0, 0, 0, 0, false, "3", cl_off, cl_on);
Button btn_register_04(0, 0, 0, 0, false, "4", cl_off, cl_on);
Button btn_register_05(0, 0, 0, 0, false, "5", cl_off, cl_on);
Button btn_register_06(0, 0, 0, 0, false, "6", cl_off, cl_on);
Button btn_register_07(0, 0, 0, 0, false, "7", cl_off, cl_on);
Button btn_register_08(0, 0, 0, 0, false, "8", cl_off, cl_on);
Button btn_register_09(0, 0, 0, 0, false, "9", cl_off, cl_on);
Button btn_register_10(0, 0, 0, 0, false, "10", cl_off, cl_on);
Button btn_register_11(0, 0, 0, 0, false, "11", cl_off, cl_on);
Button btn_register_12(0, 0, 0, 0, false, "12", cl_off, cl_on);
Button btn_register_13(0, 0, 0, 0, false, "13", cl_off, cl_on);
Button btn_register_14(0, 0, 0, 0, false, "14", cl_off, cl_on);
Button btn_register_15(0, 0, 0, 0, false, "15", cl_off, cl_on);
Button btn_register_16(0, 0, 0, 0, false, "16", cl_off, cl_on);
Button btn_register_17(0, 0, 0, 0, false, "17", cl_off, cl_on);
Button btn_register_led(0, 0, 0, 0, false, "LED", cl_off, cl_on);
Button btn_register_ctrl(0, 0, 0, 0, false, "Ctrl", cl_off, cl_on);
Button btn_slave(0, 0, 0, 0, false, "slave", cl_off, cl_on);
Button btn_slave_set(0, 0, 0, 0, false, "set", cl_off, cl_on);
Button btn_slave_reset(0, 0, 0, 0, false, "reset", cl_off, cl_on);
Button btn_slave_add(0, 0, 0, 0, false, "+", cl_off, cl_on);
Button btn_slave_sub(0, 0, 0, 0, false, "-", cl_off, cl_on);
// Button btn_save(0, 0, 0, 0, false, "save", cl_off, cl_on);

// メイン画面描画
void main_draw()
{
  M5.Lcd.fillScreen(BLACK); // 画面初期化
  btn_start.set(10, 200, 120, 40);
  btn_stop.set(170, 200, 120, 40);
  btn_signal.set(170, 5, 120, 40);
  btn_up.set(280, 70, 40, 50);
  btn_down.set(280, 140, 40, 50);
  btn_set1.set(5, 5, 80, 40);
  btn_set2.set(-1, -1);
  btn_main.set(-1, -1);
  btn_reset.set(-1, -1);
  btn_register_01.set(-1, -1);
  btn_register_02.set(-1, -1);
  btn_register_03.set(-1, -1);
  btn_register_04.set(-1, -1);
  btn_register_05.set(-1, -1);
  btn_register_06.set(-1, -1);
  btn_register_07.set(-1, -1);
  btn_register_08.set(-1, -1);
  btn_register_09.set(-1, -1);
  btn_register_10.set(-1, -1);
  btn_register_11.set(-1, -1);
  btn_register_12.set(-1, -1);
  btn_register_13.set(-1, -1);
  btn_register_14.set(-1, -1);
  btn_register_15.set(-1, -1);
  btn_register_16.set(-1, -1);
  btn_register_17.set(-1, -1);
  btn_register_led.set(-1, -1);
  btn_register_ctrl.set(-1, -1);
  btn_slave.set(-1, -1);
  btn_slave_set.set(-1, -1);
  btn_slave_reset.set(-1, -1);
  btn_slave_add.set(-1, -1);
  btn_slave_sub.set(-1, -1);
  // btn_save.set(-1, -1);

  // M5.Buttons.draw(); // ボタンを描画
  btn_start.draw();
  btn_stop.draw();
  btn_signal.draw();
  btn_up.draw();
  btn_down.draw();
  btn_set1.draw();

  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(1);

  if (device_status == 1)
  {
    for (int16_t i = 0; i < 18; i++)
    {
      if (i < 9)
      {
        M5.Lcd.setCursor(10, 60 + 16 * i);
      }
      else
      {
        M5.Lcd.setCursor(140, 60 + 16 * (i - 9));
      }

      M5.Lcd.print(F("Unit:"));
      M5.Lcd.print(i + 1);
      M5.Lcd.print(F(","));
      switch (unit_status[i])
      {
      case 0:
        M5.Lcd.print(F("-"));
        break;

      case 1:
        M5.Lcd.print(F("OK"));
        break;

      case 2:
        M5.Lcd.print(F("NG"));
        break;

      default:
        break;
      }
    }
  }
  else
  {
    M5.Lcd.setCursor(0, 80);
    for (uint8_t i = 0; i < 5; i++)
    {
      uint8_t count_temp = i + time_list_number * 5;
      if (time_list[count_temp].unit != 255)
      {
        float time_sec = float(time_list[count_temp].time) / 1000;
        M5.Lcd.print(F(""));
        M5.Lcd.print(count_temp);
        M5.Lcd.print(F(" "));
        M5.Lcd.print(time_list[count_temp].unit + 1);
        M5.Lcd.print(F("-"));
        M5.Lcd.print(time_list[count_temp].lane + 1);
        M5.Lcd.print(F(" "));
        M5.Lcd.println(String(time_sec, 3));
      }
    }
  }
}

// セット画面描画
void set1_draw()
{

  btn_start.set(-1, -1);
  btn_stop.set(-1, -1);
  btn_signal.set(-1, -1);
  btn_up.set(-1, -1);
  btn_down.set(-1, -1);
  btn_set1.set(-1, -1);
  btn_set2.set(5, 10, 80, 40);
  btn_main.set(-1, -1);
  btn_reset.set(-1, -1);
  btn_register_01.set(15, 80, 50, 40);
  btn_register_02.set(75, 80, 50, 40);
  btn_register_03.set(135, 80, 50, 40);
  btn_register_04.set(195, 80, 50, 40);
  btn_register_05.set(255, 80, 50, 40);
  btn_register_06.set(15, 140, 50, 40);
  btn_register_07.set(75, 140, 50, 40);
  btn_register_08.set(135, 140, 50, 40);
  btn_register_09.set(195, 140, 50, 40);
  btn_register_10.set(255, 140, 50, 40);
  btn_register_11.set(-1, -1);
  btn_register_12.set(-1, -1);
  btn_register_13.set(-1, -1);
  btn_register_14.set(-1, -1);
  btn_register_15.set(-1, -1);
  btn_register_16.set(-1, -1);
  btn_register_17.set(-1, -1);
  btn_register_led.set(-1, -1);
  btn_register_ctrl.set(-1, -1);
  btn_slave.set(-1, -1);
  btn_slave_set.set(-1, -1);
  btn_slave_reset.set(-1, -1);
  btn_slave_add.set(-1, -1);
  btn_slave_sub.set(-1, -1);
  // btn_save.set(-1, -1);

  M5.Lcd.fillScreen(BLACK); // 画面初期化
  // M5.Buttons.draw(); // ボタンを描画
  btn_set2.draw();
  btn_register_01.draw();
  btn_register_02.draw();
  btn_register_03.draw();
  btn_register_04.draw();
  btn_register_05.draw();
  btn_register_06.draw();
  btn_register_07.draw();
  btn_register_08.draw();
  btn_register_09.draw();
  btn_register_10.draw();
}

// セット画面描画
void set2_draw()
{
  M5.Lcd.fillScreen(BLACK); // 画面初期化
  btn_start.set(-1, -1);
  btn_stop.set(-1, -1);
  btn_signal.set(-1, -1);
  btn_up.set(-1, -1);
  btn_down.set(-1, -1);
  btn_set1.set(-1, -1);
  btn_set2.set(-1, -1);
  btn_main.set(5, 10, 80, 40);
  btn_reset.set(135, 200, 100, 40);
  btn_register_01.set(-1, -1);
  btn_register_02.set(-1, -1);
  btn_register_03.set(-1, -1);
  btn_register_04.set(-1, -1);
  btn_register_05.set(-1, -1);
  btn_register_06.set(-1, -1);
  btn_register_07.set(-1, -1);
  btn_register_08.set(-1, -1);
  btn_register_09.set(-1, -1);
  btn_register_10.set(-1, -1);
  btn_register_11.set(15, 80, 50, 40);
  btn_register_12.set(75, 80, 50, 40);
  btn_register_13.set(135, 80, 50, 40);
  btn_register_14.set(195, 80, 50, 40);
  btn_register_15.set(255, 80, 50, 40);
  btn_register_16.set(15, 140, 50, 40);
  btn_register_17.set(75, 140, 50, 40);
  btn_register_led.set(135, 140, 50, 40);
  btn_register_ctrl.set(195, 140, 50, 40);
  btn_slave.set(255, 140, 50, 40);
  btn_slave_set.set(-1, -1);
  btn_slave_reset.set(-1, -1);
  btn_slave_add.set(-1, -1);
  btn_slave_sub.set(-1, -1);
  // btn_save.set(-1, -1);
  //  M5.Buttons.draw(); // ボタンを描画
  btn_main.draw();
  btn_reset.draw();
  btn_register_11.draw();
  btn_register_12.draw();
  btn_register_13.draw();
  btn_register_14.draw();
  btn_register_15.draw();
  btn_register_16.draw();
  btn_register_17.draw();
  btn_register_led.draw();
  btn_register_ctrl.draw();
  btn_slave.draw();
}

// セット画面描画
void slave_draw()
{
  M5.Lcd.fillScreen(BLACK); // 画面初期化
  btn_start.set(-1, -1);
  btn_stop.set(-1, -1);
  btn_signal.set(-1, -1);
  btn_up.set(-1, -1);
  btn_down.set(-1, -1);
  btn_set1.set(-1, -1);
  btn_set2.set(-1, -1);
  btn_main.set(5, 10, 80, 40);
  btn_reset.set(-1, -1);
  btn_register_01.set(-1, -1);
  btn_register_02.set(-1, -1);
  btn_register_03.set(-1, -1);
  btn_register_04.set(-1, -1);
  btn_register_05.set(-1, -1);
  btn_register_06.set(-1, -1);
  btn_register_07.set(-1, -1);
  btn_register_08.set(-1, -1);
  btn_register_09.set(-1, -1);
  btn_register_10.set(-1, -1);
  btn_register_11.set(-1, -1);
  btn_register_12.set(-1, -1);
  btn_register_13.set(-1, -1);
  btn_register_14.set(-1, -1);
  btn_register_15.set(-1, -1);
  btn_register_16.set(-1, -1);
  btn_register_17.set(-1, -1);
  btn_register_led.set(-1, -1);
  btn_register_ctrl.set(-1, -1);
  btn_slave.set(-1, -1);
  btn_slave_set.set(15, 100, 50, 50);
  btn_slave_reset.set(115, 100, 50, 50);
  btn_slave_add.set(255, 50, 50, 50);
  btn_slave_sub.set(255, 150, 50, 50);
  // btn_save.set(80, 150, 50, 50);
  //  M5.Buttons.draw(); // ボタンを描画
  btn_main.draw();
  btn_slave_set.draw();
  btn_slave_reset.draw();
  btn_slave_add.draw();
  btn_slave_sub.draw();
  // btn_save.draw();

  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(100, 40);
  M5.Lcd.print(slave_count_temp + 1);
  M5.Lcd.setCursor(100, 60);
  M5.Lcd.print(eeprom_date.slave_mac_address1[slave_count_temp], HEX);
  M5.Lcd.print(eeprom_date.slave_mac_address2[slave_count_temp], HEX);
  M5.Lcd.print(eeprom_date.slave_mac_address3[slave_count_temp], HEX);
  M5.Lcd.print(eeprom_date.slave_mac_address4[slave_count_temp], HEX);
  M5.Lcd.print(eeprom_date.slave_mac_address5[slave_count_temp], HEX);
  M5.Lcd.print(eeprom_date.slave_mac_address6[slave_count_temp], HEX);
}

// バイブレーション実行関数
void vibration()
{
  M5.Axp.SetLed(true);
  power.SetLDOEnable(3, true); // 3番をtrueにしてバイブレーション開始
  delay(75);                   // バイブレーションの長さ（ms）はお好みで調整
  M5.Axp.SetLed(false);
  power.SetLDOEnable(3, false); // 3番をfalseにしてバイブレーション修了
}
#endif

void receive_time_append(uint8_t unit, uint8_t lane, uint32_t time)
{
  for (uint8_t i = 0; i < 100; i++)
  {
    if (time_list[i].unit == 255)
    {
      time_list[i].unit = unit;
      time_list[i].lane = lane;
      time_list[i].time = time;
      time_list_number = float(i / 5);

      break;
    }
  }
  main_draw();
}

void signal_send(uint8_t signal_no)
{
  // 20:消灯
  // 21:黄色
  // 22:赤色
  // 23:緑色
  uint8_t _unit_no = 17;

  if (eeprom_date.slave_mac_address1[_unit_no] != 0xff ||
      eeprom_date.slave_mac_address2[_unit_no] != 0xff ||
      eeprom_date.slave_mac_address3[_unit_no] != 0xff ||
      eeprom_date.slave_mac_address4[_unit_no] != 0xff ||
      eeprom_date.slave_mac_address5[_unit_no] != 0xff ||
      eeprom_date.slave_mac_address6[_unit_no] != 0xff)
  {

    // 待機信号送信
    uint8_t send_mac_add[6] = {
        eeprom_date.slave_mac_address1[_unit_no],
        eeprom_date.slave_mac_address2[_unit_no],
        eeprom_date.slave_mac_address3[_unit_no],
        eeprom_date.slave_mac_address4[_unit_no],
        eeprom_date.slave_mac_address5[_unit_no],
        eeprom_date.slave_mac_address6[_unit_no]};
    uint8_t data[1] = {signal_no};
    esp_err_t result = esp_now_send(send_mac_add, data, sizeof(data));
  }
}

esp_now_peer_info_t slave;
// 送信コールバック
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  char macStr[18];
#ifdef debug
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
#endif
}

// 受信コールバック
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len)
{
  uint32_t _receive_time = millis();
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
#endif

  // ペアリングは未登録の返信なので先に処理する
  if (data[0] == 1)
  {
    if (slave_count_temp == data[1])
    {
      eeprom_date.slave_mac_address1[slave_count_temp] = mac_addr[0];
      eeprom_date.slave_mac_address2[slave_count_temp] = mac_addr[1];
      eeprom_date.slave_mac_address3[slave_count_temp] = mac_addr[2];
      eeprom_date.slave_mac_address4[slave_count_temp] = mac_addr[3];
      eeprom_date.slave_mac_address5[slave_count_temp] = mac_addr[4];
      eeprom_date.slave_mac_address6[slave_count_temp] = mac_addr[5];
    }
    return;
  }

  // 受信したmacアドレスが登録済みか確認する
  uint8_t _receive_no = 255;
  for (int i = 0; i < 19; i++)
  {
    if (mac_addr[0] == eeprom_date.slave_mac_address1[i] &&
        mac_addr[1] == eeprom_date.slave_mac_address2[i] &&
        mac_addr[2] == eeprom_date.slave_mac_address3[i] &&
        mac_addr[3] == eeprom_date.slave_mac_address4[i] &&
        mac_addr[4] == eeprom_date.slave_mac_address5[i] &&
        mac_addr[5] == eeprom_date.slave_mac_address6[i])
    {
      _receive_no = i;
      break;
    }
  }

  // 登録してある子機の場合の処理
  if (_receive_no != 255)
  {
    uint32_t _receive_time_data;
    // ヘッダーで処理を分ける
    switch (data[0])
    {

    // スタート準備信号
    case 10:
      start_trig = true;
      break;

    // シグナル信号
    case 11:
      signal_trig = true;
      break;

    // ストップ信号
    case 12:
      stop_trig = true;
      break;

    // 現在時間受信
    case 32:
      if (start_trig_time != 0)
      {
        receve_time = millis();
        _receive_time_data = uint32_t(data[2] << 24) + uint32_t(data[3] << 16) + uint32_t(data[4] << 8) + uint32_t(data[5]);
        comp_time[_receive_no] = _receive_time - _receive_time_data;

        // 測定開始信号送信
        uint8_t send_mac_add[6] = {mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]};
        uint8_t data[1] = {30};
        esp_err_t result = esp_now_send(send_mac_add, data, sizeof(data));
      }
      else
      {
        // 待機信号送信
        uint8_t send_mac_add[6] = {mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]};
        uint8_t data[1] = {31};
        esp_err_t result = esp_now_send(send_mac_add, data, sizeof(data));
      }

      break;

    // 通過時間受信
    case 33:
      for (int i = 0; i < 5; i++)
      {
        if (receive_data_list[i].number == 255)
        {
          _receive_time_data = uint32_t(data[3] << 24) + uint32_t(data[4] << 16) + uint32_t(data[5] << 8) + uint32_t(data[6]);
          // Serial.println(_receive_time_data);
          receive_data_list[i].number = data[1];
          receive_data_list[i].lane = data[2];
          receive_data_list[i].time = _receive_time_data + comp_time[_receive_no];
          if (start_time == 0)
          {
            start_time = receive_data_list[i].time;
            device_status = 0; // デバイス：通常
          }

          break;
        }
      }

      break;

    default:
      break;
    }
  }
}

// macアドレスセット
void esp_now_mac_set()
{
  // マルチキャスト用Slave登録
  memset(&slave, 0, sizeof(slave));
  for (int i = 0; i < 6; ++i)
  {
    slave.peer_addr[i] = (uint8_t)0xff;
  }
  esp_err_t addStatus = esp_now_add_peer(&slave);
  for (int i = 0; i < 19; i++)
  {
    slave.peer_addr[0] = eeprom_date.slave_mac_address1[i];
    slave.peer_addr[1] = eeprom_date.slave_mac_address2[i];
    slave.peer_addr[2] = eeprom_date.slave_mac_address3[i];
    slave.peer_addr[3] = eeprom_date.slave_mac_address4[i];
    slave.peer_addr[4] = eeprom_date.slave_mac_address5[i];
    slave.peer_addr[5] = eeprom_date.slave_mac_address6[i];
    esp_err_t addStatus = esp_now_add_peer(&slave);
  }
  if (addStatus == ESP_OK)
  {
    // Pair success
    Serial.println("Pair success");
  }
  // ESP-NOWコールバック登録
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);
}

// スタート時の処理
bool start()
{
  device_status = 1; // デバイス：スタート待機
  // signal_send(21);
  // 受信した時間をリセット
  receive_time_reset();
  time_list_number = 0;
  bool result = false;

  // 受信ログ削除
  for (int i = 0; i < 5; i++)
  {
    receive_data_list[i].number = 255;
  }

  // 受信した時間をリセット
  for (int i = 0; i < (sizeof(comp_time) / sizeof(comp_time[0])); i++)
  {
    comp_time[i] = 0;
  }

  start_time = 0;

  start_trig_time = millis(); // スタート準備した時間をセット

  // 全子機の問い合わせ待ち
  for (uint8_t j = 0; j < 20; j++)
  {
    delay(100);
    result = true;
    for (int i = 0; i < 18; i++)
    {
      if (eeprom_date.slave_mac_address1[i] == 255 &&
          eeprom_date.slave_mac_address2[i] == 255 &&
          eeprom_date.slave_mac_address3[i] == 255 &&
          eeprom_date.slave_mac_address4[i] == 255 &&
          eeprom_date.slave_mac_address5[i] == 255 &&
          eeprom_date.slave_mac_address6[i] == 255)
      {
        // 登録されていない子機なので次に行く
        unit_status[i] = 0; // 登録なしは"0"
        continue;
      }
      else
      {
        // 子機の時間が登録されていない場合はfor文を繰り返して待つ
        if (comp_time[i] == 0)
        {
          unit_status[i] = 2; // 応答待ちは"2"
          result = false;
          // break;
        }
        else
        {
          unit_status[i] = 1; // 応答済みは"1"
        }
      }
    }
    if (result == true)
      break;
  }
  for (uint8_t i = 0; i < 18; i++)
  {
    Serial.print(F("unit,"));
    switch (unit_status[i])
    {
    case 0:
      Serial.println(F("none;"));
      break;

    case 1:
      Serial.println(F("ok;"));
      break;

    case 2:
      Serial.println(F("ng;"));
      break;

    default:
      break;
    }
  }

  delay(100);
  start_trig_time = 0; // スタート準備した時間をセット
  if (result == true)
  {
    signal_send(22); // 緑点灯
  }
  else
  {
    signal_send(21); // 黄点灯
  }

  main_draw();

  return result;
}

// stop時の処理
void stop()
{

  uint8_t data[] = {1};
  for (uint8_t i = 0; i < 19; i++)
  {
    uint8_t data[] = {31, i};
    if (eeprom_date.slave_mac_address1[i] != 0xff ||
        eeprom_date.slave_mac_address2[i] != 0xff ||
        eeprom_date.slave_mac_address3[i] != 0xff ||
        eeprom_date.slave_mac_address4[i] != 0xff ||
        eeprom_date.slave_mac_address5[i] != 0xff ||
        eeprom_date.slave_mac_address6[i] != 0xff)
    {
      uint8_t send_address[6];
      send_address[0] = eeprom_date.slave_mac_address1[i];
      send_address[1] = eeprom_date.slave_mac_address2[i];
      send_address[2] = eeprom_date.slave_mac_address3[i];
      send_address[3] = eeprom_date.slave_mac_address4[i];
      send_address[4] = eeprom_date.slave_mac_address5[i];
      send_address[5] = eeprom_date.slave_mac_address6[i];

      esp_err_t result = esp_now_send(slave.peer_addr, data, sizeof(data));
    }
  }
  signal_send(20);
  device_status = 0; // デバイス：通常
}

void signal()
{
  signal_send(23);
}

void slave_set(uint8_t number)
{
  uint8_t send_mac_add[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  uint8_t data[2] = {0, number};
  esp_err_t result = esp_now_send(send_mac_add, data, sizeof(data));
}

void slave_reset(uint8_t number)
{
  eeprom_date.slave_mac_address1[number] = 0xff;
  eeprom_date.slave_mac_address2[number] = 0xff;
  eeprom_date.slave_mac_address3[number] = 0xff;
  eeprom_date.slave_mac_address4[number] = 0xff;
  eeprom_date.slave_mac_address5[number] = 0xff;
  eeprom_date.slave_mac_address6[number] = 0xff;
}

#ifdef _M5Core2
// タッチパネルイベント
void event_btn_start(Event &e)
{
  vibration();
  if (e.type == E_TOUCH)
  {
    start_trig = true;
  }
  else
  {
  }
}

void event_btn_stop(Event &e)
{
  vibration();

  if (e.type == E_TOUCH)
  {
    stop_trig = true;
  }
  else
  {
  }
}

void event_btn_signal(Event &e)
{
  vibration();

  if (e.type == E_TOUCH)
  {
    signal_trig = true;
  }
  else
  {
  }
}

void event_btn_up(Event &e)
{
  vibration();

  if (e.type == E_TOUCH)
  {
    if (time_list_number != 19)
      time_list_number++;
    main_draw();
  }
  else
  {
  }
}

void event_btn_down(Event &e)
{
  vibration();

  if (e.type == E_TOUCH)
  {
    if (time_list_number != 0)
      time_list_number--;
    main_draw();
  }
  else
  {
  }
}

// EEPROMリセット
void event_btn_reset(Event &e)
{
  vibration();
  if (e.type == E_TOUCH)
  {
    eeprom_reset();
  }
  else
  {
  }
}

void event_btn_set1(Event &e)
{
  vibration();
  if (e.type == E_TOUCH)
  {
  }
  else
  {
    set1_draw();
  }
}

void event_btn_set2(Event &e)
{
  vibration();
  if (e.type == E_TOUCH)
  {
  }
  else
  {
    set2_draw();
  }
}

void event_btn_main(Event &e)
{
  vibration();
  if (e.type == E_TOUCH)
  {
  }
  else
  {
    main_draw();
  }
}

void event_btn_slave(Event &e)
{
  vibration();
  if (e.type == E_TOUCH)
  {
    slave_draw();
  }
  else
  {
  }
}

void event_btn_slave_set(Event &e)
{
  vibration();
  if (e.type == E_TOUCH)
  {
    slave_set(slave_count_temp);
    delay(500);
    eeprom_write();
    esp_now_mac_set();

    slave_draw();
  }
  else
  {
  }
}

void event_btn_slave_reset(Event &e)
{
  vibration();
  if (e.type == E_TOUCH)
  {
    slave_reset(slave_count_temp);
    slave_draw();
  }
  else
  {
  }
}

void event_btn_slave_add(Event &e)
{
  vibration();
  if (e.type == E_TOUCH)
  {
    if (slave_count_temp != 19)
      slave_count_temp++;
    slave_draw();
  }
  else
  {
  }
}

void event_btn_slave_sub(Event &e)
{
  vibration();
  if (e.type == E_TOUCH)
  {
    if (slave_count_temp != 0)
      slave_count_temp--;
    slave_draw();
  }
  else
  {
  }
}

// void event_btn_save(Event &e)
//{
//   vibration();
//   if (e.type == E_TOUCH)
//   {
//     eeprom_write();
//     esp_now_mac_set();
//   }
//  else
//   {
//   }
// }

// 登録
void do_register(uint8_t data)
{
  vibration();
  slave_count_temp = data;
  slave_draw();
}

// ボタンイベント
void event_register_01(Event &e) { do_register(0); }
void event_register_02(Event &e) { do_register(1); }
void event_register_03(Event &e) { do_register(2); }
void event_register_04(Event &e) { do_register(3); }
void event_register_05(Event &e) { do_register(4); }
void event_register_06(Event &e) { do_register(5); }
void event_register_07(Event &e) { do_register(6); }
void event_register_08(Event &e) { do_register(7); }
void event_register_09(Event &e) { do_register(8); }
void event_register_10(Event &e) { do_register(9); }
void event_register_11(Event &e) { do_register(10); }
void event_register_12(Event &e) { do_register(11); }
void event_register_13(Event &e) { do_register(12); }
void event_register_14(Event &e) { do_register(13); }
void event_register_15(Event &e) { do_register(14); }
void event_register_16(Event &e) { do_register(15); }
void event_register_17(Event &e) { do_register(16); }
void event_register_led(Event &e) { do_register(17); }
void event_register_ctrl(Event &e) { do_register(18); }

#endif

// セッティング用サブルーチン
void serial_read()
{

  // 変数定義
  String read_serial = "";
  Serial.setTimeout(100);

  read_serial = Serial.readStringUntil(';');

  // スペースと改行を消す
  int search_number = 0; // 文字検索用検索
  while (search_number != -1)
  {                                               // 消したい文字が見つからなくなるまで繰り返す
    search_number = read_serial.lastIndexOf(" "); // スペースを検索する
    if (search_number = -1)
      search_number = read_serial.lastIndexOf("\n"); // 改行を検索する
    if (search_number != -1)
      read_serial.remove(search_number);
  }

  // 受信したときの処置
  if (read_serial != "")
  { // 文字が入っていたら実行する
    int comma_position = read_serial.indexOf(",");
    if (read_serial.equalsIgnoreCase("help") == true)
    {
      // ここにヘルプの内容を書き込む
      Serial.println(F("help:"));
    }
    else if (read_serial.equalsIgnoreCase("list") == true)
    {
      //ここに設定値のリストを書き込む
      Serial.println(F("list:"));
    }
    else if (read_serial.equalsIgnoreCase("save") == true)
    {
      //ここに設定値のリストを書き込む
      eeprom_write();
      esp_now_mac_set();
      Serial.println(F("save_ok;"));
    }
    else if (read_serial.equalsIgnoreCase("reset_all") == true)
    {
      //ここに設定値のリストを書き込む
      eeprom_reset();
      Serial.println(F("reset_all_ok;"));
    }
    else if (read_serial.equalsIgnoreCase("start") == true)
    {
      //ここに設定値のリストを書き込む
      start();
      Serial.println(F("start_ok;"));
    }
    else if (read_serial.equalsIgnoreCase("stop") == true)
    {
      //ここに設定値のリストを書き込む
      stop();
      Serial.println(F("stop_ok;"));
    }
    else if (comma_position != -1)
    {                                                                                     // ","があるか判定する
      String item_str = read_serial.substring(0, comma_position);                         // 項目読み取り
      String value_str = read_serial.substring(comma_position + 1, read_serial.length()); // 数値読み取り
      int16_t int_data;

      if (item_str == "set")
      {
        int_data = value_str.toInt();
        Serial.print(F("set_ok,"));
        Serial.print(value_str);
        Serial.println(F(";"));
        slave_set(int_data);
      }

      if (item_str == "reset")
      {
        int_data = value_str.toInt();
        Serial.print(F("reset_ok,"));
        Serial.print(value_str);
        Serial.println(F(";"));
        slave_reset(int_data);
      }
    }
  }
  read_serial = ""; //　クリア
}

void setup()
{
  receive_time_reset();

  // 受信ログ初期化
  for (int i = 0; i < 5; i++)
  {
    receive_data_list[i].number = 255;
  }

#ifdef _M5Core2
  M5.begin(true, true, false, true); // Core2初期化

  // EEPROM 読み込み
  EEPROM.begin(1024);                   // EEPROM開始(サイズ指定)
  EEPROM.get<set_data>(0, eeprom_date); // EEPROMを読み込む

  M5.Lcd.setBrightness(120);
  M5.Lcd.fillScreen(BLACK); // 画面初期化
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(2);

  // タッチパネル用
  M5.Buttons.setFont(FSSB12);                                   // 全てのボタンのフォント指定
  btn_start.addHandler(event_btn_start, E_TOUCH + E_RELEASE);   // ボタンのイベント関数を指定
  btn_stop.addHandler(event_btn_stop, E_TOUCH + E_RELEASE);     // ボタンのイベント関数を指定
  btn_signal.addHandler(event_btn_signal, E_TOUCH + E_RELEASE); // ボタンのイベント関数を指定
  btn_up.addHandler(event_btn_up, E_TOUCH + E_RELEASE);         // ボタンのイベント関数を指定
  btn_down.addHandler(event_btn_down, E_TOUCH + E_RELEASE);     // ボタンのイベント関数を指定
  btn_set1.addHandler(event_btn_set1, E_TOUCH + E_RELEASE);     // ボタンのイベント関数を指定
  btn_set2.addHandler(event_btn_set2, E_TOUCH + E_RELEASE);     // ボタンのイベント関数を指定
  btn_main.addHandler(event_btn_main, E_TOUCH + E_RELEASE);     // ボタンのイベント関数を指定
  btn_reset.addHandler(event_btn_reset, E_TOUCH + E_RELEASE);   // ボタンのイベント関数を指定
  btn_register_01.addHandler(event_register_01, E_TOUCH + E_RELEASE);
  btn_register_02.addHandler(event_register_02, E_TOUCH + E_RELEASE);
  btn_register_03.addHandler(event_register_03, E_TOUCH + E_RELEASE);
  btn_register_04.addHandler(event_register_04, E_TOUCH + E_RELEASE);
  btn_register_05.addHandler(event_register_05, E_TOUCH + E_RELEASE);
  btn_register_06.addHandler(event_register_06, E_TOUCH + E_RELEASE);
  btn_register_07.addHandler(event_register_07, E_TOUCH + E_RELEASE);
  btn_register_08.addHandler(event_register_08, E_TOUCH + E_RELEASE);
  btn_register_09.addHandler(event_register_09, E_TOUCH + E_RELEASE);
  btn_register_10.addHandler(event_register_10, E_TOUCH + E_RELEASE);
  btn_register_11.addHandler(event_register_11, E_TOUCH + E_RELEASE);
  btn_register_12.addHandler(event_register_12, E_TOUCH + E_RELEASE);
  btn_register_13.addHandler(event_register_13, E_TOUCH + E_RELEASE);
  btn_register_14.addHandler(event_register_14, E_TOUCH + E_RELEASE);
  btn_register_15.addHandler(event_register_15, E_TOUCH + E_RELEASE);
  btn_register_16.addHandler(event_register_16, E_TOUCH + E_RELEASE);
  btn_register_17.addHandler(event_register_17, E_TOUCH + E_RELEASE);
  btn_register_led.addHandler(event_register_led, E_TOUCH + E_RELEASE);
  btn_register_ctrl.addHandler(event_register_ctrl, E_TOUCH + E_RELEASE);
  btn_slave.addHandler(event_btn_slave, E_TOUCH + E_RELEASE);
  btn_slave_set.addHandler(event_btn_slave_set, E_TOUCH + E_RELEASE);
  btn_slave_reset.addHandler(event_btn_slave_reset, E_TOUCH + E_RELEASE);
  btn_slave_add.addHandler(event_btn_slave_add, E_TOUCH + E_RELEASE);
  btn_slave_sub.addHandler(event_btn_slave_sub, E_TOUCH + E_RELEASE);
  // btn_save.addHandler(event_btn_save, E_TOUCH + E_RELEASE);

  M5.Axp.SetLed(false); // 緑LEDの消灯
#endif

  Serial.begin(115200);

  // ESP-NOW初期化
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

  esp_now_mac_set();

#ifdef _M5Core2
  //メイン画面画面
  Serial.println("main_draw");
  main_draw();
#endif
}

void loop()
{
#ifdef _M5Core2
  M5.update();
#endif
  // 各種トリガ判定
  if (start_trig == true)
  {
    start_trig = false;
    start();
  }
  if (stop_trig == true)
  {
    stop_trig = false;
    stop();
  }
  if (signal_trig == true)
  {
    signal_trig = false;
    signal();
  }

  for (uint8_t i = 0; i < 17; i++)
  {
    if (response_trig[i] == true)
    {
      response_trig[i] = false;
    }
  }

  // 受信表示
  for (int i = 0; i < 5; i++)
  {
    if (receive_data_list[i].number != 255)
    {
      receive_time_append(receive_data_list[i].number,
                          receive_data_list[i].lane,
                          receive_data_list[i].time - start_time);
      Serial.print(F("receive_time"));
      Serial.print(F(",number="));
      Serial.print(receive_data_list[i].number);
      Serial.print(F(",lane="));
      Serial.print(receive_data_list[i].lane);
      Serial.print(F(",time="));
      Serial.print(receive_data_list[i].time - start_time);
      Serial.println(F(";"));
      // 送信したら無効化する
      receive_data_list[i].number = 255;
    }
  }
  if (Serial.available() > 0)
  {
    serial_read();
  }

  delay(1);
}
