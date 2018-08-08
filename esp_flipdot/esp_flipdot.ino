// Flipdot
// Sebastius 2018
// CC BY-SA 4.0
//
//
// PCA9685 cheatsheet
//
// Specific for my flipdot controller:
// Addresses  0x40 - 0x5F columndrivers (4 on a board, for 32 colums)
//            0x60 - 0x6F rowdriver (2 on a bord, for 16 rows)
//            0x70        all call
//
//  Registers PCA9685:
//  Registers 0x00 - Mode 0 register (see below)
//            0x07 - led 00 on   - data 0x10 ON, 0x00 OFF
//            0x09 - led 00 off  - data 0x10 ON, 0x00 OFF
//            0x43 - led 15 on
//            0x45 - led 15 off
//            0xfb - all leds on
//            0xfd - all leds off
//
//  Mode 0    0x00 - Normal operation
//            0x80 - Reset
//
// Theory of operation
// Column high + row low = BLACK dot
// Column low + row high = YELLOW dot

//#define DEBUG

#include <Wire.h>

#define width 1         // in number of panels
#define height 1        // in number of panels
#define panelwidth  32  // in dots
#define panelheight 16  // in dots
#define COLS width*panelwidth
#define ROWS height*panelheight
#define pulsetime 4     // in ms, How long should the pulse last? 
#define offtime 1       // in ms, Delay to to power down

#if width==0 || height==0 || width*height > 8
#error "Please check width and height: width and height should both be > 0 and width*height should not be >8!"
#endif

void flipdot(uint16_t x, uint16_t y, bool color) {
  // This routine seems a bit convoluted, calculating the addresses for each pixel when you could make a map beforehand.
  // However the math takes up < 1ms for 8 panels in total (64x64 sets of calculations) on an 80mhz ESP8266.
  // The actual flipping of the dots is what is slowing us down, at the moment at 5ms per dot.
  // So 64x64 would take approximately 22 seconds for a full flip.
  // One 32x16 panel will need approximately 2.5 seconds (and then some delay afterwards to let the dots settle)
  //
  // Two ideas for improvement:
  // - flipping a dot on all 8 panels
  // - partial refresh (so not changing what doesnt need changing)
  // Both require an actual framebuffer, so thats next for this project.
  //

  // Input sanitation. Out of range? We wrap around.
  // Sanitation is needed because this panel is headed for hacker-events...
  // Stuff will break (actual short circuits, blown chips, coils and diodes) when addressing is wrong.
  x %= width * panelwidth;
  y %= height * panelheight;

  // Quick mafs
  // Find out what panel we're on
  byte xpanelcoordinate = x / panelwidth;
  byte ypanelcoordinate = y / panelheight;
  byte panelnumber = xpanelcoordinate + (ypanelcoordinate * width);

  // Determine which drivers we want to speak to
  byte columndriveraddress = 0x40 + ((panelnumber * 4) + ((x % panelwidth)  / 8));
  byte rowdriveraddress    = 0x60 + ((panelnumber * 2) + ((y % panelheight) / 8));

  // Now we determine what column and row we're on (%8) and determine which register matches
  // If color is HIGH then we need the column to be at positive rail and row at gnd-rail.
  // If color is LOW then we need the row to be at positive rail and column at gnd-rail.
  // This is done in hardware by using the first 8 outputs of the chip for the gnd-rail and the second 8 for the positive rail.

  byte column = x % 8;
  byte columnregisteroffset = (color) ? 0x07 : 0x27;
  byte columnregister = 4 * column + columnregisteroffset;

  byte row = y % 8;
  byte rowregisteroffset = (color) ? 0x27 : 0x07;
  byte rowregister =  4 * row + rowregisteroffset;

  everythingoff(); // just to be safe

  // Now we will flip some bits!
  // Powerup!

  i2cwrite(rowdriveraddress,    rowregister          , 0x10);
  i2cwrite(rowdriveraddress,    rowregister    + 0x02, 0x00);

  i2cwrite(columndriveraddress, columnregister       , 0x10);
  i2cwrite(columndriveraddress, columnregister + 0x02, 0x00);

  delay (pulsetime);

  // And powerdown!
  i2cwrite(rowdriveraddress,    rowregister          , 0x00);
  i2cwrite(rowdriveraddress,    rowregister    + 0x02, 0x10);

  i2cwrite(columndriveraddress, columnregister       , 0x00);
  i2cwrite(columndriveraddress, columnregister + 0x02, 0x10);

  everythingoff(); // just to be more safe

  delay(offtime);
}

void i2cwrite(byte address, byte reg, byte content) {
#ifdef DEBUG
  Serial.print("I2C: ");
  if (address < 0x10) {
    Serial.print("0");
  }
  Serial.print(address, HEX);
  Serial.print(" ");
  if (reg < 0x10) {
    Serial.print("0");
  }
  Serial.print(reg, HEX);
  Serial.print(" ");
  if (content < 0x10) {
    Serial.print("0");
  }
  Serial.println(content, HEX);
#endif
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(content);
  Wire.endTransmission();
}

