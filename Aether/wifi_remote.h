#ifndef WIFI_REMOTE_H
#define WIFI_REMOTE_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <esp_now.h>
#include <WiFi.h>

// ==== Externs for main sketch globals ====
extern Adafruit_SSD1306 display;
extern void beep(uint16_t freq, uint16_t dur);
extern void readButtons();
extern bool upPressed, downPressed, okPressed;
extern bool prevUp, prevDown, prevOk;

// ==== UI Constants ====
#define WIFI_REMOTE_SCREEN_WIDTH 128
#define WIFI_REMOTE_SCREEN_HEIGHT 64
#define WIFI_SCAN_LIMIT 8

// ==== Peer structure ====
typedef struct {
  uint8_t mac[6];
  char ssid[33];
  int8_t rssi;
  bool online;
} WifiRemotePeer;

WifiRemotePeer wifiPeers[WIFI_SCAN_LIMIT];
int wifiPeerCount = 0;
int wifiPeerIndex = 0;

// ==== Switch state ====
typedef struct {
  uint8_t switches[4];
} SwitchPayload;

SwitchPayload switchPayload = {{0,0,0,0}};
int selectedSwitch = 0;

// ==== State machine ====
enum WifiRemoteState {
  WIFI_SCAN,
  WIFI_SELECT,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_FAILED,
  WIFI_EXIT
};

// ==== ESP-NOW callbacks ====
bool wifiRemoteGotData = false;
void wifiRemote_onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len == sizeof(SwitchPayload)) {
      memcpy(&switchPayload, data, len);
      wifiRemoteGotData = true;
  }
}
void wifiRemote_onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

// ==== UI helpers ====
void wifiRemote_show_info(const char* msg, uint16_t dly=1000) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(msg, 0,0, &x1, &y1, &w, &h);
  int x = (WIFI_REMOTE_SCREEN_WIDTH-w)/2;
  int y = (WIFI_REMOTE_SCREEN_HEIGHT-h)/2;
  display.setCursor(x, y);
  display.print(msg);
  display.display();
  beep(1500, 40);
  delay(dly);
}

