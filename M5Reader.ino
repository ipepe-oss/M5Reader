#include <epdiy.h>
#include <M5GFX.h>
#include <SD.h>
#include <SPI.h>
#include <FS.h>
#include <tinyxml2.h>
#include <miniz.h>

// Define SD card pin
#define PIN_SD_CS GPIO_NUM_4

// Display init
M5GFX display;

// Touch areas
const int NEXT_PAGE_AREA = 1;
const int PREV_PAGE_AREA = 0;
const int TOC_AREA = 2;
const int BACK_AREA = 3;
int touchArea = -1;

// UI states
enum UIState {
  SELECTING_BOOK,
  SELECTING_TOC,
  READING_BOOK
};

// Current UI state
UIState currentState = SELECTING_BOOK;

// Forward declarations
class ZipFile;
class Epub;
class EpubReader;
class BookList;

// ===== Zip File Handler =====
class ZipFile {
private:
  String m_filename;

public:
  ZipFile(const char* filename) {
    m_filename = filename;
  }
  
  ~ZipFile() {}
  
  uint8_t* readFileToMemory(const char* filename, size_t* size = nullptr) {
    // Open the zip file
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));
    bool status = mz_zip_reader_init_file(&zip_archive, m_filename.c_str(), 0);
    if (!status) {
      Serial.printf("Failed to open zip file: %s\n", m_filename.c_str());
      return nullptr;
    }
    
    // Find the file within the zip
    mz_uint32 file_index = 0;
    if (!mz_zip_reader_locate_file_v2(&zip_archive, filename, nullptr, 0, &file_index)) {
      Serial.printf("Could not find file %s in zip\n", filename);
      mz_zip_reader_end(&zip_archive);
      return nullptr;
    }
    
    // Get file size
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(&zip_archive, file_index, &file_stat)) {
      Serial.printf("Failed to get file stats\n");
      mz_zip_reader_end(&zip_archive);
      return nullptr;
    }
    
    // Allocate memory for file data (add 1 for null terminator for text files)
    size_t file_size = file_stat.m_uncomp_size;
    uint8_t* file_data = (uint8_t*)calloc(file_size + 1, 1);
    if (!file_data) {
      Serial.printf("Failed to allocate memory for %s\n", file_stat.m_filename);
      mz_zip_reader_end(&zip_archive);
      return nullptr;
    }
    
    // Extract file to memory
    status = mz_zip_reader_extract_to_mem(&zip_archive, file_index, file_data, file_size, 0);
    if (!status) {
      Serial.printf("Failed to extract file\n");
      free(file_data);
      mz_zip_reader_end(&zip_archive);
      return nullptr;
    }
    
    // Return the size if pointer provided
    if (size) {
      *size = file_size;
    }
    
    // Close zip file
    mz_zip_reader_end(&zip_archive);
    return file_data;
  }
};

// ===== EPUB Table of Contents Entry =====
struct TocEntry {
  String title;
  String href;
  String anchor;
  int level;
  
  TocEntry(String t, String h, String a, int l) : 
    title(t), href(h), anchor(a), level(l) {}
};

// ===== EPUB Class =====
class Epub {
private:
  String m_path;
  String m_title;
  String m_author;
  String m_coverImage;
  String m_tocNcxItem;
  String m_basePath;
  std::vector<std::pair<String, String>> m_spine;
  std::vector<TocEntry> m_toc;
  
