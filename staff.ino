/***************************************************
  This is a example sketch demonstrating bitmap drawing
  capabilities of the SSD1351 library for the 1.5"
  and 1.27" 16-bit Color OLEDs with SSD1351 driver chip

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/products/1431
  ------> http://www.adafruit.com/products/1673

  If you're using a 1.27" OLED, change SSD1351HEIGHT in Adafruit_SSD1351.h
 	to 96 instead of 128

  These displays use SPI to communicate, 4 or 5 pins are required to
  interface
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  BSD license, all text above must be included in any redistribution

  The Adafruit GFX Graphics core library is also required
  https://github.com/adafruit/Adafruit-GFX-Library
  Be sure to install it!
 ****************************************************/

/************************************************************************
 * Includes
 ************************************************************************/

#include <Adafruit_Sensor.h>
#include <Adafruit_LSM303_U.h>
#include <Adafruit_L3GD20_U.h>
#include <Adafruit_9DOF.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>

/************************************************************************
 * Constants
 ************************************************************************/

#undef DEBUG

 // If we are using the hardware SPI interface, these are the pins (for future ref)
#define sclk 13
#define mosi 11
#define dc   7
#define cs   9
#define rst  8

// For Arduino Uno/Duemilanove, etc
//  connect the SD card with MOSI going to pin 11, MISO going to pin 12 and SCK going to pin 13 (standard)
//  Then pin 10 goes to CS (or whatever you have set up)
#define SD_CS 10    // Set the chip select line to whatever you use (10 doesnt conflict with the library)


// Color definitions
#define	BLACK           0x0000
#define	BLUE            0x001F
#define	RED             0xF800
#define	GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define WHITE           0xFFFF

/************************************************************************
 * global variables
 ************************************************************************/

// to draw images from the SD card, we will share the hardware SPI interface
Adafruit_SSD1351 tft = Adafruit_SSD1351(cs, dc, rst);

/* Assign a unique ID to the sensors */
Adafruit_9DOF                dof   = Adafruit_9DOF();
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified(30301);
Adafruit_LSM303_Mag_Unified   mag   = Adafruit_LSM303_Mag_Unified(30302);

/* Update this with the correct SLP for accurate altitude measurements */
float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;

// the file itself
File bmpFile;

// information we extract about the bitmap file
int bmpWidth, bmpHeight;
uint8_t bmpDepth, bmpImageoffset;

SPISettings settings(24000000, MSBFIRST, SPI_MODE3); // Teensy 3.1 max SPI

/************************************************************************
 * helper functions
 ************************************************************************/

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}


// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

#define BUFFPIXEL 64

void bmpDraw(char *filename, uint8_t x, uint8_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();
  int bmpInt;

  if((x >= tft.width()) || (y >= tft.height())) return;

#ifdef DEBUG
  Serial.println();
  Serial.print("Loading image '");
  Serial.print(filename);
  Serial.println('\'');
#endif
  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
#ifdef DEBUG
    Serial.print("File not found");
#endif
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    bmpInt = read32(bmpFile);
#ifdef DEBUG
    Serial.print("File size: "); Serial.println(bmpInt);
#endif
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    bmpInt = read32(bmpFile);
#ifdef DEBUG
    Serial.print("Image Offset: "); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print("Header size: "); Serial.println(bmpInt);
#endif
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
#ifdef DEBUG
      Serial.print("Bit Depth: "); Serial.println(bmpDepth);
#endif
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
#ifdef DEBUG
        Serial.print("Image size: ");
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);
#endif
        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        for (row=0; row<h; row++) { // For each scanline...
          tft.goTo(x, y+row);

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          // optimize by setting pins now
          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];

            //            tft.drawPixel(x+col, y+row, YELLOW);
                        tft.drawPixel(x+col, y+row, tft.Color565(r,g,b));
            // optimized!
            //tft.pushColor(tft.Color565(r,g,b));
          } // end pixel
        } // end scanline
#ifdef DEBUG
        Serial.print("Loaded in ");
        Serial.print(millis() - startTime);
        Serial.println(" ms");
#endif
      } // end goodBmp
    }
  }

  bmpFile.close();
#ifdef DEBUG
  if(!goodBmp) Serial.println("BMP format not recognized.");
#endif

}

void setupOLED () {
  pinMode(cs, OUTPUT);
  digitalWrite(cs, HIGH);

  pinMode(SD_CS, OUTPUT);

  // initialize the OLED
  tft.begin();
#ifdef DEBUG
  Serial.println("init");
#endif
  SPI.beginTransaction(settings);
  tft.fillScreen(WHITE);
  SPI.endTransaction();

  delay(500);
#ifdef DEBUG
  Serial.print("Initializing SD card...");
#endif
  if (!SD.begin(SD_CS)) {
#ifdef DEBUG
    Serial.println("failed!");
#endif
    return;
  }
#ifdef DEBUG
  Serial.println("SD OK!");
#endif
  SPI.beginTransaction(settings);
  tft.fillScreen(BLACK);
  bmpDraw("8ball2.bmp", 0, 0);
  SPI.endTransaction();
  delay(2000);
  //  tft.fillScreen(RED);
}


