// Flipdot
// Sebastius 2018
// CC BY-SA 4.0
// 
//
// PCA9685 cheatsheet
//
// Specific for my flipdot controller:
// Adressen   0x40 - 0x5F columndrivers (4 on a board, for 32 colums)
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

#define DEBUG

#include <Wire.h>

#define width 2         // in number of panels
#define height 4        // in number of panels
#define panelwidth  32
#define panelheight 16
#define pulsetime 4     // How long should the pulse last? 
// I had it at 1ms and it was working on a Nano... Maybe due to bugs so slow. Will try next time.
#define offtime 1       // Delay to allow the i2c expanders to power down


void flipdot(int x, int y, bool color) {
  // This routine seems a bit convoluted, calculating the adresses for each pixel when you could make a map beforehand.
  // However the math takes up < 1ms for 8 panels in total (64x64 sets of calculations) on an 80mhz ESP8266.
  // The actual flipping of the dots is what is slowing us down, at the moment at 5ms per dot.
  // So 64x64 would take approximately 22 seconds for a full flip.
  // One 32x16 panel will need approximately 2.5 seconds (and then some delay afterwards to let the dots settle)
  //
  // Two ideas for improvement:
  // - flipping a dot on all 8 panels
  // - partial refresh (so not changing what doesnt need changing)
  // Both require an actual framebuffer, so thats next for this project.

  // Input sanitation. Out of range? We wrap around.
  // Sanitation is needed because this panel is headed for hacker-events...
  // Stuff will break (actual short circuits, blown chips, coils and diodes) when adressing is wrong.
  x %= width * panelwidth;
  y %= height * panelheight;

  // Quick mafs
  // Find out what panel we're on
  byte xpanelcoordinate = x / panelwidth;
  byte ypanelcoordinate = y / panelheight;
  byte panelnumber = xpanelcoordinate + (ypanelcoordinate * width);

  // Determine which drivers we want to speak to
  byte columndriveradress = 0x40 + ((panelnumber * 4) + ((x % panelwidth)  / 8));
  byte rowdriveradress    = 0x60 + ((panelnumber * 2) + ((y % panelheight) / 8));

  // Now we determine what column and row we're on (%8) and determine which register matches
  // If color is HIGH then we need the column to be at positive rail and row at gnd-rail.
  // If color is LOW then we need the row to be at positive rail and column at gnd-rail.
  // This is done in hardware by using the first 8 outputs of the chip for the gnd-rail and the second 8 for the positive rail.
  byte columnregisteroffset = 0x07;            // register for LED0_High register
  byte column = x % 8;
  if (! color) columnregisteroffset = 0x27; // register for LED8_High register
  byte columnregister = 4 * column + columnregisteroffset;

  byte rowregisteroffset = 0x07;
  byte row = x % 8;
  if (color) rowregisteroffset = 0x27;
  byte rowregister =  4 * row + rowregisteroffset;

  // Now we will flip some bits!
  // Powerup!
  i2cwrite(rowdriveradress,    rowregister          , 0x10);
  i2cwrite(rowdriveradress,    rowregister    + 0x02, 0x00);

  i2cwrite(columndriveradress, columnregister       , 0x10);
  i2cwrite(columndriveradress, columnregister + 0x02, 0x00);

  delay (pulsetime);

  // And powerdown!
  i2cwrite(rowdriveradress,    rowregister          , 0x00);
  i2cwrite(rowdriveradress,    rowregister    + 0x02, 0x10);

  i2cwrite(columndriveradress, columnregister       , 0x00);
  i2cwrite(columndriveradress, columnregister + 0x02, 0x10);

  delay(offtime);
}

void i2cwrite(byte adress, byte reg, byte content) {

#ifdef DEBUG
  Serial.print("I2C: "); Serial.print(adress, HEX); Serial.print(" "); Serial.print(reg, HEX); Serial.print(" "); Serial.println(content, HEX);
#endif

  Wire.beginTransmission(adress);
  Wire.write(reg);
  Wire.write(content);
  Wire.endTransmission();
}

void resetPCA9685() {
  i2cwrite(0x70, 0x00, 0x80); // Reset all PCA9865s on the bus
  delay(10);
  i2cwrite(0x70, 0x00, 0x00); // Set the Mode0 register to normal operation
  delay(5);
}

void setup() {
  Serial.begin(115200);
  Serial.println(); Serial.println();
  Serial.println("Flipdot");

  Serial.println(ESP.getFlashChipRealSize());

  //Wire.begin(); // Nano
  Wire.begin(D1, D2); // ESP8266, SDA, SCL
  //Wire.setClock(1000000); // ESP8266, weet nog niet of bekabeling het aan kan en of het zin heeft...
  Wire.setClock(400000); // Nano + ESP8266

  resetPCA9685();
  flipdot(0, 0, 1);
}



void loop() {
  yield();
}