void wifiRemote_drawStatusBar(const char* text) {
  display.fillRect(0, 0, WIFI_REMOTE_SCREEN_WIDTH, 14, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print(text);
  display.setTextColor(SSD1306_WHITE);
}

void wifiRemote_drawSwitches() {
  int iconW = 24, iconH = 24, spacing = 6, y = 24;
  for (int i = 0; i < 4; ++i) {
    int x = 8 + i*(iconW+spacing);
    if (i == selectedSwitch)
      display.drawRect(x-2, y-2, iconW+4, iconH+4, SSD1306_WHITE);
    display.fillRect(x, y, iconW, iconH, switchPayload.switches[i] ? SSD1306_WHITE : SSD1306_BLACK);
    display.drawRect(x, y, iconW, iconH, SSD1306_WHITE);
    display.setTextColor(switchPayload.switches[i] ? SSD1306_BLACK : SSD1306_WHITE);
    display.setTextSize(2);
    display.setCursor(x+4, y+6);
    display.print(switchPayload.switches[i] ? "ON" : "OFF");
    display.setTextColor(SSD1306_WHITE);
  }
}

// ==== Main WiFi Remote function ====
void runWifiRemote() {
  WifiRemoteState state = WIFI_SCAN;
  wifiPeerCount = 0;
  wifiPeerIndex = 0;
  selectedSwitch = 0;
  wifiRemoteGotData = false;
  memset(&switchPayload, 0, sizeof(switchPayload));

  // ESP-NOW/WiFi setup
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_now_deinit();
  if (esp_now_init() != ESP_OK) {
    wifiRemote_show_info("ESP-NOW Err.", 1200);
    WiFi.mode(WIFI_OFF);
    return;
  }
  esp_now_register_recv_cb(wifiRemote_onDataRecv);
  esp_now_register_send_cb(wifiRemote_onDataSent);

  // ==== State: SCAN ====
  while (state == WIFI_SCAN) {
    wifiRemote_show_info("Scanning WiFi...", 500);
    int n = WiFi.scanNetworks(); // << Fix: use basic scan
    wifiPeerCount = 0;
    for (int i = 0; i < n && wifiPeerCount < WIFI_SCAN_LIMIT; ++i) {
      String ssid = WiFi.SSID(i);
      if (ssid.indexOf("ESP32") != -1 || ssid.indexOf("ESPNOW") != -1) { // Simple filter
        wifiPeers[wifiPeerCount].rssi = WiFi.RSSI(i);
        wifiPeers[wifiPeerCount].online = false;
        ssid.toCharArray(wifiPeers[wifiPeerCount].ssid, 33);
        memcpy(wifiPeers[wifiPeerCount].mac, WiFi.BSSID(i), 6);
        wifiPeerCount++;
      }
    }
    if (wifiPeerCount == 0) {
      wifiRemote_show_info("No ESP-NOW Devices!", 1200);
      wifiRemote_show_info("Hold OK: Exit", 600);
      // Wait for user to exit scan
      while (1) {
        readButtons();
        if (okPressed && !prevOk) {
          state = WIFI_EXIT;
          break;
        }
        prevOk = okPressed;
        delay(60);
      }
    } else {
      state = WIFI_SELECT;
      wifiPeerIndex = 0;
    }
  }

  // ==== State: SELECT ====
  while (state == WIFI_SELECT) {
    readButtons();
    display.clearDisplay();
    wifiRemote_drawStatusBar("Select Device (OK:Connect)");
    display.setTextSize(1);
    for (int i = 0; i < wifiPeerCount; ++i) {
      display.setCursor(10, 18 + i * 12);
      if (i == wifiPeerIndex) display.print("> ");
      else display.print("  ");
      display.print(wifiPeers[i].ssid);
      display.print(" [");
      display.print(wifiPeers[i].rssi);
      display.print("]");
    }
    display.setCursor(0, WIFI_REMOTE_SCREEN_HEIGHT-12);
    display.print("Back: hold OK");
    display.display();

    if (upPressed && !prevUp && wifiPeerIndex > 0) {
      wifiPeerIndex--; beep(1000, 30);
    }
    if (downPressed && !prevDown && wifiPeerIndex < wifiPeerCount-1) {
      wifiPeerIndex++; beep(1000, 30);
    }
    // Connect
    if (okPressed && !prevOk) {
      state = WIFI_CONNECTING;
    }
    // Back (hold OK)
    static unsigned long okHoldStart = 0;
    if (okPressed) {
      if (okHoldStart == 0) okHoldStart = millis();
      if (millis() - okHoldStart > 900) {
        beep(700, 80);
        state = WIFI_EXIT;
      }
    } else {
      okHoldStart = 0;
    }
    prevUp = upPressed; prevDown = downPressed; prevOk = okPressed;
    delay(20);
    if (state != WIFI_SELECT) break;
  }

  // ==== State: CONNECTING ====
  while (state == WIFI_CONNECTING) {
    wifiRemote_show_info("Connecting...", 500);
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, wifiPeers[wifiPeerIndex].mac, 6);
    peerInfo.channel = 0; peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
    // Try to communicate (send dummy packet)
    SwitchPayload dummy = {{0,0,0,0}};
    esp_err_t res = esp_now_send(wifiPeers[wifiPeerIndex].mac, (uint8_t*)&dummy, sizeof(dummy));
    if (res == ESP_OK) {
      wifiRemote_show_info("Connected!", 800);
      state = WIFI_CONNECTED;
    } else {
      wifiRemote_show_info("Conn. Failed!", 800);
      state = WIFI_SELECT;
    }
  }

  // ==== State: CONNECTED (Switches) ====
  while (state == WIFI_CONNECTED) {
    readButtons();
    display.clearDisplay();
    wifiRemote_drawStatusBar(wifiPeers[wifiPeerIndex].ssid);
    wifiRemote_drawSwitches();
    display.setTextSize(1);
    display.setCursor(0, WIFI_REMOTE_SCREEN_HEIGHT-12);
    display.print("Back: hold OK");
    display.display();

    // Toggle switch
    if (okPressed && !prevOk) {
      switchPayload.switches[selectedSwitch] = !switchPayload.switches[selectedSwitch];
      esp_now_send(wifiPeers[wifiPeerIndex].mac, (uint8_t*)&switchPayload, sizeof(SwitchPayload));
      beep(1800, 40);
    }
    // Navigation
    if (upPressed && !prevUp && selectedSwitch > 0) {
      selectedSwitch--; beep(1000, 30);
    }
    if (downPressed && !prevDown && selectedSwitch < 3) {
      selectedSwitch++; beep(1000, 30);
    }
    // Back (hold OK)
    static unsigned long okHoldStart = 0;
    if (okPressed) {
      if (okHoldStart == 0) okHoldStart = millis();
      if (millis() - okHoldStart > 900) {
        beep(700, 80);
        state = WIFI_EXIT;
      }
    } else {
      okHoldStart = 0;
    }
    prevUp = upPressed; prevDown = downPressed; prevOk = okPressed;
    delay(20);
    if (state != WIFI_CONNECTED) break;
  }

  // ==== Cleanup ====
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);
}

#endif