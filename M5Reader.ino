/*
 * E-Book reader for M5Paper using M5GFX and epdiy.
 *
 * It reads a single book on the root of the Micro SD card, called "Book.txt".
 *
 * The left and right buttons navigate between pages, and the middle button turns the device off.
 *
 * The page you're on is saved to the SD card, rather than the EEPROM, to avoid wear.
 */

#include <epdiy.h>
#include <M5GFX.h>
#include <SD.h>
#include <M5Unified.h>

M5GFX display;
M5Canvas canvas(&display);

const char *DATA_FILE = "/pageNumber.txt";

int currentPage = 0;
int pageCount = 0;
int border = 10;
uint8_t *textFile;

struct aPage {
  uint32_t start;
  uint32_t end;
} pages[2000];

String textSubstring(uint8_t *textFile, int startPtr, int endPtr) {
  String s = "";
  for (int i = startPtr; i < endPtr; i++) {
    s += (char) textFile[i];
  }
  return s;
}

int textIndexOfSpaceCR(uint8_t *textFile, int startPtr, int textLength) {
  for (int ptr = startPtr; ptr < textLength; ptr++) {
    if ((textFile[ptr] == 32) || (textFile[ptr] == 13)) {
      return ptr;
    }
  }
  return -1;
}

bool reachedEndOfBook(int wordStart, int textLength) {
  return wordStart >= textLength;
}

void storePageSD(uint32_t c) {
  auto f = SD.open(DATA_FILE, "wb");
  uint8_t buf[4];
  buf[0] = c;
  buf[1] = c >> 8;
  buf[2] = c >> 16;
  buf[3] = c >> 24;
  auto bytes = f.write(&buf[0], 4);
  f.close();
}

uint32_t getPageSD() {
  uint32_t val;
  if (SD.exists(DATA_FILE)) {
    auto f = SD.open(DATA_FILE, "rb");
    val = f.read();
    f.close();
  }
  else {
    val = 0;
  }
  return val;
}

int getNextPage(uint8_t *textFile, int startPtr, int textLength) {
  int wordStart = 0;
  int wordEnd = 0;
  int xPos = 0;
  int yPos = 0;
  int xMax = canvas.width() - (border << 1);
  int yMax = canvas.height() - (border << 1);
  
  while (!reachedEndOfBook(wordStart + startPtr, textLength - 500)) {
    // Get the end of the current word.
    wordEnd = textIndexOfSpaceCR(textFile, startPtr + wordStart + 1, textLength) - startPtr;
    // Measure the text.
    String text = textSubstring(textFile, startPtr + wordStart, startPtr + wordEnd);
    int textPixelLength = canvas.textWidth(text);
    // If the line of text with the new word over-runs the side of the screen,
    if ((xPos + textPixelLength >= xMax) || (text.charAt(0) == 13)) {
      xPos = 0;
      yPos += 22;  // Line height
      wordStart++; // Miss out space as this is a new line.
      if ((yPos + 90) >= yMax) {
        if (pageCount > 0) {
          Serial.print(" New PAGE: ");
        }
        return startPtr + wordStart;
      }
    }
    xPos += textPixelLength;
    wordStart = wordEnd;
  }
  return textLength;
}

void findPageStartStop(uint8_t *textFile, int textLength) {
  int startPtr = 0;
  int endPtr = 0;
  while ((endPtr < textLength) && (textFile[endPtr] != 0)) {
    endPtr = getNextPage(textFile, startPtr, textLength);
    if (startPtr >= textLength) break;
    pages[pageCount].start = startPtr;
    pages[pageCount].end = endPtr;
    pageCount++;
    startPtr = endPtr;
  }
}

void displayPage(uint8_t *textFile, aPage page) {
  canvas.setCursor(border, border);
  String text = textSubstring(textFile, page.start, page.end);
  int wordStart = 0;
  int wordEnd = 0;
  while ((text.indexOf(' ', wordStart) >= 0) && (wordStart <= text.length())) {
    wordEnd = text.indexOf(' ', wordStart + 1);
    if (wordEnd < 0) wordEnd = text.length();
    uint16_t len = canvas.textWidth(text.substring(wordStart, wordEnd));
    if (canvas.getCursorX() + len >= canvas.width() - (border << 1)) {
      canvas.println();
      canvas.setCursor(border, canvas.getCursorY());
      wordStart++;
    }
    canvas.print(text.substring(wordStart, wordEnd));
    wordStart = wordEnd;
  }

  // Footer with page information
  char footer[64];
  sprintf(footer, "Page %d of %d", currentPage + 1, pageCount);
  canvas.setTextDatum(BR_DATUM);
  canvas.drawString(footer, canvas.width() - border, canvas.height() - border);
  canvas.setTextDatum(TL_DATUM);
  
  canvas.pushSprite(0, 0);
}

void setup() {
  M5.begin();
  Serial.begin(115200);
  
  // Initialize display
  display.begin();
  
  if (display.isEPD()) {
    display.setEpdMode(epd_mode_t::epd_quality);
    display.invertDisplay(true);
    display.clear(TFT_BLACK);
  }
  
  if (display.width() < display.height()) {
    display.setRotation(display.getRotation() ^ 1);
  }

  // Initialize canvas
  canvas.setColorDepth(1); // mono color
  canvas.createSprite(display.width(), display.height());
  canvas.setTextSize(1);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextWrap(true, true);
  
  // Initialize SD card
  if (!SD.begin()) {
    Serial.println("SD card initialization failed!");
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString("Error: SD card initialization failed", canvas.width()/2, canvas.height()/2);
    canvas.pushSprite(0, 0);
    return;
  }
  
  // Load text file
  textFile = (uint8_t*)ps_malloc(1048576); // 1MB
  auto f = SD.open("/book.txt", "rb");
  
  if (!f) {
    Serial.println("Failed to open book.txt");
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString("Error: book.txt not found", canvas.width()/2, canvas.height()/2);
    canvas.pushSprite(0, 0);
    return;
  }
  
  int fileLength = f.available();
  Serial.print("File length: ");
  Serial.println(fileLength);
  f.read(textFile, f.available());
  f.close();
  Serial.println("Loaded.");

  Serial.println("Splitting into pages.");
  findPageStartStop(textFile, fileLength);

  currentPage = getPageSD();
  if (currentPage >= pageCount) {
    currentPage = 0;
  }
  
  canvas.fillSprite(TFT_WHITE);
  displayPage(textFile, pages[currentPage]);
}

void loop() {
  delay(100);
  M5.update();
  
  // Left button - previous page
  if (M5.BtnL.wasPressed()) {
    if (--currentPage < 0) currentPage = 0;
    canvas.fillSprite(TFT_WHITE);
    displayPage(textFile, pages[currentPage]);
  }
  
  // Middle button - save page and shutdown
  if (M5.BtnP.wasPressed()) {
    storePageSD(currentPage);
    canvas.setTextDatum(MC_DATUM);
    canvas.drawString("Turning off...", canvas.width()/2, canvas.height()/2);
    canvas.pushSprite(0, 0);
    delay(1000);
    M5.Power.powerOff();
  }
  
  // Right button - next page
  if (M5.BtnR.wasPressed()) {
    if (++currentPage >= pageCount) currentPage = pageCount - 1;
    canvas.fillSprite(TFT_WHITE);
    displayPage(textFile, pages[currentPage]);
  }
}