/**************************************************************************
 *   Initialise the sensors
 **************************************************************************/
void initSensors()
{
  if(!accel.begin())
  {
    /* There was a problem detecting the LSM303 ... check your connections */
    Serial.println(F("Ooops, no LSM303 detected ... Check your wiring!"));
    while(1);
  }
  if(!mag.begin())
  {
    /* There was a problem detecting the LSM303 ... check your connections */
    Serial.println("Ooops, no LSM303 detected ... Check your wiring!");
    while(1);
  }
}

void setup(void) {
#ifdef DEBUG
  Serial.begin(9600);
#endif
  setupOLED();
  initSensors();
}

enum BallState { Up, Down, GoingDown, GoingUp };

void loop() {
  sensors_event_t accel_event;
//  sensors_event_t mag_event;
  sensors_vec_t   orientation;
  static BallState state = Up;

  /* Calculate pitch and roll from the raw accelerometer data */
  accel.getEvent(&accel_event);
  if (dof.accelGetOrientation(&accel_event, &orientation))
  {
    /* 'orientation' should have valid .roll and .pitch fields */
/*
    Serial.print(F("Roll: "));
    Serial.print(orientation.roll);
    Serial.print(F("; "));
    Serial.print(F("Pitch: "));
    Serial.print(orientation.pitch);
    Serial.print(F("; "));
*/
#ifdef DEBUG
    tft.fillScreen(BLUE);
    tft.setCursor(0,0);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.print(F("Roll: "));
    tft.print(orientation.roll);
    tft.print(F("; "));
    tft.setCursor(0,32);
    tft.print(F("Pitch: "));
    tft.print(orientation.pitch);
    tft.print(F("; "));
#endif

    switch (state) {
    case Up:
      if (orientation.roll > -15) {
        state = GoingDown;
      }
      break;
    case Down:
      if (orientation.roll <  -15) {
        state = GoingUp;
      }
      break;
    case GoingDown:
      if (orientation.roll > -15) {
        state = Down;
      } else {
        state = GoingUp;
      }
      break;
    case GoingUp:
      if (orientation.roll < -15) {
        state = Up;
      } else {
        state = GoingDown;
      }
      break;
    };

#ifdef DEBUG
    tft.setCursor(0,64);
    switch (state) {
    case Up:
         tft.print("upright");
         break;
    case Down:
         tft.print("down");
         break;
    case GoingDown:
         tft.print ("going down");
         break;
    case GoingUp:
         tft.print ("going up");
         break;
    };
#endif

    switch (state) {
    case Up:
      delay(1000);
      break;
    case Down:
      delay(100);
      break;
    case GoingDown:
      SPI.beginTransaction(settings);
      tft.fillScreen(BLACK);
      bmpDraw("q1.bmp",0,0);
      SPI.endTransaction();
      delay(3000);
      break;
    case GoingUp:
      SPI.beginTransaction(settings);
      tft.fillScreen(BLACK);

      tft.setTextColor(WHITE);

      switch (random(10)) {
      case 0:
        tft.setTextSize(4);
        tft.setCursor(16,50);
        tft.printf(F("Yes!"));
        break;
      case 1:
        tft.printf(F("Never!"));
        break;
      case 2:
        tft.printf(F("Too spooky to tell!"));
        break;
      case 3:
        tft.printf(F("The answer is undead."));
        break;
      case 4:
        tft.printf(F("Only the bats know."));
        break;
      case 5:
        tft.printf(F("Nevermore!"));
        break;
      case 6:
        tft.printf(F("Certainly!"));
        break;
      case 7:
        tft.printf(F("Believe it!"));
        break;
      case 8:
        tft.printf(F("Of course!"));
        break;
      case 9:
        tft.printf(F("Perhaps."));
        break;
      }
      SPI.endTransaction();
      delay(5000);
      SPI.beginTransaction(settings);
      bmpDraw("8ball2.bmp",0,0);
      SPI.endTransaction();
      break;
    };
  }

}

/*
8ball1.bmp
8ball2.bmp
Scytheman.bmp
cheshire.bmp
creep1.bmp
creep2.bmp
creep3.bmp
creep4.bmp
creep5.bmp
creep6.bmp
ghost1.bmp
ghost2.bmp
goblin1.bmp
hat.bmp
ninjghst.bmp
pumpkin.bmp
q1.bmp
wizard1.bmp

*/
