#include <Wire.h>
#include <Adafruit_SSD1306.h>

// ==== Display and button global variables ====
#define SDA_PIN 21
#define SCL_PIN 22
#define UP_PIN 35
#define DOWN_PIN 32
#define OK_PIN 26
#define BUZZER_PIN 27
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

bool upPressed = false, downPressed = false, okPressed = false;
bool prevUp = false, prevDown = false, prevOk = false;
unsigned long lastDebounceTime[3] = {0, 0, 0};
const unsigned long debounceDelay = 50;

// ==== Menu system ====
#define MAX_MENU_ITEMS 10
#define VISIBLE_MENU_ITEMS 4

enum MenuType { MENU_ITEM, MENU_SUBMENU, MENU_BACK };

struct Menu; // forward

struct MenuItem {
  const char* name;
  MenuType type;
  void (*action)(const char*); // For MENU_ITEM
  struct Menu* subMenu; // For MENU_SUBMENU
};

struct Menu {
  const char* title;
  MenuItem items[MAX_MENU_ITEMS];
  uint8_t itemCount;
  int selectedIndex;
  int scrollTop; // for vertical scrolling
  struct Menu* parent;
};

// ==== Forward declarations for actions ====
void action_ble_remote(const char* msg);
void action_wifi_remote(const char* msg);
void action_ir_remote(const char* msg);
void action_air_remote(const char* msg);
void action_wifi_deauther(const char* msg);
void action_ir_deauther(const char* msg);
void action_rf_deauther(const char* msg);
void action_offline_weather(const char* msg);
void action_online_weather(const char* msg);
void action_games(const char* msg);
void action_test_sensors(const char* msg);
void action_sleep_timer(const char* msg);

// ==== FEATURE HEADERS ====
#include "ble_remote.h"

// ==== Menu definitions ====
Menu remotesMenu, deautherMenu, weatherMenu, funMenu, settingsMenu, mainMenu;

MenuItem remotesMenuItems[] = {
  {"Back", MENU_BACK, nullptr, nullptr},
  {"BLE Remote", MENU_ITEM, action_ble_remote, nullptr},
  {"WiFi Remote", MENU_ITEM, action_wifi_remote, nullptr},
  {"IR Remote", MENU_ITEM, action_ir_remote, nullptr},
  {"AIR Remote", MENU_ITEM, action_air_remote, nullptr}
};
MenuItem deautherMenuItems[] = {
  {"Back", MENU_BACK, nullptr, nullptr},
  {"WiFi Deauther", MENU_ITEM, action_wifi_deauther, nullptr},
  {"IR Deauther", MENU_ITEM, action_ir_deauther, nullptr},
  {"RF Deauther", MENU_ITEM, action_rf_deauther, nullptr}
};
MenuItem weatherMenuItems[] = {
  {"Back", MENU_BACK, nullptr, nullptr},
  {"OFFLINE Weather", MENU_ITEM, action_offline_weather, nullptr},
  {"ONLINE Weather", MENU_ITEM, action_online_weather, nullptr}
};
MenuItem funMenuItems[] = {
  {"Back", MENU_BACK, nullptr, nullptr},
  {"Games", MENU_ITEM, action_games, nullptr}
};
MenuItem settingsMenuItems[] = {
  {"Back", MENU_BACK, nullptr, nullptr},
  {"Test Sensors", MENU_ITEM, action_test_sensors, nullptr},
  {"Sleep Timer", MENU_ITEM, action_sleep_timer, nullptr}
};
MenuItem mainMenuItems[] = {
  {"REMOTES", MENU_SUBMENU, nullptr, &remotesMenu},
  {"DEAUTHER", MENU_SUBMENU, nullptr, &deautherMenu},
  {"Weather", MENU_SUBMENU, nullptr, &weatherMenu},
  {"FUN", MENU_SUBMENU, nullptr, &funMenu},
  {"SETTINGS", MENU_SUBMENU, nullptr, &settingsMenu}
};

