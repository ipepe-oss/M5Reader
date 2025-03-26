#include <epdiy.h>
#include <M5GFX.h>
#include <display_splash_screen.h>
#include <display_menu.h>

M5GFX display;
int selectedMenuItem = 0;

void setup() {
  display.begin();
  display.setEpdMode(epd_mode_t::epd_fastest);
  display.invertDisplay(true);

  displaySplashScreen(display);
  displayMenu(display);
}

void loop() {
  delay(100);
}
