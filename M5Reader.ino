#include <epdiy.h>
#include <M5GFX.h>

M5GFX display;
int selectedMenuItem = 0;

void setup() {
  display.begin();
  display.setEpdMode(epd_mode_t::epd_fastest);
  display.invertDisplay(true);
  display.clear(TFT_WHITE);
  
  display.setTextColor(TFT_BLACK, TFT_WHITE);
  display.setTextSize(4);
  display.setCursor(display.width()/2 - 80, display.height()/2 - 30);
  display.println("M5Reader");
  
  delay(2000);
  
  display.clear(TFT_WHITE);
  display.setTextSize(2);
  
  int tileWidth = display.width() / 2;
  int tileHeight = display.height() / 4;
  
  display.fillRect(0, 0, tileWidth, tileHeight, TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(tileWidth/2 - 30, tileHeight/2 - 8);
  display.print("Books");
  
  display.drawRect(tileWidth, 0, tileWidth, tileHeight, TFT_BLACK);
  display.setTextColor(TFT_BLACK, TFT_WHITE);
  display.setCursor(tileWidth + tileWidth/2 - 40, tileHeight/2 - 8);
  display.print("Settings");
  
  display.drawRect(0, tileHeight, tileWidth, tileHeight, TFT_BLACK);
  display.setCursor(tileWidth/2 - 30, tileHeight + tileHeight/2 - 8);
  display.print("Recent");
  
  display.drawRect(tileWidth, tileHeight, tileWidth, tileHeight, TFT_BLACK);
  display.setCursor(tileWidth + tileWidth/2 - 30, tileHeight + tileHeight/2 - 8);
  display.print("Search");
  
  display.drawRect(0, 2*tileHeight, tileWidth, tileHeight, TFT_BLACK);
  display.setCursor(tileWidth/2 - 50, 2*tileHeight + tileHeight/2 - 8);
  display.print("Bookmarks");
  
  display.drawRect(tileWidth, 2*tileHeight, tileWidth, tileHeight, TFT_BLACK);
  display.setCursor(tileWidth + tileWidth/2 - 45, 2*tileHeight + tileHeight/2 - 8);
  display.print("Calibrate");
  
  display.drawRect(0, 3*tileHeight, tileWidth, tileHeight, TFT_BLACK);
  display.setCursor(tileWidth/2 - 30, 3*tileHeight + tileHeight/2 - 8);
  display.print("About");
  
  display.drawRect(tileWidth, 3*tileHeight, tileWidth, tileHeight, TFT_BLACK);
  display.setCursor(tileWidth + tileWidth/2 - 20, 3*tileHeight + tileHeight/2 - 8);
  display.print("Help");
  
  display.display();
}

void loop() {
  delay(100);
}