  bool findContentOpfFile(ZipFile& zip, String& contentOpfFile) {
    // Read the container.xml to find content.opf
    char* metaInfo = (char*)zip.readFileToMemory("META-INF/container.xml");
    if (!metaInfo) {
      Serial.println("Could not find META-INF/container.xml");
      return false;
    }
    
    // Parse the metadata
    tinyxml2::XMLDocument metaDoc;
    auto result = metaDoc.Parse(metaInfo);
    free(metaInfo);
    
    if (result != tinyxml2::XML_SUCCESS) {
      Serial.println("Could not parse META-INF/container.xml");
      return false;
    }
    
    auto container = metaDoc.FirstChildElement("container");
    if (!container) {
      Serial.println("Could not find container element");
      return false;
    }
    
    auto rootfiles = container->FirstChildElement("rootfiles");
    if (!rootfiles) {
      Serial.println("Could not find rootfiles element");
      return false;
    }
    
    // Find the content.opf file
    auto rootfile = rootfiles->FirstChildElement("rootfile");
    while (rootfile) {
      const char* mediaType = rootfile->Attribute("media-type");
      if (mediaType && strcmp(mediaType, "application/oebps-package+xml") == 0) {
        const char* fullPath = rootfile->Attribute("full-path");
        if (fullPath) {
          contentOpfFile = fullPath;
          return true;
        }
      }
      rootfile = rootfile->NextSiblingElement("rootfile");
    }
    
    Serial.println("Could not get path to content.opf file");
    return false;
  }
  
  bool parseContentOpf(ZipFile& zip, String& contentOpfFile) {
    // Read content.opf file
    char* contents = (char*)zip.readFileToMemory(contentOpfFile.c_str());
    if (!contents) {
      Serial.printf("Could not read %s\n", contentOpfFile.c_str());
      return false;
    }
    
    // Parse content.opf
    tinyxml2::XMLDocument doc;
    auto result = doc.Parse(contents);
    free(contents);
    
    if (result != tinyxml2::XML_SUCCESS) {
      Serial.println("Error parsing content.opf");
      return false;
    }
    
    auto package = doc.FirstChildElement("package");
    if (!package) {
      Serial.println("Could not find package element");
      return false;
    }
    
    // Get metadata - title and cover image
    auto metadata = package->FirstChildElement("metadata");
    if (!metadata) {
      Serial.println("Missing metadata");
      return false;
    }
    
    // Get title
    auto title = metadata->FirstChildElement("dc:title");
    if (title && title->GetText()) {
      m_title = title->GetText();
    } else {
      m_title = "Unknown Title";
    }
    
    // Get author
    auto creator = metadata->FirstChildElement("dc:creator");
    if (creator && creator->GetText()) {
      m_author = creator->GetText();
    } else {
      m_author = "Unknown Author";
    }
    
    // Find cover image
    auto cover = metadata->FirstChildElement("meta");
    while (cover && cover->Attribute("name") && strcmp(cover->Attribute("name"), "cover") != 0) {
      cover = cover->NextSiblingElement("meta");
    }
    
    const char* coverItem = cover ? cover->Attribute("content") : nullptr;
    
    // Create manifest item mapping
    auto manifest = package->FirstChildElement("manifest");
    if (!manifest) {
      Serial.println("Missing manifest");
      return false;
    }
    
    std::map<String, String> items;
    auto item = manifest->FirstChildElement("item");
    
    while (item) {
      String itemId = item->Attribute("id");
      String href = m_basePath + item->Attribute("href");
      
      // Store cover image
      if (coverItem && itemId == coverItem) {
        m_coverImage = href;
      }
      
      // Store NCX file (table of contents)
      if (itemId == "ncx" || (item->Attribute("media-type") && 
          strcmp(item->Attribute("media-type"), "application/x-dtbncx+xml") == 0)) {
        m_tocNcxItem = href;
      }
      
      items[itemId] = href;
      item = item->NextSiblingElement("item");
    }
    
    // Find spine (reading order)
    auto spine = package->FirstChildElement("spine");
    if (!spine) {
      Serial.println("Missing spine");
      return false;
    }
    
    // Read spine items
    auto itemref = spine->FirstChildElement("itemref");
    while (itemref) {
      auto id = itemref->Attribute("idref");
      if (items.find(id) != items.end()) {
        m_spine.push_back(std::make_pair(id, items[id]));
      }
      itemref = itemref->NextSiblingElement("itemref");
    }
    
    return true;
  }
  
