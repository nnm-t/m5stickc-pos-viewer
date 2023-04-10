#include <M5StickC.h>
#include <Ticker.h>
#include <BLEDevice.h>
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>

namespace {
  constexpr const char* bluetooth_name = "Wio-POS-Viewer";

  BLEUUID service_uuid(static_cast<uint16_t>(0xF6E7));
  BLEUUID price_characteristic_uuid(static_cast<uint16_t>(0x0001));

  constexpr const uint32_t color_black = 0x000000U;
  constexpr const uint32_t color_red = 0xFF0000U;
  constexpr const uint32_t color_blue = 0x0099FFU;
  constexpr const uint32_t color_green = 0x0000FFU;
  constexpr const uint32_t color_yellow = 0xFFFF00U;
  constexpr const uint32_t color_white = 0xFFFFFFU;

  LGFX lcd;
  Ticker ticker;
}

enum class BLEPosDataType : uint8_t
{
	Goods = 0,
	Amount = 1,
	Sum = 2,
	Paid = 3
};

class BLEPosServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer* pServer) override
  {
    lcd.fillRect(0, 0, 160, 16, color_white);
    lcd.setTextColor(color_blue, color_white);
    lcd.setFont(&fonts::Font0);
    lcd.drawString("Bluetooth LE: Connected", 0, 8);

    BLEDevice::stopAdvertising();

    lcd.setTextColor(color_black, color_white);
    lcd.drawString("Advertising: OFF", 0, 0);
    lcd.setFont(&fonts::lgfxJapanGothic_32);
  }

  void onDisconnect(BLEServer* pServer) override
  {
    lcd.fillRect(0, 0, 160, 16, color_white);

    BLEDevice::startAdvertising();

    lcd.setTextColor(color_black, color_white);
    lcd.setFont(&fonts::Font0);
    lcd.drawString("Advertising: ON", 0, 0);
    lcd.setFont(&fonts::lgfxJapanGothic_32);
  }
};

class BLEPosCharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic* pCharacteristic) override
  {
    std::string rxValue = pCharacteristic->getValue();
    Serial.println(String("RAW Value: ") + rxValue.data());

    lcd.fillRect(0, 16, 160, 64, color_black);

    if (rxValue.length() != 6)
    {
      lcd.setTextColor(color_white, color_black);
      lcd.setFont(&fonts::Font0);
      lcd.drawString("Invalid Value", 0, 16);
      lcd.setFont(&fonts::lgfxJapanGothic_32);
      return;
    }

    BLEPosDataType type = static_cast<BLEPosDataType>(rxValue[0]);
    int8_t number = static_cast<int8_t>(rxValue[1]);
    int32_t price = (rxValue[2] << 24) + (rxValue[3] << 16) + (rxValue[4] << 8) + rxValue[5];


    switch (type)
    {
      case BLEPosDataType::Goods:
        // 金額カウント
        lcd.setTextColor(color_yellow, color_black);
        lcd.drawString("計数 " + String(number, DEC) + "点", 0, 16);
        lcd.setTextDatum(TR_DATUM);
        lcd.drawString(String(price, DEC) + "円", 160, 48);
        break;
      case BLEPosDataType::Amount:
        // 金額入力
        // rxValue[1] 無視
        lcd.setTextColor(color_white, color_black);
        lcd.drawString("金額入力", 0, 16);
        lcd.setTextDatum(TR_DATUM);
        lcd.drawString(String(price, DEC) + "円", 160, 48);
        break;
      case BLEPosDataType::Sum:
        // 会計前 総額
        // rxValue[1] 無視
        lcd.setTextColor(color_red, color_black);
        lcd.drawString("合計", 0, 16);
        lcd.setTextDatum(TR_DATUM);
        lcd.drawString(String(price, DEC) + "円", 160, 48);
        break;
      case BLEPosDataType::Paid:
        // お買い上げありがとうございました
        // rxValue[1:5] 無視
        lcd.setTextColor(color_green, color_black);
        lcd.setTextSize(0.5f, 1);
        lcd.setTextDatum(TC_DATUM);
        lcd.drawString("ありがとうございます", 80, 48);
        break;
      default:
        lcd.setTextColor(color_white, color_black);
        lcd.setTextDatum(MC_DATUM);
        lcd.drawString("Invalid Type", 80, 48);
        break;
    }

    lcd.setTextDatum(TL_DATUM);
    lcd.setTextSize(1);
  }
};

namespace {
  BLEPosServerCallbacks server_callbacks;
  BLEPosCharacteristicCallbacks characteristic_callbacks;

  uint8_t brightness = 9;
}

void onTimerTicked()
{
  float battery_voltage = M5.Axp.GetBatVoltage();
  
  lcd.setTextDatum(TR_DATUM);
  lcd.setFont(&fonts::Font0);
  lcd.setTextColor(color_black, color_white);
  lcd.drawString(String(battery_voltage) + "V", 160, 0);
  lcd.setFont(&fonts::lgfxJapanGothic_32);
  lcd.setTextDatum(TL_DATUM);
}

void setup()
{
  M5.begin();
  lcd.init();
  lcd.setRotation(3);

  M5.Axp.ScreenBreath(brightness);

  lcd.fillRect(0, 0, 160, 16, color_white);

  BLEDevice::init(bluetooth_name);
  BLEServer* server = BLEDevice::createServer();
  BLEService* service = server->createService(service_uuid);

  BLECharacteristic* characteristic = service->createCharacteristic(price_characteristic_uuid, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  characteristic->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
  characteristic->setCallbacks(&characteristic_callbacks);
  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(service_uuid);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMinPreferred(0x12);
  server->setCallbacks(&server_callbacks);
  BLEDevice::startAdvertising();

  lcd.setTextColor(color_black, color_white);
  lcd.setFont(&fonts::Font0);
  lcd.drawString("Advertising: ON", 0, 0);
  lcd.setFont(&fonts::lgfxJapanGothic_32);

  ticker.attach_ms(1000, onTimerTicked);
}

void loop()
{
  M5.update();

  if (M5.BtnA.wasPressed())
  {
    lcd.fillRect(0, 16, 160, 64, color_black);
  }

  if (M5.BtnB.wasPressed())
  {
    M5.Axp.ScreenBreath(brightness++);
    if (brightness > 12)
    {
      brightness = 5;
    }
  }
}
