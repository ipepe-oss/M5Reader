#include <epdiy.h>
#include <M5GFX.h>

M5GFX display;

// Menu constants
static constexpr int MENU_COLS = 2;
static constexpr int MENU_ROWS = 4;
static constexpr int MENU_ITEMS = MENU_COLS * MENU_ROWS;

struct MenuItem {
  const char* label;
  void (*action)();
};

// Forward declarations
void menuAction1();
void menuAction2();
void menuAction3();
void menuAction4();
void menuAction5();
void menuAction6();
void menuAction7();
void menuAction8();

MenuItem menuItems[MENU_ITEMS] = {
  {"Books", menuAction1},
  {"Settings", menuAction2},
  {"Recent", menuAction3},
  {"Search", menuAction4},
  {"Bookmarks", menuAction5},
  {"Calibrate", menuAction6},
  {"About", menuAction7},
  {"Help", menuAction8},
};

int selectedMenuItem = 0;

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

void displayMenu() {
  display.clear(TFT_WHITE);
  
  int displayWidth = display.width();
  int displayHeight = display.height();
  
  int tileWidth = displayWidth / MENU_COLS;
  int tileHeight = displayHeight / MENU_ROWS;
  
  display.setTextSize(2);
  
  for (int i = 0; i < MENU_ITEMS; i++) {
    int col = i % MENU_COLS;
    int row = i / MENU_COLS;
    
    int x = col * tileWidth;
    int y = row * tileHeight;
    
    // Draw tile border
    if (i == selectedMenuItem) {
      display.fillRect(x, y, tileWidth, tileHeight, TFT_BLACK);
      display.setTextColor(TFT_WHITE, TFT_BLACK);
    } else {
      display.drawRect(x, y, tileWidth, tileHeight, TFT_BLACK);
      display.setTextColor(TFT_BLACK, TFT_WHITE);
    }
    
    // Center text in tile
    int textWidth = strlen(menuItems[i].label) * 12; // Approximate width
    int textX = x + (tileWidth - textWidth) / 2;
    int textY = y + (tileHeight - 16) / 2; // 16 is approximate text height
    
    display.setCursor(textX, textY);
    display.print(menuItems[i].label);
  }
  
  display.display();
}

// Placeholder menu actions
void menuAction1() { /* Books functionality */ }
void menuAction2() { /* Settings functionality */ }
void menuAction3() { /* Recent functionality */ }
void menuAction4() { /* Search functionality */ }
void menuAction5() { /* Bookmarks functionality */ }
void menuAction6() { /* Calibrate functionality */ }
void menuAction7() { /* About functionality */ }
void menuAction8() { /* Help functionality */ }

void setup(void)
{
    display.begin();
    display.setEpdMode(epd_mode_t::epd_text);
    display.invertDisplay(true);
    display.clear(TFT_WHITE);
    displaySplashScreen();
    delay(2000); // Show splash screen for 2 seconds
    displayMenu();
}

void loop(void)
{
    // Handle touch input (to be implemented)
    delay(100);
}