  bool parseTocNcxFile(ZipFile& zip) {
    // Check if we have a TOC file
    if (m_tocNcxItem.isEmpty()) {
      Serial.println("No NCX file specified");
      return false;
    }
    
    // Read the NCX file
    char* ncxData = (char*)zip.readFileToMemory(m_tocNcxItem.c_str());
    if (!ncxData) {
      Serial.printf("Could not find %s\n", m_tocNcxItem.c_str());
      return false;
    }
    
    // Parse the TOC
    tinyxml2::XMLDocument doc;
    auto result = doc.Parse(ncxData);
    free(ncxData);
    
    if (result != tinyxml2::XML_SUCCESS) {
      Serial.println("Error parsing TOC");
      return false;
    }
    
    auto ncx = doc.FirstChildElement("ncx");
    if (!ncx) {
      Serial.println("Could not find ncx element");
      return false;
    }
    
    auto navMap = ncx->FirstChildElement("navMap");
    if (!navMap) {
      Serial.println("Could not find navMap element");
      return false;
    }
    
    // Process navigation points
    auto navPoint = navMap->FirstChildElement("navPoint");
    while (navPoint) {
      // Get title
      auto navLabel = navPoint->FirstChildElement("navLabel");
      if (navLabel) {
        auto text = navLabel->FirstChildElement("text");
        if (text && text->GetText()) {
          String title = text->GetText();
          
          // Get destination
          auto content = navPoint->FirstChildElement("content");
          if (content && content->Attribute("src")) {
            String href = m_basePath + content->Attribute("src");
            
            // Split href and anchor
            String anchor = "";
            int hashPos = href.indexOf('#');
            if (hashPos > 0) {
              anchor = href.substring(hashPos + 1);
              href = href.substring(0, hashPos);
            }
            
            // Add to table of contents
            m_toc.push_back(TocEntry(title, href, anchor, 0));
          }
        }
      }
      navPoint = navPoint->NextSiblingElement("navPoint");
    }
    
    return true;
  }
  
  String normalizePath(const String& path) {
    // Split path into components
    std::vector<String> components;
    int start = 0;
    int end = 0;
    
    while ((end = path.indexOf('/', start)) >= 0) {
      if (end > start) {
        String component = path.substring(start, end);
        if (component == "..") {
          if (!components.empty()) {
            components.pop_back();
          }
        } else if (component != ".") {
          components.push_back(component);
        }
      }
      start = end + 1;
    }
    
    // Handle the last component
    if (start < path.length()) {
      String component = path.substring(start);
      if (component == "..") {
        if (!components.empty()) {
          components.pop_back();
        }
      } else if (component != ".") {
        components.push_back(component);
      }
    }
    
    // Rebuild path
    String result = "";
    for (auto& component : components) {
      if (result.length() > 0) {
        result += "/";
      }
      result += component;
    }
    
    return result;
  }

public:
  Epub(const String& path) : m_path(path) {}
  
  ~Epub() {}
  
  bool load() {
    ZipFile zip(m_path.c_str());
    String contentOpfFile;
    
    // Find and parse content.opf
    if (!findContentOpfFile(zip, contentOpfFile)) {
      return false;
    }
    
    // Get base path for content
    m_basePath = contentOpfFile.substring(0, contentOpfFile.lastIndexOf('/') + 1);
    
    // Parse content.opf
    if (!parseContentOpf(zip, contentOpfFile)) {
      return false;
    }
    
    // Parse table of contents
    if (!parseTocNcxFile(zip)) {
      Serial.println("No TOC found, proceeding anyway");
    }
    
    return true;
  }
  
  String getTitle() { return m_title; }
  
  String getAuthor() { return m_author; }
  
  String getPath() { return m_path; }
  
  uint8_t* getItemContents(const String& itemHref, size_t* size = nullptr) {
    ZipFile zip(m_path.c_str());
    String path = normalizePath(itemHref);
    return zip.readFileToMemory(path.c_str(), size);
  }
  
  int getSpineItemsCount() { return m_spine.size(); }
  
  String getSpineItem(int spineIndex) {
    if (spineIndex >= 0 && spineIndex < m_spine.size()) {
      return m_spine[spineIndex].second;
    }
    // Return first item if index is out of range
    return m_spine[0].second;
  }
  
  int getTocItemsCount() { return m_toc.size(); }
  
  TocEntry& getTocItem(int tocIndex) {
    return m_toc[tocIndex];
  }
  
