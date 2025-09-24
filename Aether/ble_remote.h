#ifndef BLE_REMOTE_H
#define BLE_REMOTE_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>

// Externs for main sketch globals
extern Adafruit_SSD1306 display;
extern void beep(uint16_t freq, uint16_t dur);
extern void readButtons();
extern bool upPressed, downPressed, okPressed;
extern bool prevUp, prevDown, prevOk;

#define UP_PIN 35
#define DOWN_PIN 32
#define OK_PIN 26

BLEHIDDevice* hid = nullptr;
BLEServer* pServer = nullptr;
BLEAdvertising* pAdvertising = nullptr;
bool bleRemoteConnected = false;

class BleRemoteCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) { 
    Serial.println("[BLE_REMOTE] Client connected.");
    bleRemoteConnected = true; 
  }
  void onDisconnect(BLEServer* pServer) { 
    Serial.println("[BLE_REMOTE] Client disconnected.");
    bleRemoteConnected = false;
    if (pAdvertising) pAdvertising->start(); // restart advertising for next connection
  }
};

void drawBleSymbol(int cx, int cy) {
  display.drawLine(cx, cy-8, cx, cy+8, SSD1306_WHITE);
  display.drawLine(cx-6, cy-4, cx+6, cy+4, SSD1306_WHITE);
  display.drawLine(cx+6, cy-4, cx-6, cy+4, SSD1306_WHITE);
  display.drawPixel(cx, cy, SSD1306_WHITE);
}

void ble_send_key(uint8_t keycode) {
  if (!hid) return;
  uint8_t report[8] = {0};
  report[2] = keycode;
  hid->inputReport(1)->setValue(report, 8);
  hid->inputReport(1)->notify();
  delay(15);
  report[2] = 0;
  hid->inputReport(1)->setValue(report, 8);
  hid->inputReport(1)->notify();
  delay(8);
}

void ble_remote_cleanup() {
  if (pAdvertising) pAdvertising->stop();
  hid = nullptr;
  pServer = nullptr;
  pAdvertising = nullptr;
  bleRemoteConnected = false;
  prevUp = prevDown = prevOk = false;
  upPressed = downPressed = okPressed = false;
  Serial.println("[BLE_REMOTE] BLE Remote cleaned up.");
}

void runBLERemote() {
  // --- FIX: Clear button state on entry ---
  readButtons();
  upPressed = downPressed = okPressed = false;
  prevUp = prevDown = prevOk = false;
  delay(120);

  Serial.println("[BLE_REMOTE] Starting BLE HID...");
  BLEDevice::init("AETHER 32");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new BleRemoteCallbacks());
  hid = new BLEHIDDevice(pServer);
  hid->manufacturer()->setValue("Aether32");
  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  hid->hidInfo(0x00, 0x01);

  const uint8_t reportMap[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01, 0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01, 0x75, 0x08,
    0x81, 0x01, 0x95, 0x05, 0x75, 0x01, 0x05, 0x08, 0x19, 0x01, 0x29, 0x05, 0x91, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x91, 0x01, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0
  };
  hid->reportMap((uint8_t*)reportMap, sizeof(reportMap));
  hid->startServices();

  pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_KEYBOARD);
  pAdvertising->addServiceUUID(hid->hidService()->getUUID());
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  pAdvertising->start();

  Serial.println("[BLE_REMOTE] Advertising started.");

  // OLED UI
  auto show_ble_screen = [](const char *status) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds("AETHER 32", 0,0, &x1, &y1, &w, &h);
    int cx = display.width()/2;
    display.setCursor((display.width()-w)/2, 2);
    display.print("AETHER 32");
    drawBleSymbol(cx, 32);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor((display.width()-strlen(status)*6)/2, 54);
    display.print(status);
    display.display();
  };

  // Wait for connection
  unsigned long lastStatus = 0;
  while (!bleRemoteConnected) {
    readButtons();
    Serial.print("[BLE_REMOTE] Waiting...  OK: "); Serial.println(okPressed);
    if (okPressed && !prevOk) {
      Serial.println("[BLE_REMOTE] OK pressed during wait, exiting.");
      beep(1000, 80);
      ble_remote_cleanup();
      return;
    }
    prevOk = okPressed;
    if (millis() - lastStatus > 400) {
      show_ble_screen("Waiting...");
      lastStatus = millis();
    }
    delay(30);
  }
  beep(1800, 80);
  Serial.println("[BLE_REMOTE] Connected!");
  show_ble_screen("Connected!");
  delay(700);

  // Control mode: map buttons to keycodes
  auto show_control_ui = []() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 2);
    display.print("IG Reels Remote");
    display.setCursor(0, 16);
    display.print("[UP] Next [DOWN] Prev");
    display.setCursor(0, 28);
    display.print("[OK] Like");
    display.setCursor(0, 54);
    display.print("Hold OK to exit.");
    drawBleSymbol(display.width()/2, 40);
    display.display();
  };

  show_control_ui();

  unsigned long okHoldStart = 0;
  while (bleRemoteConnected) {
    readButtons();
    Serial.print("[BLE_REMOTE] UP: "); Serial.print(upPressed);
    Serial.print(" DOWN: "); Serial.print(downPressed);
    Serial.print(" OK: "); Serial.println(okPressed);

    if (upPressed && !prevUp) {
      Serial.println("[BLE_REMOTE] UP pressed - sending UP ARROW");
      ble_send_key(0x52);
      beep(1200, 20);
    }
    if (downPressed && !prevDown) {
      Serial.println("[BLE_REMOTE] DOWN pressed - sending DOWN ARROW");
      ble_send_key(0x51);
      beep(1200, 20);
    }
    if (okPressed && !prevOk) {
      Serial.println("[BLE_REMOTE] OK pressed - sending SPACE x2");
      ble_send_key(0x2C);
      delay(50);
      ble_send_key(0x2C);
      beep(1900, 20);
    }

    // Long press OK to exit
    if (okPressed) {
      if (okHoldStart == 0) okHoldStart = millis();
      else if (millis() - okHoldStart > 1000) {
        Serial.println("[BLE_REMOTE] OK long press detected, exiting BLE remote.");
        beep(700, 80);
        ble_remote_cleanup();
        return;
      }
    } else okHoldStart = 0;

    prevUp = upPressed; prevDown = downPressed; prevOk = okPressed;
    show_control_ui();
    delay(15);
  }
  Serial.println("[BLE_REMOTE] BLE disconnected, cleaning up.");
  ble_remote_cleanup();
}

#endif