#include <epdiy.h>
#include <M5GFX.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>

// Debug flag - set to true for detailed debugging
#define DEBUG_MODE true

// Define SD card pin
#define PIN_SD_CS GPIO_NUM_4

// Display init
M5GFX display;

// EPUB reader variables
int currentPage = 0;
int totalPages = 0;
bool needsRefresh = true;

// Touch variables
int touchX = 0;
int touchY = 0;
uint8_t touchCount = 0;

// Power management
unsigned long lastActivityTime = 0;
const unsigned long SLEEP_TIMEOUT = 300000; // 5 minutes in ms

// Buffer sizes
#define MAX_CHAPTER_SIZE 32768
#define MAX_CHAPTER_COUNT 100
#define MAX_PATH_LENGTH 128
#define MAX_TITLE_LENGTH 256
#define MAX_CONTENT_LENGTH 1024

// Forward declaration
class EpubReader;
EpubReader* epubReader = NULL;

// ============================================================================
// EpubReader class for parsing EPUB files
// ============================================================================
class EpubReader {
private:
  // File system reference
  fs::FS* fileSystem;
  File epubFile;
  
  // Book metadata
  char title[MAX_TITLE_LENGTH];
  char author[MAX_TITLE_LENGTH];
  
  // Content
  uint16_t chapterCount;
  char chapterPaths[MAX_CHAPTER_COUNT][MAX_PATH_LENGTH];
  char currentChapterTitle[MAX_TITLE_LENGTH];
  char* chapterContent;
  uint16_t currentChapterIndex;
  
  // Display parameters
  uint16_t displayWidth;
  uint16_t displayHeight;
  uint16_t lineHeight;
  uint16_t charsPerLine;
  uint16_t linesPerPage;
  
  // Pagination
  uint32_t currentCharPos;
  uint32_t chapterSize;
  uint16_t pagesInCurrentChapter;
  uint16_t currentPageInChapter;
  
  // Helper variables
  bool initialized;
  
public:
  EpubReader() {
    initialized = false;
    chapterContent = NULL;
  }
  
  ~EpubReader() {
    if (chapterContent != NULL) {
      free(chapterContent);
      chapterContent = NULL;
    }
  }
  
  bool open(fs::FS &fs, const char* path) {
    fileSystem = &fs;
    
    // Allocate memory for chapter content
    if (chapterContent == NULL) {
      chapterContent = (char*)heap_caps_malloc(MAX_CHAPTER_SIZE, MALLOC_CAP_SPIRAM);
      if (chapterContent == NULL) {
        Serial.println("Failed to allocate memory for chapter content");
        return false;
      }
    }
    
    // Reset variables
    chapterCount = 0;
    currentChapterIndex = 0;
    currentCharPos = 0;
    currentPageInChapter = 0;
    
    // Try to open the EPUB file
    epubFile = fs.open(path);
    if (!epubFile) {
      Serial.println("Failed to open EPUB file");
      return false;
    }
    
    // Parse the EPUB file structure (simplified)
    if (!parseEpubStructure()) {
      Serial.println("Failed to parse EPUB structure");
      epubFile.close();
      return false;
    }
    
    // Load the first chapter
    if (!loadChapter(0)) {
      Serial.println("Failed to load first chapter");
      epubFile.close();
      return false;
    }
    
    initialized = true;
    return true;
  }
  
  bool isInitialized() {
    return initialized;
  }
  
  bool createDummyBook() {
    // Allocate memory for chapter content if needed
    if (chapterContent == NULL) {
      chapterContent = (char*)heap_caps_malloc(MAX_CHAPTER_SIZE, MALLOC_CAP_SPIRAM);
      if (chapterContent == NULL) {
        Serial.println("Failed to allocate memory for dummy content");
        return false;
      }
    }
    
    // Reset variables
    chapterCount = 0;
    currentChapterIndex = 0;
    currentCharPos = 0;
    currentPageInChapter = 0;
    
    // Create book metadata
    strcpy(title, "Sample EPUB Book");
    strcpy(author, "Example Author");
    
    // Add dummy chapters
    chapterCount = 5;
    strcpy(chapterPaths[0], "chapter1.html");
    strcpy(chapterPaths[1], "chapter2.html");
    strcpy(chapterPaths[2], "chapter3.html");
    strcpy(chapterPaths[3], "chapter4.html");
    strcpy(chapterPaths[4], "chapter5.html");
    
    // Load the first chapter
    if (!loadChapter(0)) {
      Serial.println("Failed to load dummy chapter");
      return false;
    }
    
    initialized = true;
    return true;
  }
  