  int getSpineIndexForTocIndex(int tocIndex) {
    // Find spine index for the corresponding TOC item
    if (tocIndex >= 0 && tocIndex < m_toc.size()) {
      String href = m_toc[tocIndex].href;
      
      for (int i = 0; i < m_spine.size(); i++) {
        if (m_spine[i].second == href) {
          return i;
        }
      }
    }
    
    return 0; // Default to start
  }
};

// ===== Page Element classes =====
class PageElement {
public:
  int yPos;
  
  PageElement(int y) : yPos(y) {}
  virtual ~PageElement() {}
  virtual void render(M5GFX& display) = 0;
};

class TextElement : public PageElement {
public:
  String text;
  bool isBold;
  bool isItalic;
  
  TextElement(const String& t, int y, bool bold = false, bool italic = false) 
    : PageElement(y), text(t), isBold(bold), isItalic(italic) {}
  
  void render(M5GFX& display) override {
    display.setCursor(10, yPos);
    
    // Set text style
    if (isBold && isItalic) {
      display.setTextStyle(textStyle::italic_bold);
    } else if (isBold) {
      display.setTextStyle(textStyle::bold);
    } else if (isItalic) {
      display.setTextStyle(textStyle::italic);
    } else {
      display.setTextStyle(textStyle::normal);
    }
    
    display.println(text);
  }
};

class ImageElement : public PageElement {
public:
  Epub* epub;
  String src;
  int width;
  int height;
  
  ImageElement(Epub* e, const String& s, int y, int w, int h) 
    : PageElement(y), epub(e), src(s), width(w), height(h) {}
  
  void render(M5GFX& display) override {
    // Simple placeholder for now
    display.drawRect(10, yPos, width, height, TFT_BLACK);
    display.setCursor(width/2 - 50, yPos + height/2 - 10);
    display.println("[Image]");
    
    // TODO: Implement actual image rendering with M5GFX
    // This would require loading the image from the EPUB
    // and rendering it on the display
  }
};

// ===== Page class =====
class Page {
public:
  std::vector<PageElement*> elements;
  
  ~Page() {
    for (auto element : elements) {
      delete element;
    }
    elements.clear();
  }
  
  void render(M5GFX& display) {
    for (auto element : elements) {
      element->render(display);
    }
  }
};

// ===== HTML Parser =====
class HtmlParser : public tinyxml2::XMLVisitor {
private:
  std::vector<Page*> pages;
  Page* currentPage;
  Epub* epub;
  int currentY;
  int pageHeight;
  bool isBold;
  bool isItalic;
  String basePath;
  
  void startNewPage() {
    currentPage = new Page();
    pages.push_back(currentPage);
    currentY = 0;
  }
  
  void addText(const String& text, bool bold, bool italic) {
    if (text.trim().length() == 0) return;
    
    // Create new page if needed
    if (currentPage == nullptr) {
      startNewPage();
    }
    
    // Check if we need to move to next page
    if (currentY + 30 > pageHeight) {
      startNewPage();
    }
    
    // Add text element
    currentPage->elements.push_back(new TextElement(text, currentY, bold, italic));
    currentY += 30; // Simple line height
  }

public:
  HtmlParser(Epub* e, const String& base, int height) 
    : epub(e), basePath(base), pageHeight(height), currentY(0), 
      isBold(false), isItalic(false), currentPage(nullptr) {}
  
  ~HtmlParser() {
    for (auto page : pages) {
      delete page;
    }
  }
  
  // XML Parser callbacks
  bool VisitEnter(const tinyxml2::XMLElement& element, const tinyxml2::XMLAttribute* firstAttribute) {
    String elementName = element.Name();
    
    if (elementName == "b" || elementName == "strong") {
      isBold = true;
    } else if (elementName == "i" || elementName == "em") {
      isItalic = true;
    } else if (elementName == "img") {
      // Handle image
      const char* src = element.Attribute("src");
      if (src) {
        String imgSrc = basePath + src;
        
        // Create a new page if needed
        if (currentPage == nullptr) {
          startNewPage();
        }
        
        // Simple fixed image size for now
        int imgWidth = 300;
        int imgHeight = 200;
        
        // Check if we need a new page
        if (currentY + imgHeight > pageHeight) {
          startNewPage();
        }
        
        // Add image
        currentPage->elements.push_back(new ImageElement(epub, imgSrc, currentY, imgWidth, imgHeight));
        currentY += imgHeight + 20; // Add some padding
      }
    } else if (elementName == "p" || elementName == "div" || elementName == "h1" || 
               elementName == "h2" || elementName == "h3" || elementName == "h4") {
      // Add some spacing
      currentY += 10;
    }
    
    return true;
  }
  
