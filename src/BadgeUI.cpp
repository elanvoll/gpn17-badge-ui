#include <Adafruit_GFX.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans7pt7b.h>
#include <Fonts/Org_01.h>
#include <FS.h>
#include <TFT_ILI9163C.h>

#include <BadgeUI.h>

//Stolen form https://github.com/Jan--Henrik/TFT_ILI9163C/blob/master/examples/SD_example/SD_example.ino
uint32_t read32(File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

//Stolen form https://github.com/Jan--Henrik/TFT_ILI9163C/blob/master/examples/SD_example/SD_example.ino
uint16_t read16(File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}


void SimpleTextDisplay::draw(TFT_ILI9163C* tft, Theme * theme, uint16_t offsetX, uint16_t offsetY) {
    tft->fillScreen(theme->backgroundColor);
    tft->setTextColor(theme->textColor);
    tft->setFont(&FreeSans9pt7b);
    tft->setCursor(2 + offsetX, 18 + offsetY);
    tft->print(this->text);
    this->dirty = false;
}

void FullScreenStatus::draw(TFT_ILI9163C* tft, Theme * theme, uint16_t offsetX, uint16_t offsetY) {
    tft->fillScreen(theme->backgroundColor);
    tft->setTextColor(theme->textColor);
    tft->setFont(&FreeSans24pt7b);
    tft->setTextSize(1);
    tft->setCursor(5, 75);
    tft->print(this->main);
    tft->setFont(&FreeSans9pt7b);
    tft->setCursor(12, 105);
    tft->print(this->sub);
    this->dirty = false;
}

// Stolen from https://github.com/Jan--Henrik/TFT_ILI9163C/blob/master/examples/SD_example/SD_example.ino
void FullScreenBMPStatus::draw(TFT_ILI9163C* tft, Theme * theme, uint16_t offsetX, uint16_t offsetY) {
    tft->fillScreen(theme->backgroundColor);
    tft->setTextColor(theme->textColor);
    if(bmp) {
        bmpDraw(this->bmp, bmpx, bmpy, tft);
    }
    tft->setFont(&FreeSans9pt7b);
    tft->setCursor(subx, suby);
    tft->print(this->sub);
    this->dirty = false;
}

void FullScreenBMPDisplay::draw(TFT_ILI9163C* tft, Theme * theme, uint16_t offsetX, uint16_t offsetY) {
    tft->fillScreen(theme->backgroundColor);
    if(bmp) {
        bmpDraw(this->bmp, 0, 0, tft);
        this->dirty = false;
    }
}

void Menu::draw(TFT_ILI9163C* tft, Theme * theme, uint16_t offsetX, uint16_t offsetY) {
    tft->fillScreen(theme->backgroundColor);
    MenuItem * currentDraw = firstVisible;
    uint16_t width = _TFTWIDTH-offsetX-1;
    uint16_t height = _TFTHEIGHT-offsetY-1;
    int i = 0;
    while(currentDraw && i < itemsPerPage) {
        tft->setCursor(10+offsetX, (i*height/itemsPerPage)+25+offsetY);
        tft->drawLine(offsetX, i*height/itemsPerPage+offsetY, width, i*height/itemsPerPage+offsetY, theme->foregroundColor);
        currentDraw->draw(tft, theme, 0, 0);
        currentDraw = currentDraw->next;
        i++;
    }
    this->dirty = false;
}

void MenuItem::draw(TFT_ILI9163C* tft, Theme * theme, uint16_t offsetX, uint16_t offsetY){
    tft->setFont(&FreeSans7pt7b);
    tft->setTextSize(1);
    if(selected) {
        tft->setTextColor(theme->selectedTextColor);
    } else {
        tft->setTextColor(theme->textColor);
    }
    tft->print(this->text);
}

void StatusOverlay::draw(TFT_ILI9163C* tft, Theme * theme, uint16_t offsetX, uint16_t offsetY) {
    tft->fillRect(0, 0,_TFTWIDTH, 11, theme->backgroundColor);
    tft->setTextColor(theme->textColor);
    tft->setCursor(0, 7);
    tft->setTextSize(1);
    tft->setFont(&Org_01);
    tft->print(wifi);
    tft->setCursor(70, 7);
    tft->printf("% 2d:%02d %d%%", this->hours, this->minutes,
                std::max(std::min(int((bat-batCritical)/float(batFull-batCritical)*100), 100),0));
    tft->drawLine(0, 11, _TFTWIDTH-1, 11, theme->foregroundColor);
    dirty = false;
}

uint16_t StatusOverlay::getOffsetX(){
    return 0;
}

uint16_t StatusOverlay::getOffsetY(){
    return 11;
}

void NotificationScreen::draw(TFT_ILI9163C * tft, Theme * theme, uint16_t offsetX, uint16_t offsetY) {
    tft->fillScreen(theme->backgroundColor);
    tft->setCursor(0,15);
    tft->setTextColor(theme->textColor);
    tft->setFont(&FreeSans7pt7b);
    if(location == "") {
        tft->printf("%s\n\n%s", summary.c_str(), description.c_str()); 
    } else {
        tft->printf("%s\n@%s\n\n%s", summary.c_str(), location.c_str(), description.c_str()); 
    }
    dirty = false;
}

void BMPRender::bmpDraw(const char *filename, uint8_t x, uint16_t y, TFT_ILI9163C* tft) {
    File     bmpFile;
    uint16_t bmpWidth, bmpHeight;   // W+H in pixels
    uint8_t  bmpDepth;              // Bit depth (currently must be 24)
    uint32_t bmpImageoffset;        // Start of image data in file
    uint32_t rowSize;               // Not always = bmpWidth; may have padding
    uint8_t  sdbufferLen = BUFFPIXEL * 3;
    uint8_t  sdbuffer[sdbufferLen]; // pixel buffer (R+G+B per pixel)
    uint8_t  buffidx = sdbufferLen; // Current position in sdbuffer
    boolean  goodBmp = false;       // Set to true on valid header parse
    boolean  flip    = true;        // BMP is stored bottom-to-top
    uint16_t w, h, row, col;
    uint8_t  r, g, b;
    uint32_t pos = 0;

    if((x >= tft->width()) || (y >= tft->height())) return;

    // Open requested file on SD card
    if ((bmpFile = SPIFFS.open(filename,"r")) == NULL) {
      tft->setCursor(20,20);
      tft->print("file not found!");
      return;
    }

    // Parse BMP header
    if(read16(bmpFile) == 0x4D42) { // BMP signature
      read32(bmpFile);
      (void)read32(bmpFile); // Read & ignore creator bytes
      bmpImageoffset = read32(bmpFile); // Start of image data
      // Read DIB header
      read32(bmpFile);
      bmpWidth  = read32(bmpFile);
      bmpHeight = read32(bmpFile);
      if(read16(bmpFile) == 1) { // # planes -- must be '1'
	bmpDepth = read16(bmpFile); // bits per pixel
	if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed
	  goodBmp = true; // Supported BMP format -- proceed!
	  rowSize = (bmpWidth * 3 + 3) & ~3;// BMP rows are padded (if needed) to 4-byte boundary
	  if (bmpHeight < 0) {
	    bmpHeight = -bmpHeight;
	    flip      = false;
	  }
	  // Crop area to be loaded
	  w = bmpWidth;
	  h = bmpHeight;
	  if((x+w-1) >= tft->width())  w = tft->width()  - x;
	  if((y+h-1) >= tft->height()) h = tft->height() - y;
	  //tft->startPushData(x, y, x+w-1, y+h-1);
	  for (row=0; row<h; row++) { // For each scanline...
        uint8_t row_data[w * 2];
	    if (flip){ // Bitmap is stored bottom-to-top order (normal BMP)
	      pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
	    } 
	    else {     // Bitmap is stored top-to-bottom
	      pos = bmpImageoffset + row * rowSize;
	    }
	    if (bmpFile.position() != pos) { // Need seek?
	      bmpFile.seek(pos, SeekSet);
	      buffidx = sdbufferLen; // Force buffer reload
	    }
	    for (col=0; col<w; col++) { // For each pixel...
	      // Time to read more pixel data?
	      if (buffidx >= sdbufferLen) { // Indeed
	        bmpFile.read(sdbuffer, sdbufferLen);
	        buffidx = 0; // Set index to beginning
	      }
	      // Convert pixel from BMP to TFT format, push to display
	      b = sdbuffer[buffidx++];
	      g = sdbuffer[buffidx++];
	      r = sdbuffer[buffidx++];
          uint16_t val = tft->Color565(r,g,b);
          row_data[2 * col + 1] = (uint8_t)val;
          row_data[2 * col] = *(((uint8_t*)&val) + 1);
	    } // end pixel
        tft->writeRow(y + row, x, w, row_data);
	  } // end scanline
	  //tft->endPushData();
	} // end goodBmp
      }
    }

    bmpFile.close();
    if(!goodBmp) {
      tft->setCursor(20,20);
      tft->print("file unrecognized!");
    }
}