void setupMenus() {
  remotesMenu = {"REMOTES", {}, 5, 0, 0, &mainMenu};
  memcpy(remotesMenu.items, remotesMenuItems, sizeof(remotesMenuItems));

  deautherMenu = {"DEAUTHER", {}, 4, 0, 0, &mainMenu};
  memcpy(deautherMenu.items, deautherMenuItems, sizeof(deautherMenuItems));

  weatherMenu = {"Weather", {}, 3, 0, 0, &mainMenu};
  memcpy(weatherMenu.items, weatherMenuItems, sizeof(weatherMenuItems));

  funMenu = {"FUN", {}, 2, 0, 0, &mainMenu};
  memcpy(funMenu.items, funMenuItems, sizeof(funMenuItems));

  settingsMenu = {"SETTINGS", {}, 3, 0, 0, &mainMenu};
  memcpy(settingsMenu.items, settingsMenuItems, sizeof(settingsMenuItems));

  mainMenu = {"MAIN MENU", {}, 5, 0, 0, nullptr};
  memcpy(mainMenu.items, mainMenuItems, sizeof(mainMenuItems));

  mainMenu.selectedIndex = 0; mainMenu.scrollTop = 0;
  remotesMenu.selectedIndex = 0; remotesMenu.scrollTop = 0;
  deautherMenu.selectedIndex = 0; deautherMenu.scrollTop = 0;
  weatherMenu.selectedIndex = 0; weatherMenu.scrollTop = 0;
  funMenu.selectedIndex = 0; funMenu.scrollTop = 0;
  settingsMenu.selectedIndex = 0; settingsMenu.scrollTop = 0;
}

// ==== Button handling ====
void beep(uint16_t freq = 1800, uint16_t dur = 50) {
  tone(BUZZER_PIN, freq, dur);
  delay(dur);
  noTone(BUZZER_PIN);
}

void readButtons() {
  bool readingUp = digitalRead(UP_PIN) == HIGH;
  bool readingDown = digitalRead(DOWN_PIN) == HIGH;
  bool readingOk = digitalRead(OK_PIN) == HIGH;
  unsigned long now = millis();

  if (readingUp != prevUp) lastDebounceTime[0] = now;
  if ((now - lastDebounceTime[0]) > debounceDelay) upPressed = readingUp;
  prevUp = readingUp;

  if (readingDown != prevDown) lastDebounceTime[1] = now;
  if ((now - lastDebounceTime[1]) > debounceDelay) downPressed = readingDown;
  prevDown = readingDown;

  if (readingOk != prevOk) lastDebounceTime[2] = now;
  if ((now - lastDebounceTime[2]) > debounceDelay) okPressed = readingOk;
  prevOk = readingOk;
}

// ==== Menu rendering ====
unsigned long lastScrollTime = 0;
int scrollOffset = 0;

void drawMenu(Menu* menu) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Title bar
  display.fillRect(0, 0, SCREEN_WIDTH, 14, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(2, 2);
  display.print(menu->title);
  display.setTextColor(SSD1306_WHITE);

  int total = menu->itemCount;
  int sel = menu->selectedIndex;
  int winStart = menu->scrollTop;
  int winEnd = winStart + VISIBLE_MENU_ITEMS;

  if (sel < winStart) menu->scrollTop = winStart = sel;
  if (sel >= winEnd) menu->scrollTop = winStart = sel - VISIBLE_MENU_ITEMS + 1;
  winEnd = winStart + VISIBLE_MENU_ITEMS;
  if (winEnd > total) winEnd = total;

  int y = 18;
  for (int i = winStart; i < winEnd; i++) {
    bool selected = (i == menu->selectedIndex);
    if (selected) {
      display.fillRect(0, y-2, SCREEN_WIDTH, 13, SSD1306_INVERSE);
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      display.setCursor(5, y);
      display.print("> ");
      display.print(menu->items[i].name);
      display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    } else {
      display.setCursor(5, y);
      display.print("  ");
      display.print(menu->items[i].name);
    }
    y += 13;
  }
  // Scroll bar (optional)
  if (total > VISIBLE_MENU_ITEMS) {
    int barHeight = (VISIBLE_MENU_ITEMS * (SCREEN_HEIGHT-18)) / total;
    int barPos = ((menu->scrollTop) * (SCREEN_HEIGHT-18)) / total;
    display.fillRect(SCREEN_WIDTH-3, 18+barPos, 2, barHeight, SSD1306_WHITE);
  }
  display.display();
}