  bool Visit(const tinyxml2::XMLText& text) {
    addText(text.Value(), isBold, isItalic);
    return true;
  }
  
  bool VisitExit(const tinyxml2::XMLElement& element) {
    String elementName = element.Name();
    
    if (elementName == "b" || elementName == "strong") {
      isBold = false;
    } else if (elementName == "i" || elementName == "em") {
      isItalic = false;
    } else if (elementName == "p" || elementName == "div" || elementName == "h1" || 
               elementName == "h2" || elementName == "h3" || elementName == "h4") {
      // Add paragraph spacing
      currentY += 10;
    }
    
    return true;
  }
  
  void parse(const char* html, int length) {
    tinyxml2::XMLDocument doc;
    doc.Parse(html, length);
    doc.Accept(this);
  }
  
  int getPageCount() {
    return pages.size();
  }
  
  void renderPage(int pageIndex, M5GFX& display) {
    if (pageIndex >= 0 && pageIndex < pages.size()) {
      pages[pageIndex]->render(display);
    }
  }
};

// ===== EPUB Reader =====
class EpubReader {
private:
  Epub* epub;
  HtmlParser* parser;
  int currentSection;
  int currentPage;
  int pagesInCurrentSection;
  
  void parseAndLayoutCurrentSection(M5GFX& display) {
    if (parser != nullptr) {
      delete parser;
      parser = nullptr;
    }
    
    // Show loading message
    display.clear(TFT_WHITE);
    display.setCursor(10, 10);
    display.println("Loading...");
    display.display();
    
    // Get the current section content
    String item = epub->getSpineItem(currentSection);
    String basePath = item.substring(0, item.lastIndexOf('/') + 1);
    
    size_t contentSize;
    char* html = (char*)epub->getItemContents(item, &contentSize);
    
    if (!html) {
      display.setCursor(10, 50);
      display.println("Error: Could not load section");
      display.display();
      return;
    }
    
    // Create parser
    parser = new HtmlParser(epub, basePath, display.height() - 40);
    parser->parse(html, contentSize);
    free(html);
    
    // Store page count
    pagesInCurrentSection = parser->getPageCount();
  }

public:
  EpubReader() : epub(nullptr), parser(nullptr), 
                 currentSection(0), currentPage(0), pagesInCurrentSection(0) {}
  
  ~EpubReader() {
    if (parser) delete parser;
    if (epub) delete epub;
  }
  
  bool open(fs::FS &fs, const char* path) {
    // Check if file exists
    File file = fs.open(path);
    if (!file) {
      return false;
    }
    file.close();
    
    // Create new Epub object
    if (epub) delete epub;
    epub = new Epub(path);
    
    // Load EPUB metadata
    return epub->load();
  }
  
  void next(M5GFX& display) {
    currentPage++;
    if (currentPage >= pagesInCurrentSection) {
      currentSection++;
      currentPage = 0;
      
      if (currentSection >= epub->getSpineItemsCount()) {
        currentSection = epub->getSpineItemsCount() - 1;
        currentPage = pagesInCurrentSection - 1;
        return;
      }
      
      parseAndLayoutCurrentSection(display);
    }
  }
  
  void prev(M5GFX& display) {
    if (currentPage > 0) {
      currentPage--;
    } else {
      if (currentSection > 0) {
        currentSection--;
        parseAndLayoutCurrentSection(display);
        currentPage = pagesInCurrentSection - 1;
      }
    }
  }
  
  void render(M5GFX& display) {
    if (!parser) {
      parseAndLayoutCurrentSection(display);
    }
    
    display.clear(TFT_WHITE);
    
    // Render current page
    parser->renderPage(currentPage, display);
    
    // Draw page number
    display.setCursor(display.width()/2 - 50, display.height() - 20);
    display.printf("Page %d of %d", currentPage + 1, pagesInCurrentSection);
  }
  