  void setDisplayParams(uint16_t width, uint16_t height, float textSize) {
    displayWidth = width;
    displayHeight = height;
    
    // Calculate text metrics
    lineHeight = (uint16_t)(textSize * 16);
    charsPerLine = displayWidth / (uint16_t)(textSize * 8);
    linesPerPage = displayHeight / lineHeight;
    
    // Update pagination if a chapter is loaded
    if (chapterContent != NULL && chapterSize > 0) {
      calculatePagination();
    }
  }
  
  int getTotalPages() {
    int pages = 0;
    
    // In a real implementation, we would calculate pages for all chapters
    // Here we just return a simple estimation
    if (chapterCount > 0 && pagesInCurrentChapter > 0) {
      pages = chapterCount * pagesInCurrentChapter;
    } else {
      pages = 10; // Default for demo
    }
    
    return pages;
  }
  
  const char* getTitle() {
    return title;
  }
  
  const char* getAuthor() {
    return author;
  }
  
  const char* getCurrentChapterTitle() {
    return currentChapterTitle;
  }
  
  bool nextPage() {
    if (!initialized) return false;
    
    currentPageInChapter++;
    
    if (currentPageInChapter >= pagesInCurrentChapter) {
      // Try to load next chapter
      if (currentChapterIndex < chapterCount - 1) {
        currentChapterIndex++;
        if (loadChapter(currentChapterIndex)) {
          currentPageInChapter = 0;
          return true;
        }
        return false;
      }
      // Already at the last page
      currentPageInChapter = pagesInCurrentChapter - 1;
      return false;
    }
    
    // Update character position for new page
    currentCharPos = getPageStartCharPos(currentPageInChapter);
    return true;
  }
  
  bool prevPage() {
    if (!initialized) return false;
    
    if (currentPageInChapter > 0) {
      currentPageInChapter--;
      // Update character position for new page
      currentCharPos = getPageStartCharPos(currentPageInChapter);
      return true;
    } else {
      // Try to load previous chapter
      if (currentChapterIndex > 0) {
        currentChapterIndex--;
        if (loadChapter(currentChapterIndex)) {
          currentPageInChapter = pagesInCurrentChapter - 1;
          // Set to last page in previous chapter
          currentCharPos = getPageStartCharPos(currentPageInChapter);
          return true;
        }
        return false;
      }
    }
    return false;
  }
  
