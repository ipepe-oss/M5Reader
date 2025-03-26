#include <epdiy.h>
#include <M5GFX.h>

M5GFX display;


void displaySplashScreen(void)
{
    display.clear(TFT_WHITE);

    display.setTextColor(TFT_BLACK, TFT_WHITE);
    display.setCursor(0, 0);

    // Display splash screen
    int centerX = display.width() / 2;
    int centerY = display.height() / 2;

    display.setTextSize(4);
    display.setCursor(centerX - 80, centerY - 30);
    display.println("M5Reader");
}

void setup(void)
{
    display.begin();
    display.setEpdMode(epd_mode_t::epd_text);
    display.invertDisplay(true);
    display.clear(TFT_WHITE);
    displaySplashScreen();
}

void loop(void)
{
    delay(1000);
}