  void goToSection(int section, M5GFX& display) {
    currentSection = section;
    currentPage = 0;
    parseAndLayoutCurrentSection(display);
  }
  
  String getTitle() {
    return epub ? epub->getTitle() : "No Book";
  }
  
  String getAuthor() {
    return epub ? epub->getAuthor() : "";
  }
  
  int getTocItemsCount() {
    return epub ? epub->getTocItemsCount() : 0;
  }
  
  TocEntry& getTocItem(int index) {
    return epub->getTocItem(index);
  }
  
  void goToTocItem(int tocIndex, M5GFX& display) {
    int spineIndex = epub->getSpineIndexForTocIndex(tocIndex);
    goToSection(spineIndex, display);
  }
};

// ===== Book List =====
class BookList {
private:
  std::vector<String> books;
  int selectedIndex;

public:
  BookList() : selectedIndex(0) {}
  
  void scan(fs::FS &fs, const char* directory = "/") {
    books.clear();
    
    File root = fs.open(directory);
    if (!root || !root.isDirectory()) {
      return;
    }
    
    File file = root.openNextFile();
    while (file) {
      String fileName = file.name();
      if (fileName.endsWith(".epub")) {
        books.push_back(fileName);
      }
      file = root.openNextFile();
    }
    
    selectedIndex = 0;
  }
  
  void next() {
    if (selectedIndex < books.size() - 1) {
      selectedIndex++;
    }
  }
  
  void prev() {
    if (selectedIndex > 0) {
      selectedIndex--;
    }
  }
  
  String getSelected() {
    if (selectedIndex >= 0 && selectedIndex < books.size()) {
      return books[selectedIndex];
    }
    return "";
  }
  
  int getCount() {
    return books.size();
  }
  
  void render(M5GFX& display) {
    display.clear(TFT_WHITE);
    display.setCursor(10, 10);
    display.println("Available Books:");
    display.drawLine(10, 35, display.width() - 10, 35, TFT_BLACK);
    
    int startIdx = max(0, selectedIndex - 5);
    int endIdx = min(startIdx + 10, (int)books.size());
    
    for (int i = startIdx; i < endIdx; i++) {
      display.setCursor(20, 50 + (i - startIdx) * 30);
      
      // Highlight selected book
      if (i == selectedIndex) {
        display.fillRect(10, 45 + (i - startIdx) * 30, display.width() - 20, 30, TFT_DARKGREY);
        display.setTextColor(TFT_WHITE);
      } else {
        display.setTextColor(TFT_BLACK);
      }
      
      // Get book name from path
      String bookName = books[i];
      int lastSlash = bookName.lastIndexOf('/');
      if (lastSlash >= 0) {
        bookName = bookName.substring(lastSlash + 1);
      }
      
      display.println(bookName);
      display.setTextColor(TFT_BLACK);
    }
    
    // Draw navigation cues
    display.setCursor(10, display.height() - 20);
    display.println("Use touch to navigate and select");
  }
};

// ===== Table of Contents =====
class TableOfContents {
private:
  EpubReader* reader;
  int selectedIndex;
  int itemCount;

public:
  TableOfContents(EpubReader* r) : reader(r), selectedIndex(0) {
    itemCount = reader->getTocItemsCount();
  }
  
  void next() {
    if (selectedIndex < itemCount - 1) {
      selectedIndex++;
    }
  }
  
  void prev() {
    if (selectedIndex > 0) {
      selectedIndex--;
    }
  }
  
  int getSelected() {
    return selectedIndex;
  }
  
