void displaySplashScreen(M5GFX &display) {
  display.clear(TFT_WHITE);

  display.setTextColor(TFT_BLACK, TFT_WHITE);
  display.setTextSize(4);
  display.setCursor(display.width()/2 - 80, display.height()/2 - 30);
  display.println("M5Reader");

  delay(2000);
}
