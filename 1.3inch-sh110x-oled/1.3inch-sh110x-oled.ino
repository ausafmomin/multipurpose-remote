#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1   // No reset pin
#define I2C_ADDRESS   0x3C // Most SH1106 modules use 0x3C. Try 0x3D if your display is blank.

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  delay(250); // Give the OLED time to power up

  if(!display.begin(I2C_ADDRESS, true)) { // true: reset display
    Serial.println("Display allocation failed!");
    while(1);
  }
  display.setRotation(2);
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("SH1106 OLED");
  display.setTextSize(1);
  display.setCursor(0, 30);
  display.println("Adafruit_SH110X Test");
  display.setCursor(0, 50);
  display.print("I2C Addr: 0x");
  display.println(I2C_ADDRESS, HEX);
  display.display();

  delay(3000);

  // Draw simple graphics
  display.clearDisplay();
  display.drawRect(0, 0, 127, 63, SH110X_WHITE); // Frame
  display.fillCircle(64, 32, 20, SH110X_WHITE);  // Filled circle
  display.display();
  delay(2000);

  // Invert display briefly
  display.invertDisplay(true);
  delay(1000);
  display.invertDisplay(false);

  // End of test message
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("Test complete!");
  display.display();
}

void loop() {
  // Nothing here
}