  void render(M5GFX& display) {
    display.clear(TFT_WHITE);
    display.setCursor(10, 10);
    display.println("Table of Contents:");
    display.setCursor(10, 30);
    display.println(reader->getTitle());
    display.drawLine(10, 55, display.width() - 10, 55, TFT_BLACK);
    
    int startIdx = max(0, selectedIndex - 5);
    int endIdx = min(startIdx + 10, itemCount);
    
    for (int i = startIdx; i < endIdx; i++) {
      display.setCursor(20, 70 + (i - startIdx) * 30);
      
      // Highlight selected item
      if (i == selectedIndex) {
        display.fillRect(10, 65 + (i - startIdx) * 30, display.width() - 20, 30, TFT_DARKGREY);
        display.setTextColor(TFT_WHITE);
      } else {
        display.setTextColor(TFT_BLACK);
      }
      
      TocEntry& entry = reader->getTocItem(i);
      display.println(entry.title);
      display.setTextColor(TFT_BLACK);
    }
    
    // Draw navigation cues
    display.setCursor(10, display.height() - 20);
    display.println("Use touch to navigate and select");
  }
};

// ===== Global variables =====
EpubReader epubReader;
BookList bookList;
TableOfContents* toc = nullptr;

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  display.init();
  if (display.isEPD()) {
    display.setEpdMode(epd_mode_t::epd_text);
    display.invertDisplay(true);
    display.clear(TFT_WHITE);
  }
  
  // Set rotation to landscape if needed
  if (display.width() < display.height()) {
    display.setRotation(display.getRotation() ^ 1);
  }
  
  // Setup text
  display.setTextSize(1);
  display.setTextColor(TFT_BLACK, TFT_WHITE);
  
  // Initialize SD card
  display.setCursor(0, 0);
  display.println("Initializing SD card...");
  display.display();
  
  if (!SD.begin(PIN_SD_CS, SPI, 25000000)) {
    display.println("SD Card initialization failed!");
    display.display();
    return;
  }
  
  display.println("SD Card initialized.");
  display.display();
  
  // Scan for books
  bookList.scan(SD, "/");
  display.println("Found " + String(bookList.getCount()) + " books");
  display.display();
  
  delay(1000);
  display.clear(TFT_WHITE);
  bookList.render(display);
  display.display();
}

void loop() {
  checkTouchInput();
  delay(50);
}

void checkTouchInput() {
  if (display.getTouch(&touchArea)) {
    switch (currentState) {
      case SELECTING_BOOK:
        handleBookListInput();
        break;
      case SELECTING_TOC:
        handleTocInput();
        break;
      case READING_BOOK:
        handleReaderInput();
        break;
    }
    display.display();
  }
}

void handleBookListInput() {
  if (touchArea == NEXT_PAGE_AREA) {
    bookList.next();
    bookList.render(display);
  } else if (touchArea == PREV_PAGE_AREA) {
    bookList.prev();
    bookList.render(display);
  } else {
    // Book selected, open it
    String selectedBook = bookList.getSelected();
    if (!selectedBook.isEmpty()) {
      if (epubReader.open(SD, selectedBook.c_str())) {
        // Create TOC if it doesn't exist
        if (toc != nullptr) {
          delete toc;
        }
        toc = new TableOfContents(&epubReader);
        
        // Show the table of contents
        currentState = SELECTING_TOC;
        toc->render(display);
      } else {
        display.clear(TFT_WHITE);
        display.setCursor(10, 10);
        display.println("Error: Could not open book");
        display.display();
        delay(2000);
        bookList.render(display);
      }
    }
  }
}

void handleTocInput() {
  if (touchArea == NEXT_PAGE_AREA) {
    toc->next();
    toc->render(display);
  } else if (touchArea == PREV_PAGE_AREA) {
    toc->prev();
    toc->render(display);
  } else if (touchArea == BACK_AREA) {
    // Go back to book list
    currentState = SELECTING_BOOK;
    bookList.render(display);
  } else {
    // Item selected, go to section
    int selected = toc->getSelected();
    epubReader.goToTocItem(selected, display);
    currentState = READING_BOOK;
    epubReader.render(display);
  }
}

void handleReaderInput() {
  if (touchArea == NEXT_PAGE_AREA) {
    epubReader.next(display);
    epubReader.render(display);
  } else if (touchArea == PREV_PAGE_AREA) {
    epubReader.prev(display);
    epubReader.render(display);
  } else if (touchArea == TOC_AREA) {
    // Go back to TOC
    currentState = SELECTING_TOC;
    toc->render(display);
  }
}