  void renderPage(M5GFX& disp, int pageX, int pageY) {
    if (!initialized || chapterContent == NULL) return;
    
    // Set the correct position to render from
    uint32_t startPos = getPageStartCharPos(currentPageInChapter);
    uint32_t endPos = (currentPageInChapter == pagesInCurrentChapter - 1) ? 
                       chapterSize : getPageStartCharPos(currentPageInChapter + 1);
    
    if (startPos >= chapterSize) return;
    
    // Temporary buffer for rendering
    char lineBuf[256];
    int lineLen = 0;
    int currentX = pageX;
    int currentY = pageY;
    
    // Display chapter title on the first page
    if (currentPageInChapter == 0) {
      disp.setCursor(currentX, currentY);
      disp.setTextColor(TFT_BLACK, TFT_WHITE);
      disp.println(currentChapterTitle);
      currentY += lineHeight * 2;
    }
    
    // Render text content
    for (uint32_t i = startPos; i < endPos && i < chapterSize; i++) {
      char c = chapterContent[i];
      
      // Handle line breaks
      if (c == '\n' || c == '\r' || lineLen >= charsPerLine - 1) {
        if (c != '\n' && c != '\r') {
          lineBuf[lineLen++] = c;
        }
        
        // Null terminate the line
        lineBuf[lineLen] = '\0';
        
        // Render the line
        disp.setCursor(currentX, currentY);
        disp.print(lineBuf);
        
        // Move to next line
        currentY += lineHeight;
        lineLen = 0;
        
        // Check if we need to start a new page
        if (currentY >= pageY + displayHeight - lineHeight) {
          break;
        }
        
        // Skip \r\n sequence
        if (c == '\r' && i + 1 < endPos && chapterContent[i + 1] == '\n') {
          i++;
        }
      } else {
        // Add character to current line
        lineBuf[lineLen++] = c;
      }
    }
    
    // Render any remaining content in the buffer
    if (lineLen > 0) {
      lineBuf[lineLen] = '\0';
      disp.setCursor(currentX, currentY);
      disp.print(lineBuf);
    }
    
    // Draw page indicator
    char pageInfo[64];
    sprintf(pageInfo, "Page %d/%d - Ch %d/%d", 
            currentPageInChapter + 1, pagesInCurrentChapter,
            currentChapterIndex + 1, chapterCount);
    
    disp.setCursor(displayWidth/2 - 100, displayHeight - 20);
    disp.setTextColor(TFT_BLACK, TFT_WHITE);
    disp.print(pageInfo);
  }
  
private:
  bool parseEpubStructure() {
    // In a real implementation, this would extract the table of contents
    // and chapter files from the EPUB ZIP structure
    
    // For this simplified implementation, we'll create some dummy chapter data
    strcpy(title, "Sample EPUB Book");
    strcpy(author, "Example Author");
    
    chapterCount = 3;
    strcpy(chapterPaths[0], "chapter1.html");
    strcpy(chapterPaths[1], "chapter2.html");
    strcpy(chapterPaths[2], "chapter3.html");
    
    return true;
  }
  
  bool loadChapter(uint16_t chapterIdx) {
    if (chapterIdx >= chapterCount) return false;
    
    // In a real implementation, we would extract the chapter content
    // from the EPUB file (which is a ZIP archive)
    
    // For this simplified implementation, we'll create some dummy content
    currentChapterIndex = chapterIdx;
    sprintf(currentChapterTitle, "Chapter %d", chapterIdx + 1);
    
    // Generate some dummy text for the chapter
    char dummyText[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. "
                       "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
                       "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris "
                       "nisi ut aliquip ex ea commodo consequat.\n\n"
                       "Duis aute irure dolor in reprehenderit in voluptate velit esse "
                       "cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat "
                       "cupidatat non proident, sunt in culpa qui officia deserunt mollit "
                       "anim id est laborum.\n\n";
    
    // Clear the content buffer
    memset(chapterContent, 0, MAX_CHAPTER_SIZE);
    
    // Copy the chapter title
    int pos = 0;
    pos += sprintf(chapterContent + pos, "%s\n\n", currentChapterTitle);
    
    // Create enough content to fill multiple pages
    for (int i = 0; i < 20 && pos < MAX_CHAPTER_SIZE - 500; i++) {
      pos += sprintf(chapterContent + pos, "Section %d.%d\n\n", chapterIdx + 1, i + 1);
      
      // Add paragraph text
      int charsLeft = MAX_CHAPTER_SIZE - pos - 1;
      if (charsLeft >= (int)strlen(dummyText)) {
        pos += sprintf(chapterContent + pos, "%s", dummyText);
      } else {
        // No room for more content
        break;
      }
    }
    
    // Update chapter size
    chapterSize = pos;
    
    // Reset position and calculate pagination
    currentCharPos = 0;
    currentPageInChapter = 0;
    calculatePagination();
    
    return true;
  }
  
  void calculatePagination() {
    // Calculate how many pages are needed for the current chapter
    
    // Simple approach: count characters and divide by chars per page
    uint32_t charsPerPage = charsPerLine * linesPerPage;
    pagesInCurrentChapter = (chapterSize + charsPerPage - 1) / charsPerPage;
    
    // Ensure we have at least one page
    if (pagesInCurrentChapter == 0) {
      pagesInCurrentChapter = 1;
    }
  }
  
  uint32_t getPageStartCharPos(uint16_t pageIdx) {
    if (pageIdx >= pagesInCurrentChapter) {
      return chapterSize;
    }
    
    if (pageIdx == 0) {
      return 0;
    }
    
    // In a real implementation, we would track line breaks properly
    // For this simplified version, we estimate based on characters per page
    uint32_t charsPerPage = charsPerLine * linesPerPage;
    return pageIdx * charsPerPage;
  }
};

// ============================================================================
// Helper Functions
// ============================================================================

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  File root = fs.open(dirname);
  if (!root) {
    display.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    display.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  int fileCount = 0;
  
  while (file && fileCount < 5) { // Show at most 5 files
    if (file.isDirectory()) {
      if (levels) {
        display.print("  DIR : ");
        display.println(file.name());
      }
    } else {
      display.print("  FILE: ");
      display.print(file.name());
      display.print(" (");
      display.print(file.size());
      display.println(")");
      fileCount++;
    }
    file = root.openNextFile();
  }
  
  if (file) {
    display.println("... and more files");
  }
}

bool findAndOpenAnyEpub(File dir) {
  if (!dir || !dir.isDirectory()) {
    return false;
  }
  
  File file = dir.openNextFile();
  bool found = false;
  char filePath[MAX_PATH_LENGTH];
  
  while (file && !found) {
    if (file.isDirectory()) {
      // Skip directories starting with . (hidden folders)
      if (file.name()[0] != '.') {
        found = findAndOpenAnyEpub(file);
      }
    } else {
      String fileName = String(file.name());
      fileName.toLowerCase();
      
      if (fileName.endsWith(".epub")) {
        display.println("Found EPUB: " + fileName);
        display.display();
        
        // Get full path
        sprintf(filePath, "%s", file.path());
        
        if (epubReader->open(SD, filePath)) {
          display.println("Book loaded successfully!");
          found = true;
        } else {
          display.println("Error opening " + fileName);
        }
      }
    }
    
    if (!found) {
      file = dir.openNextFile();
    }
  }
  
  return found;
}

// ============================================================================
// Main Arduino Functions
// ============================================================================

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  Serial.println("M5Reader starting up...");
  
