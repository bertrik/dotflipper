//  Flipdot
//  Sebastius 2018
//  CC BY-SA 4.0
//
//  https://github.com/sebastius/dotflipper
//  https://revspace.nl/dotflipper
//
//  The Dotflipper is a generic FlipDot controller, for 32x16 panels without their own driver chip.
//  There can be up to 8 panels on the i2c bus for a maximum of 4096 dots of flippin' goodness.
//  Adressing is done per panel, starting top left so:
//
//  0 1    or  0 1 2 3 4 5 6 7   or 0    or 0 1 2   etc...
//  2 3                             1       3 4 5
//  4 5                             2
//  6 7                             3
//
//  Set the adresses on the board using the DIP switch top right.
//  A cheat-sheet is provided on the board.
//  (0 = 000, 1 = 001, 2 = 010, 3 = 011, 4 = 100, 5 = 101, 6 = 110, 7 = 111)
//
//  PCA9685 cheatsheet
//  Datasheet at
//  https://www.nxp.com/products/analog/interfaces/ic-bus/ic-led-controllers/16-channel-12-bit-pwm-fm-plus-ic-bus-led-controller:PCA9685
//
//  Specific for this flipdot controller:
//
//  All adressing is done through I2C
//
//  Addresses 0x40 - 0x5F columndrivers (4 on a board, for 32 colums)
//            0x60 - 0x6F rowdriver (2 on a bord, for 16 rows)
//            0x70        all call
//
//  Note: The PCF9685 does have adress ranges up to 7F but after 0x6F
//        there are some adresses used for other purposes (0x70 ALL_CALL for instance)
//        to make $math easier, i chose not to go above 6F (48 chips distributed over 8 boards)
//        In theory you could add two more boards. Since i only have 5 flipdot panels of this type
//        I won't bother. ;)
//
//  Registers PCA9685:
//  Registers 0x00 - Mode 0 register (see below)
//            0x07 - led 00 on   - data 0x10 ON, 0x00 OFF
//            0x09 - led 00 off  - data 0x10 ON, 0x00 OFF
//            0x43 - led 15 on
//            0x45 - led 15 off
//            0xfb - all leds on
//            0xfd - all leds off
//            Note: there are a few more registers around each LED
//            But if you write to the 0x07 and 0x09 registers (and onward),
//            you can ignore the others.
//
//  Mode 0    0x00 - Normal operation
//            0x80 - Reset
//
//  Theory of operation to flip a dot
//   - Column positive voltage + row gnd = BLACK dot
//   - Column gnd + row positive voltage = YELLOW dot
//
//   To get a black dot:  Column driven by UDN2981 and Row driven by ULN2803
//   To get a yellow dot: Column driven by ULN2803 and Row driven by UDN2981

//#define DEBUG

#include <Wire.h>
//TwoWire Wire2 = TwoWire();

#define width 1         // in number of panels
#define height 2        // in number of panels
#define panelwidth  32  // in dots
#define panelheight 16  // in dots
#define COLS (width*panelwidth)
#define ROWS (height*panelheight)
#define pulsetime 4     // in ms, How long should the pulse last? 
#define offtime 1       // in ms, Delay to to power down

#define nunchuk_lowtres 90
#define nunchuk_hitres 170

#if width==0 || height==0 || width*height > 8
#error "Please check width and height: width and height should both be > 0 and width*height should not be >8!"
#endif

// Framebuffers
boolean current_state[COLS][ROWS];
boolean next_state[COLS][ROWS];

//SNAKE//
boolean dl = false, dr = false, du = false, dd = false; // to check in which direction the snake is currently moving
boolean l, r, u, d, p; // direction 

int snakex[200], snakey[200], slength
int tempx = 10, tempy = 10, xx, yy;
int xegg, yegg;

unsigned long mytime = 280;
unsigned long schedule;

int score = 0, flag = 0;
//END SNAKE//


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

  // Safety
  // If you try to use multiple rows or columns on a panel you will get magic smoke,
  // so we switch everything off on that panel for safety reasons:
  
  // rows
  for (uint8_t address = 0x60 + (panelnumber * 2); address < 0x62 + (panelnumber * 2); address++) {
    everythingoff(address);
  }
  
  // columns
  for (uint8_t address = 0x40 + (panelnumber * 4); address < 0x44 + (panelnumber * 4); address++) {
    everythingoff(address);
  }

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
  everythingoff(0x70); // just making sure
  delay(20);
}