void resetPCA9685() {
  i2cwrite(0x70, 0x00, 0x80); // Reset all PCA9865s on the bus
  delay(10);
  i2cwrite(0x70, 0x00, 0x00); // Set the Mode0 register to normal operation
  delay(5);
  everythingoff(); // just making sure
  delay(20);
}

void everythingoff() {
  // Write to the all-call address (all PCA9865's on bus) to the ALL_LED registers to switch them all off.
  i2cwrite(0x70, 0xFA, 0x00);
  i2cwrite(0x70, 0xFB, 0x00);
  i2cwrite(0x70, 0xFC, 0x00);
  i2cwrite(0x70, 0xFD, 0x10);
}

void i2cscanner() {
  byte error, address;
  int nDevices;

  Serial.println("Scanning I2C bus...");

  nDevices = 0;
  for (address = 1; address < 127; address++ ) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      Serial.print((address < 0x10) ? "0" : "");
      Serial.println(address, HEX);
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      Serial.print((address < 0x10) ? "0" : "");
      Serial.println(address, HEX);
    }
  }
  Serial.println("I2C scan done\n");
  if (nDevices == 0) {
    Serial.println("No I2C devices found, infinite loop now so you can check everything.");
    infiniteloop();
  } else {
    Serial.print(nDevices, DEC);
    Serial.print(" I2C device");
    Serial.print((nDevices > 1) ? "s" : "");
    Serial.println(" found");
  }
}

void infiniteloop() {
  while (1) {
    yield(); // so the ESP doesnt crash
  }
}

boolean current_state[COLS][ROWS]; // stores the current state of the cells

void GoLnext() {
  int x;
  int y;

  boolean value;
  boolean next[COLS][ROWS]; // stores the next state of the cells

  for (int r = 0; r < ROWS; r++) { // for each row
    for (int c = 0; c < COLS; c++) { // and each column

      // count how many live neighbors this cell has
      int liveNeighbors = 0;
      for (int i = -1; i < 2; i++) {
        y = r + i;
        if (y == -1) {
          y = ROWS - 1;
        } else if (y == ROWS) {
          y = 0;
        }
        for (int j = -1; j < 2; j++) {
          if (i != 0 || j != 0) {
            x = c + j;
            if (x == -1) {
              x = COLS - 1;
            } else if (x == COLS) {
              x = 0;
            }

            if (current_state[x][y]) {
              liveNeighbors++;
            }
          }
        }
      }
      //Serial.print(liveNeighbors);

      // apply the rules
      if (current_state[c][r] && liveNeighbors >= 2 && liveNeighbors <= 3) { // live cells with 2 or 3 neighbors remain alive
        value = true;
      } else if (!current_state[c][r] && liveNeighbors == 3) { // dead cells with 3 neighbors become alive
        value = true;
      } else {
        value = false;
      }

      next[c][r] = value;

      if (current_state[c][r] != next[c][r]) {
        flipdot(c, r, value);
      }

    }
  }

  // discard the old state and keep the new one
  memcpy(current_state, next, sizeof next);
}

void GoLrandomize() {
  int slider = 75;
  int num;
  boolean value;

  randomSeed(millis());

  current_state[1][0] = 1;
  current_state[2][1] = 1;
  current_state[0][2] = 1;
  current_state[1][2] = 1;
  current_state[2][2] = 1;

  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      num = random(1, 100);

      value = num >= slider;
      if (value) {
        current_state[c][r] = value;
        flipdot(c, r, current_state[c][r]);
      }
    }
  }
}





void setup() {

#if defined(__AVR__)
  Wire.begin(); // Nano
  Wire.setClock(400000); // Nano + ESP8266
#elif defined(ESP8266)
  Wire.begin(D2, D1); // ESP8266, SDA, SCL
  Wire.setClock(400000); // ESP8266
#endif

  Serial.begin(115200);
  Serial.println();

  everythingoff();
  Serial.println();
  Serial.println("Flipdot software");

#ifdef DEBUG
  i2cscanner();
  for (uint8_t t = 10; t > 0; t--) {
    Serial.print("Waiting ");
    Serial.print(t);
    Serial.println(" seconds to continue");
    delay(1000);
  }
  Serial.print("Resuming regular code");
#endif

  Serial.println("Flipdot getting ready to go!");

  resetPCA9685();

}

void loop() {
  yield();
  for (uint16_t x = 0; x < (width * panelwidth); x++) {
    for (uint16_t y = 0; y < (height * panelheight); y++) {
      flipdot(x, y, 1);
    }
  }

  for (uint16_t y = 0; y < (height * panelheight); y++) {
    for (uint16_t x = 0; x < (width * panelwidth); x++) {
      flipdot(x, y, 0);
    }
  }

  GoLrandomize();
  for (uint8_t i = 0; i < 100; i++) {
    GoLnext();
  }
}