  // Disable watchdog during initialization
  esp_task_wdt_init(30, false);
  
  // Debug helper function
  auto log = [](const String& msg) {
    Serial.println(msg);
    if (display.isEPD()) {
      display.println(msg);
      display.display();
    }
  };
  
  try {
    // ------------- DISPLAY INITIALIZATION -------------
    log("Initializing display...");
    
    // Show splash screen as soon as possible
    display.init();
    if (display.isEPD()) {
      // Use text mode for better e-reader experience
      display.setEpdMode(epd_mode_t::epd_quality);
      display.setTextColor(TFT_BLACK, TFT_WHITE);
      display.setCursor(0, 0);
      display.clear(TFT_WHITE);
      
      // Display splash screen
      int centerX = display.width() / 2;
      int centerY = display.height() / 2;
      
      display.setTextSize(2);
      display.setCursor(centerX - 80, centerY - 30);
      display.println("M5Reader");
      
      display.setTextSize(1);
      display.setCursor(centerX - 140, centerY);
      display.println("EPUB Reader for M5Paper");
      
      display.setCursor(centerX - 100, centerY + 20);
      display.println("Initializing...");
      
      display.display();
      delay(1000);
    }
    
    // Set rotation to landscape if needed
    if (display.width() < display.height()) {
      display.setRotation(display.getRotation() ^ 1);
    }
    
    // ------------- BASIC STORAGE INITIALIZATION -------------
    
    // Clear display for status messages
    display.fillRect(0, 0, display.width(), 100, TFT_WHITE);
    display.setCursor(0, 0);
    display.setTextSize(1);
    
    // Simple demo mode - skip complex initialization
    bool demoMode = true;
    
    log("Entering demo mode...");
    display.display();
    delay(1000);
    
    // ------------- EPUB READER INITIALIZATION -------------
    
    // Create EPUB reader with dummy data
    epubReader = new EpubReader();
    
    if (epubReader == NULL) {
      log("Failed to create EpubReader!");
      delay(2000);
      ESP.restart();
    }
    
    log("Creating dummy book for demo.");
    display.display();
    
    // Initialize with dummy data
    epubReader->createDummyBook();
    
    // Configure display parameters
    epubReader->setDisplayParams(display.width(), display.height(), display.getTextSizeX());
    
    // Get total pages
    totalPages = epubReader->getTotalPages();
    log("Book loaded. Total pages: " + String(totalPages));
    display.display();
    
    delay(1000);
    display.clear(TFT_WHITE);
    displayCurrentPage();
    
    // Initialize power management
    lastActivityTime = millis();
    
  } catch (const std::exception& e) {
    log("Exception in setup: " + String(e.what()));
    delay(5000);
    ESP.restart();
  } catch (...) {
    log("Unknown exception in setup!");
    delay(5000);
    ESP.restart();
  }
}