void everythingoff(uint8_t address) {
  // Write to the ALL_LED registers to switch them all off.
  i2cwrite(address, 0xFA, 0x00);
  i2cwrite(address, 0xFB, 0x00);
  i2cwrite(address, 0xFC, 0x00);
  i2cwrite(address, 0xFD, 0x10);
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


void GoLnext() {
  memset(next_state, 0, sizeof(next_state));
  int x;
  int y;

  boolean value;

  for (int r = 0; r < ROWS; r++) { // for each row
    for (int c = 0; c < COLS; c++) { // and each column

      // count how many live neighbors this cell has
      int liveNeighbors = 0;
      for (int i = -1; i < 2; i++) {
        y = r + i;

        // wrap y
        if (y == -1) {
          y = ROWS - 1;
        } else if (y == ROWS) {
          y = 0;
        }

        for (int j = -1; j < 2; j++) {
          if (i != 0 || j != 0) {
            x = c + j;

            // wrap
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

      next_state[c][r] = value;
    }
  }

  for (uint16_t x = 0; x < (COLS / 2); x++) {
    for (uint16_t y = 0; y < (ROWS / 2); y++) {
      if (current_state[x][y] != next_state[x][y]) {
        flipdot(x, y, next_state[x][y]);
      }
      if (current_state[COLS - 1 - x][ROWS - 1 - y] != next_state[COLS - 1 - x][ROWS - 1 - y]) {
        flipdot(COLS - 1 - x, ROWS - 1 - y, next_state[COLS - 1 - x][ROWS - 1 - y]);
      }
      if (current_state[COLS - 1 - x][y] != next_state[COLS - 1 - x][y]) {
        flipdot(COLS - 1 - x, y, next_state[COLS - 1 - x][y]);
      }
      if (current_state[x][ROWS - 1 - y] != next_state[x][ROWS - 1 - y]) {
        flipdot(x, ROWS - 1 - y, next_state[x][ROWS - 1 - y]);
      }
    }
  }
  // discard the old state and keep the new one
  memcpy(current_state, next_state, sizeof next_state);
}

void GoLrandomize() {
  int slider = 90;
  int num;
  boolean value;

  randomSeed(millis());
  memset(current_state, 0, sizeof(current_state));
  memset(next_state, 0, sizeof(next_state));

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


#include <NintendoExtensionCtrl.h>
Nunchuk nchuk;


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

  everythingoff(0x70);
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
  ClearScreen();
  nchuk.begin();
  while (!nchuk.connect()) {
    Serial.println("Nunchuk not detected!");
    delay(1000);
    snakesetup();
  }
}

void snakesetup()
{

  slength = 3;               //Start with snake length 8
  xegg = COLS / 2;
  yegg = ROWS / 2;

  //ClearScreen();

  for (uint8_t i = 0; i <= slength; i++) //Set starting coordinates of snake
  {
    snakex[i] = COLS / 3;
    snakey[i] = ROWS / 3;
  }

  for (uint8_t i = 0; i < slength; i++)  //Draw the snake
  {
    flipdot(snakex[i], snakey[i], HIGH);
  }

  dr = true;  //Going to move right initially
  p = HIGH;
}

void GoL() {
  GoLrandomize();
  for (uint8_t i = 0; i < 100; i++) {
    GoLnext();
    //delay(100);
  }
}

void ClearScreen() {
  for (uint16_t y = 0; y < ROWS; y++) {
    for (uint16_t x = 0; x < COLS; x++) {
      flipdot(x, y, 0);
    }
  }
}

void readjoystick(){
  bool success = nchuk.update();  // Get new data from the controller
  if (!success) {
    Serial.println("Controller disconnected!");
    delay(1000);
  }
  nchuk.printDebug();

  p = HIGH;
  int joyY = nchuk.joyY();
  int joyX = nchuk.joyX();

  if (joyX < nunchuk_lowtres) {
    l = LOW;
    r = HIGH;
    d = HIGH;
    u = HIGH;
  }

  if (joyX > nunchuk_hitres) {
    l = HIGH;
    r = LOW;
    d = HIGH;
    u = HIGH;
  }

  if (joyY < nunchuk_lowtres) {
    d = LOW;
    u = HIGH;
    l = HIGH;
    r = HIGH;
  }
  
  if (joyY > nunchuk_hitres) {
    d = HIGH;
    u = LOW;
    l = HIGH;
    r = HIGH;
  }
}
void movesnake()
{
  



  if (flag == 0)
  {
    snakedirect();    //When key is pressed,this will change the coordinates accordingly and set flag to 1
    //flag would be set to 1 so that direction is not changed multiple times in a short duration
  }

  if (millis() > schedule) //this condition becomes true after every 'time' milliseconds...millis() returns the time since launch of program
  {
    if (flag == 0)                                //flag 0 means no directional key has been pressed in the last 'time' milliseconds
    {
      if (dr == true) {
        tempx = snakex[0] + 1;  // so the snake moves one step in the direction it is moving currently
        tempy = snakey[0];
      }
      if (dl == true) {
        tempx = snakex[0] - 1;  //The new coordinates of head of snake goes in tempx,tempy
        tempy = snakey[0];
      }
      if (du == true) {
        tempy = snakey[0] - 1;
        tempx = snakex[0];
      }
      if (dd == true) {
        tempy = snakey[0] + 1;
        tempx = snakex[0];
      }
    }

    flag = 0;
    checkgame();                              //Check if snake has met egg or coincided with itself
    checkegg();

    if (tempx <= 0) {
      tempx = COLS + tempx; //If the new coordinates are out of screen, set them accordingly
    }
    if (tempx >= COLS) {
      tempx = tempx - COLS;
    }
    if (tempy <= 0) {
      tempy = ROWS + tempy;
    }
    if (tempy >= ROWS) {
      tempy = tempy - ROWS;
    }

    for (uint8_t i = 0; i <= slength; i++) //Change the coordinates of all points of snake
    {
      xx = snakex[i];
      yy = snakey[i];
      snakex[i] = tempx;
      snakey[i] = tempy;
      tempx = xx;
      tempy = yy;
    }

    drawsnake();           //Draw the snake and egg at the new coordinates
    schedule = millis() + mytime;
  }
}


void checkgame()       //Game over checker
{
  for (uint8_t i = 1; i < slength; i++)        //Checking if the coordinates of head have become equal to one of the non head points of snake
  {
    if (snakex[i] == snakex[0] && snakey[i] == snakey[0])
    {
      //insert highscore routine
      ClearScreen();

      slength = 3;            //Resetting the values
      score = 0;
      mytime = 280;

      redrawsnake();              //Restart game by drawing snake with the resetted length and score
    }
  }

}

void checkegg()      //Snake meets egg
{
  if ((snakex[0] == xegg) && (snakey[0] == yegg))
  {
    score += 1;                     //Increase length,score and increase movement speed by decreasing 'time'
    slength += 1;
    if (mytime >= 20)
    {
      mytime -= 20;
    }
    flipdot(xegg, yegg, 0);    //Delete the consumed egg

    xegg = random(1, COLS);           //Create New egg randomly
    yegg = random(1, ROWS);
  }
}


void snakedirect()                  //Check if user pressed any keys and change direction if so
{
  if (l == LOW and dr == false)  //when key LEFT is pressed ,L will become low
  {
    dl = true; du = false; dd = false;
    tempx = snakex[0] - 1;            //Save the new coordinates of head in tempx,tempy
    tempy = snakey[0];
    flag = 1;                   //Do not change direction any further for the ongoing 'time' milliseconds
  }
  else if (r == LOW and dl == false)
  {
    dr = true; du = false; dd = false;
    tempx = snakex[0] + 1;
    tempy = snakey[0];
    flag = 1;
  }
  else if (u == LOW and dd == false)
  {
    du = true; dl = false; dr = false;
    tempy = snakey[0] - 1;
    tempx = snakex[0];
    flag = 1;
  }
  else if (d == LOW and du == false)
  {
    dd = true; dl = false; dr = false;
    tempy = snakey[0] + 1;
    tempx = snakex[0];
    flag = 1;
  }
  else if (p == LOW)            //Pause game for 5 seconds
  {
    ClearScreen();
    for (uint8_t i = 5; i > 0; i--)
    {
      delay(1000);
    }
    redrawsnake();          //Redraw the snake and egg at the same position as it was
  }
}


void drawsnake()        //Draw snake and egg at newly changed positions
{
  flipdot(xegg, yegg, 1);     //Draw egg at new pos
  flipdot(snakex[0], snakey[0], 1);     //Draw new head of snake
  flipdot(snakex[slength], snakey[slength], 0); //Delete old tail of snake
}

void redrawsnake()   //Redraw ALL POINTS of snake and egg
{
  flipdot(xegg, yegg, 1);
  for (uint8_t i = 0; i < slength; i++)
  {
    flipdot(snakex[i], snakey[i], 1);
  }
}

void loop() {
  yield();
  //movesnake();
  //ClearScreen();
  //delay(1000);
  GoL();
}


