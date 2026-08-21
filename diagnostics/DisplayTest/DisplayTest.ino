// GIGA Display Shield smoke test: title + live seconds counter.
// If this shows on the screen, the shield and library work.
#include "Arduino_GigaDisplay_GFX.h"

GigaDisplay_GFX display;

#define BLACK 0x0000
#define WHITE 0xFFFF
#define GREEN 0x07E0

void setup() {
  display.begin();
  display.setRotation(1);          // landscape, 800x480
  display.fillScreen(BLACK);
  display.setTextColor(GREEN);
  display.setTextSize(9);
  display.setCursor(150, 120);
  display.print("OTTO3");
  display.setTextSize(4);
  display.setTextColor(WHITE);
  display.setCursor(150, 260);
  display.print("display test");
}

int last = -1;
void loop() {
  int s = millis() / 1000;
  if (s != last) {
    last = s;
    display.fillRect(150, 330, 400, 60, BLACK);
    display.setCursor(150, 340);
    display.setTextSize(5);
    display.setTextColor(WHITE);
    display.print(s); display.print(" s");
  }
}