void loop() {
  // Check for touch input
  checkTouchInput();
  
  // Refresh display if needed
  if (needsRefresh) {
    displayCurrentPage();
    needsRefresh = false;
  }
  
  // Check for inactivity timeout
  checkSleepTimeout();
  
  // Power saving
  delay(50);
}

void checkTouchInput() {
  touchCount = display.getTouch(&touchX, &touchY);
  
  if (touchCount) {
    // Reset sleep timer on any touch
    lastActivityTime = millis();
    
    bool pageChanged = false;
    
    // Right side of screen - next page
    if (touchX > display.width() / 2) {
      // Move to next page
      if (epubReader->nextPage()) {
        pageChanged = true;
      }
    } 
    // Left side of screen - previous page
    else {
      // Move to previous page
      if (epubReader->prevPage()) {
        pageChanged = true;
      }
    }
    
    if (pageChanged) {
      needsRefresh = true;
    }
    
    // Wait for touch release to avoid multiple page turns
    while (display.getTouch(&touchX, &touchY)) {
      delay(10);
    }
  }
}

void displayCurrentPage() {
  if (!epubReader->isInitialized()) {
    displayErrorScreen("Reader not initialized!");
    return;
  }
  
  display.waitDisplay();
  display.startWrite();
  display.clear(TFT_WHITE);
  
  // Render the current page
  epubReader->renderPage(display, 0, 0);
  
  display.endWrite();
  display.display();
}

void displayErrorScreen(const char* message) {
  display.waitDisplay();
  display.startWrite();
  display.clear(TFT_WHITE);
  
  // Draw error icon or symbol
  int centerX = display.width() / 2;
  int centerY = display.height() / 2 - 40;
  
  // Draw a warning triangle
  display.fillTriangle(
    centerX, centerY - 30,
    centerX - 30, centerY + 20,
    centerX + 30, centerY + 20,
    TFT_BLACK
  );
  
  display.fillTriangle(
    centerX, centerY - 20,
    centerX - 20, centerY + 10,
    centerX + 20, centerY + 10,
    TFT_WHITE
  );
  
  // Display error message
  display.setCursor(centerX - 100, centerY + 40);
  display.setTextColor(TFT_BLACK, TFT_WHITE);
  display.println(message);
  
  // Add instructions
  display.setCursor(centerX - 120, centerY + 80);
  display.println("Please add book.epub to SD card");
  display.setCursor(centerX - 120, centerY + 100);
  display.println("or restart the device to try again.");
  
  display.endWrite();
  display.display();
}

void checkSleepTimeout() {
  // Check if we should go to sleep due to inactivity
  if (millis() - lastActivityTime > SLEEP_TIMEOUT) {
    Serial.println("Going to sleep due to inactivity...");
    
    // Save current reading position if needed
    // saveReadingState();
    
    // Show sleep message
    display.setCursor(display.width()/2 - 100, display.height()/2);
    display.println("Going to sleep...");
    display.display();
    delay(1000);
    
    // Power down display
    display.sleep();
    
    // Go to deep sleep - in real implementation would use esp_deep_sleep
    // esp_deep_sleep_start();
    
    // For demo, just reset the timer
    lastActivityTime = millis();
  }
}