// ==== Menu navigation ====
void handleMenu(Menu* &currentMenu) {
  static bool lastUp=false, lastDown=false, lastOk=false;

  if (upPressed && !lastUp) {
    if(currentMenu->selectedIndex > 0) currentMenu->selectedIndex--;
    beep(950, 25);
    scrollOffset = 0;
    lastUp = true;
  }
  if (!upPressed) lastUp = false;

  if (downPressed && !lastDown) {
    if(currentMenu->selectedIndex < (currentMenu->itemCount-1)) currentMenu->selectedIndex++;
    beep(950, 25);
    scrollOffset = 0;
    lastDown = true;
  }
  if (!downPressed) lastDown = false;

  if (okPressed && !lastOk) {
    MenuItem& item = currentMenu->items[currentMenu->selectedIndex];
    if(item.type == MENU_BACK) {
      if(currentMenu->parent) currentMenu = currentMenu->parent;
      beep(1600, 35);
      scrollOffset = 0;
    } else if(item.type == MENU_SUBMENU && item.subMenu) {
      item.subMenu->selectedIndex = 0;
      item.subMenu->scrollTop = 0;
      currentMenu = item.subMenu;
      beep(1800, 60);
      scrollOffset = 0;
    } else if(item.type == MENU_ITEM && item.action) {
      item.action(item.name);
      scrollOffset = 0;
    }
    lastOk = true;
  }
  if(!okPressed) lastOk = false;
}

// ==== Action stubs for all except BLE/WiFi Remote ====
#define ACTION_FN(name) void action_##name(const char* msg) { show_info(msg); }
void show_info(const char* msg) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(msg, 0,0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH-w)/2;
  int y = (SCREEN_HEIGHT-h)/2;
  display.setCursor(x, y);
  display.print(msg);
  display.display();
  tone(BUZZER_PIN, 1500, 40);
  delay(700);
  noTone(BUZZER_PIN);
}
ACTION_FN(wifi_remote)
ACTION_FN(ir_remote)
ACTION_FN(air_remote)
ACTION_FN(wifi_deauther)
ACTION_FN(ir_deauther)
ACTION_FN(rf_deauther)
ACTION_FN(offline_weather)
ACTION_FN(online_weather)
ACTION_FN(games)
ACTION_FN(test_sensors)
ACTION_FN(sleep_timer)

// ==== BLE Remote Action ====
void action_ble_remote(const char* msg) {
  runBLERemote();
  // Button state reset after BLE remote mode
  delay(150);
  readButtons();
  upPressed = downPressed = okPressed = false;
  prevUp = prevDown = prevOk = false;
  delay(200);
}

// ==== Global menu pointer ====
Menu* currentMenu = &mainMenu;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(400);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (1) {
      Serial.println("SSD1306 allocation failed");
      delay(1000);
    }
  }
  display.setRotation(2);

  pinMode(UP_PIN, INPUT);
  pinMode(DOWN_PIN, INPUT);
  pinMode(OK_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds("AETHER 32", 0,0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH-w)/2;
  int y = (SCREEN_HEIGHT-h)/2;
  display.setCursor(x, y);
  display.print("AETHER 32");
  display.display();
  beep(2000, 80);
  delay(1200);

  setupMenus();

  currentMenu = &mainMenu;
  mainMenu.selectedIndex = 0;
  mainMenu.scrollTop = 0;
}

void loop() {
  readButtons();
  drawMenu(currentMenu);
  handleMenu(currentMenu);
  delay(10);
}