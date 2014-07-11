/*
 Clock code by Josh Lucy <joshATlucyindustrialDAWTcom>.
 
 This work is licensed under a Creative Commons Attribution 4.0
 International License.
 This code is designed for an Adafruit Flora using a ChronoDot 2.1 RTC,
 Adafruit CAP1188 touch sensor, and Adafruit NeoPixels, but is set up to
 be prototyped and debugged on an Arduino Uno.
 
 This project's source can be found on GitHub at https://github.com/LucyIndustrial/EzClock/
 */
 
/* 
  To build the code for a test envrionment you can uncomment the
  #define DEBUGON line, and if you're testing against an Arduino
  Uno, you can comment out the #define USINGFLORA line.
*/
 
/************
 * INCLUDES *
 ************/
#include <EEPROM.h> // Onboard EEPROM storage of both time zones.
#include <Wire.h> // I2C support for RTC and cap touch sensor
#include <SPI.h> // Required for CAP1188 touch sensor, but not used.
#include <RTC_DS3231.h> // Load a lib for our specific RTC chip.
#include <RTClib.h> // Load the abstraction lib for our RTC.
#include <Adafruit_CAP1188.h> // Touch sensor support
#include <Adafruit_NeoPixel.h> // NeoPixel support


/***********
 * DEFINES *
 ***********/

// To debug, or not debug? That is the question.
#define DEBUGON

// Are we compiling for a Flora or the test UNO?
// Comment out the following #define for an UNO.
//#define USINGFLORA

// Touch interface config
#define T_KEY1        0x01
#define T_KEY2        0x02
#define T_KEY3        0x04
#define T_KEY4        0x08
#define T_KEY5        0x10
#define T_KEY6        0x20
#define T_KEY7        0x40
#define T_KEY8        0x80

// Touch sensor IRQ config
#ifdef USINGFLORA
  #define T_IRQPIN    0 // The Flora uses the "RX" pin for the IRQ
  #define T_IRQ       2 // Which is IRQ #2
#else
  #define T_IRQPIN    2 // The Uno uses D2 for the IRQ
  #define T_IRQ       0 // Which is IRQ #0
#endif

// ChronoDot 2.0/DS3231 RTC config
#define RTC_SQW_FREQ  DS3231_SQW_FREQ_1

// ChronoDot RTC SQW/IRQ pin
#ifdef USINGFLORA
  #define R_IRQPIN    1 // The Flora uses the "TX" pin for the IRQ
  #define R_IRQ       3 // Which is IRQ #3
#else
  #define R_IRQPIN    3 // The Uno uses D3 for the IRQ
  #define R_IRQ       1 // Which is IRQ #1
#endif

// Turn this LED off after boot.
#ifdef USINGFLORA
  #define INDLED      7  // On-board Flora LED PIN
#else
  #define INDLED      13 // On-board Arduino Uno LED PIN
#endif


// Clock face config - Part of this assumes pixels are hooked up like hr -> min -> sec
#define F_LENGTH      37 // How many NeoPixels do we have in the face?

#define F_DEFAULT_R   0 // The "background" color for the clock face.
#define F_DEFAULT_G   0
#define F_DEFAULT_B   0

// Clock face hour marks
#define F_HR_MARK_R   135 // Tick mark RGB values.
#define F_HR_MARK_G   90
#define F_HR_MARK_B   0

// Configure hours display stuff.
#define F_HRSTART     0  // Which pixel does the hour start at?
#define F_HRLEN       24 // The hour is 24 pixels long.
#define F_HR_R        0   // RGB values for the hour
#define F_HR_G        90
#define F_HR_B        90
#define F_HR_TRLEN    6   // Add a 6-pixel trail behind the current hour pixel.

// Configure mintues display stuff.
#define F_MINSTART    24 // Which pixel does the minute start at?
#define F_MINLEN      12 // The minute is 12 pixels long.
#define F_MIN_R       40   // RGB values for the minutes
#define F_MIN_G       0
#define F_MIN_B       0

// Display seconds display stuff.
#define F_SECSTART    24 // Which pixel is for seconds?
#define F_SECLEN      12 // The seconds is 1 pixel long
#define F_SEC_R       0   // RGB values for the seconds
#define F_SEC_G       40
#define F_SEC_B       0

// This config is for the seconds "pulse" LED/pixel.
#define F_SECLSTART   36 // Where is our center second LED
#define F_SECL_DELAY  10 // How long do we wait before dimming again?
#define F_SECL_STEP   10 // How many dimming steps do we have?
#define F_SECL_TZA_R  100 // Second pixel RGB values for time zone A
#define F_SECL_TZA_G  0
#define F_SECL_TZA_B  50
#define F_SECL_TZB_R  100 // Second pixel RGB values for time zone B
#define F_SECL_TZB_G  60
#define F_SECL_TZB_B  0

// Settings for time zone configuration display/animation
#define F_TZSET_ZRO_R 90 // RGB settings for the time zone setting screen's zero line.
#define F_TZSET_ZRO_G 90
#define F_TZSET_ZRO_B 0
#define F_TZSET_PLS_R 0 // RGB settings for hours and minutes in the GMT + region.
#define F_TZSET_PLS_G 0
#define F_TZSET_PLS_B 90
#define F_TZSET_MNS_R 90 // RGB settings for hours and minutes in the GMT - reigon.
#define F_TZSET_MNS_G 0
#define F_TZSET_MNS_B 0
#define F_TZSET_MIN_I 30 // Increment in minutes that we set - default is 30 mins.

// Light feature settings
#define F_LIGHT_A     85 // When the clock face is used as a light, this is the first brightness setting.
#define F_LIGHT_B     169 // Second brightness setting
#define F_LIGHT_C     255 // Last brightness setting
#define F_LIGHT_DLY   100 // Light mode animation delay for start/stop.

// NeoPixel clock face data pin
#ifdef USINGFLORA
  #define F_DATAPIN  6
#else
  #define F_DATAPIN  4
#endif

// EEPROM timezone data storage addresses
#define TZ_A_ID      0 // GMT offset/timezone A's identifier
#define TZ_B_ID      1 // GMT offset/timezone B's identifier
#define EEP_TZ_HR_A  0x00 // GMT offset/timezone A, offset in hours EEPROM address, 2 bytes.
#define EEP_TZ_MIN_A 0x02 // GMT offset/timezone A, offset in minutes EEPROM address, 2 bytes.
#define EEP_TZ_HR_B  0x04 // GMT offset/timezone B, offset in hours EEPROM address, 2 bytes.
#define EEP_TZ_MIN_B 0x06 // GMT offset/timezone B, offset in minutes EEPROM address, 2 bytes.

/***********
 * OBJECTS *
 ***********/

// Create touch sensor object.
Adafruit_CAP1188 cap = Adafruit_CAP1188();
// Set up the RGB LEDs that make the clock face up.
Adafruit_NeoPixel face = Adafruit_NeoPixel(F_LENGTH, F_DATAPIN, NEO_GRB + NEO_KHZ800);
// Set up or RTC.
RTC_DS3231 rtc;

/***************
 * GLOBAL VARS *
 ***************/

// Touch IRQ flag
volatile boolean t_IrqFlag = 0;
// RTC IRQ flag
volatile boolean r_IrqFlag = 0;

// Light mode step tracker
int currentStep = 0;

// Adafruit NeoPixel gamma table, from the Adafruit goggles example.
const uint8_t gamma[] = {
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,
  3,   3,   4,   4,   4,   4,   5,   5,   5,   5,   5,   6,   6,   6,   6,   7,
  7,   7,   8,   8,   8,   9,   9,   9,   10,  10,  10,  11,  11,  11,  12,  12,
  13,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,  20,
  20,  21,  21,  22,  22,  23,  24,  24,  25,  25,  26,  27,  27,  28,  29,  29,
  30,  31,  31,  32,  33,  34,  34,  35,  36,  37,  38,  38,  39,  40,  41,  42,
  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,
  58,  59,  60,  61,  62,  63,  64,  65,  66,  68,  69,  70,  71,  72,  73,  75,
  76,  77,  78,  80,  81,  82,  84,  85,  86,  88,  89,  90,  92,  93,  94,  96,
  97,  99,  100, 102, 103, 105, 106, 108, 109, 111, 112, 114, 115, 117, 119, 120,
  122 ,124 ,125, 127, 129, 130, 132, 134, 136, 137, 139, 141, 143, 145, 146, 148,
  150 ,152 ,154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180,
  182 ,184 ,186, 188, 191, 193, 195, 197, 199, 202, 204, 206, 209, 211, 213, 215,
  218 ,220 ,223, 225, 227, 230, 232, 235, 237, 240, 242, 245, 247, 250, 252, 255
};

//
// Clock face color scheme stuff...
//

// RGB color we want hr marks to have, gamma corrected.
uint32_t hrMarkColor = face.Color(gamma[F_HR_MARK_R], gamma[F_HR_MARK_G], gamma[F_HR_MARK_B]);

// "Default" face color:
uint32_t faceDefColor = face.Color(gamma[F_DEFAULT_R], gamma[F_DEFAULT_G], gamma[F_DEFAULT_B]);

// Global timekeeping
int r_hr; // Global hour value from RTC. (Shoudld be in GMT / UTC)
int r_min; // Global minute value from RTC. (Shoudld be in GMT / UTC)
int r_sec; // Global second value from RTC. (Shoudld be in GMT / UTC)

// Offset-compensated time - use r_sec for the current # of seconds since that doesn't change.
int currentHr = -13;
int currentMin = -60;

int tz_a[] = {-12, -60}; // Time zone/GMT offset A global value (use a bogus value)
int tz_b[] = {-13, -61}; // Time zone/GMT offset B global value (use a bogus value)
int currentTz[] = {-100, -100}; // This will store the current time zone (use a bogus value)
int currentTzID = TZ_A_ID;

int f_secl_r = F_SECL_TZA_R; // Set the default second RGB values to reflect that we're using time zone A at startup.
int f_secl_g = F_SECL_TZA_G;
int f_secl_b = F_SECL_TZA_B;


/*******************************
 * ARDUINO SETUP AND MAIN LOOP *
 *******************************/

// System setup method
void setup() {
  // Set up our diagnostic indicator LED
  pinMode(INDLED, OUTPUT);

  // Turn on the onboard LED to show we're setting up.
  digitalWrite(INDLED, HIGH);

  #ifdef DEBUGON
    // Configure serial internface
    Serial.begin(115200);
    Serial.println("In setup()...");
  #endif

  #ifdef DEBUGON
    Serial.println("Init I2C...");
  #endif

  // Start our I2C bus.
  Wire.begin();

  #ifdef DEBUGON
    Serial.println("Init RTC...");
  #endif

  // Spin the RTC up.
  rtc.begin();

  // Check our clock to see if it's running.
  // in case the RTC ran out of battery or reset
  // we don't want to stop the startup process.
  // Since this has no consequence unless we're
  // debugging only do this check when debugging.
  #ifdef DEBUGON
    if (!rtc.isrunning()) {
      Serial.println("RTC not running...");
    } else {
      Serial.println("RTC running...");
    }
  #endif

  #ifdef DEBUGON
    // Set up the touch sensor
    Serial.println("Init CAP1188...");
  #endif

  // Did we start the CAP1188
  if (!cap.begin()) {
    #ifdef DEBUGON
      Serial.println("CAP1188 not found.");
    #endif
    // No cap sensor, just spin.
    while (1);
  }

  // Configure our pins, except INDLED
  pinMode(T_IRQPIN, INPUT);
  pinMode(R_IRQPIN, INPUT);
  pinMode(F_DATAPIN, OUTPUT);

  // Debug
  #ifdef DEBUGON
    Serial.println("CAP1188 found!");
  #endif

  // Configure touch sensor.
  configTouch();
  // Configure realtime clock.
  configRTC();
  // Configure time zones.
  configTZ();

  // Set our interrupts.
  attachInterrupt(T_IRQ, setTIrqFlag, FALLING); // Touch
  attachInterrupt(R_IRQ, setRIrqFlag, FALLING); // RTC

  //Start the clock face up, and it will be zeroed out.
  face.begin();
  face.show();

  #ifdef DEBUGON
    Serial.println("Leaving setup()...");
  #endif

  // Turn off the onboard LED to show we're done setting up.
  digitalWrite(INDLED, LOW);
}

// Main program loop
void loop() {
  // Check to see if we got an IRQ from the RTC.
  if (checkRIRQ()) {
    #ifdef DEBUGON
      Serial.println("Got RTC IRQ.");
    #endif
        
    // Clear the IRQ flag since we're handling it now.
    clearRIRQ();
   
    // Get the new time value from the RTC.
    getRTCTime();

    // Set our global "current time" variable now that we have
    // accounted for the GMT offset.
    setCurrentTime();
        
    // Show our clock face since we have new time data!
    showClockFace();
  }

  // Do we have a touch IRQ?
  if (checkTIRQ()) {
    // Debug
    #ifdef DEBUGON
      Serial.println("Got touch IRQ in loop().");
    #endif
    
    // Grab the state of the touched keys before clearing the IRQ.
    uint8_t touched = getTouched();

    // Clear the touch IRQ since we're handling it now.
    clearTIRQ();

    // Handle the keys touched by the user.
    handleMain(touched);
  }
}


/*****************************
 * "MENU" HANDLING FUNCTIONS *
 *****************************/

// Let's do the main menu thing.
void handleMain(int t_touched) {
  #ifdef DEBUGON
    Serial.println("Handling touched key in handleMain().");
  #endif
  
  // What was touched?
  switch(t_touched) {
    // Key 1 pressed from normal clock mode.
    case T_KEY1:
      // Display time zone #1
      setCurrentTZ(TZ_A_ID);
      break;
      
    // Display time zone #2
    case T_KEY2:
      setCurrentTZ(TZ_B_ID);
      break;

    // Key 3 pressed from normal clock mode.
    case T_KEY3:
      // Activate light mode.
      handleLight();
      break;

    // Set offset for time zone ID 0.
    case T_KEY6:
      handleSetTz(TZ_A_ID);
      break;

    // Set offset for time zone ID 1.
    case T_KEY7:
     handleSetTz(TZ_B_ID);
     break;

    // Set the system UTC time.
    case T_KEY8:
      // Change the UTC time.
      break;

    // If anything else came through we don't care.
    default:
      // Do nothing.
      break;
  }
  
  return;
}

// If we're trying to turn the light on...
void handleLight() {
  #ifdef DEBUGON
    Serial.println("Handling the LED light.");
  #endif

  // Define our steps based on the #define statements "at the top".
  int lightSteps[] = {F_LIGHT_A, F_LIGHT_B, F_LIGHT_C};

  // Start-up animation
  setFaceRange(0, F_LENGTH -1, F_DEFAULT_R, F_DEFAULT_G, F_DEFAULT_B);
  face.show();
  delay(F_LIGHT_DLY);
  face.setPixelColor(F_SECLSTART, gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]]);
  face.show();
  delay(F_LIGHT_DLY);
  setFaceRange(F_MINSTART, (F_MINSTART + F_MINLEN) - 1, gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]]);
  face.show();
  delay(F_LIGHT_DLY);
  setFaceRange(F_HRSTART, F_HRLEN - 1, gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]]);
  face.show();
  delay(F_LIGHT_DLY);

  // Go until we've looped through all possible values.
  while(currentStep <= 2) {
    // If the IRQ
    if (checkTIRQ()) {
      // Clear the touch IRQ since we're handling it now.
      clearTIRQ();
      
      #ifdef DEBUGON
        Serial.println("Light step up.");
      #endif
    
      // Clear the IRQ because we're handling it.
      t_IrqFlag = 0;

      // Set the light step, 0 for off, 2 for brightest.
      currentStep++;

      // Set the step.
      setFaceRange(0, F_LENGTH - 1, gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]]);
      face.show();
    }
  }

  // Wind-down animation
  face.setPixelColor(F_SECLSTART, F_DEFAULT_R, F_DEFAULT_G, F_DEFAULT_B);
  face.show();
  delay(F_LIGHT_DLY);
  setFaceRange(F_MINSTART, (F_MINSTART + F_MINLEN) - 1, F_DEFAULT_R, F_DEFAULT_G, F_DEFAULT_B);
  face.show();
  delay(F_LIGHT_DLY);
  setFaceRange(F_HRSTART, F_HRLEN - 1, F_DEFAULT_R, F_DEFAULT_G, F_DEFAULT_B);
  face.show();
  delay(F_LIGHT_DLY);

  // Reset our counter.
  currentStep = 0;

  #ifdef DEBUGON
    Serial.println("Done lighting.");
  #endif
  
  return;
}

// Set the specified timezone.
void handleSetTz(int t_targetTz) {
  #ifdef DEBUGON
    Serial.print("User configuring timezone ID: ");
    Serial.println(t_targetTz);
  #endif
  
  // Track the timezone for the given ID we started with in case
  // it doesn't chage so we don't have to waste a r/w cycle on the EEPROM.
  int beginningTzHr;
  int beginningTzMin;
  int thisTzR; // Hold the value for the center LED to indidcate the TZ we're setting.
  int thisTzG;
  int thisTzB;
  int thisHr; // Hold the hour we want to store as TZ minute
  int thisMin; // Hold the minute we want to store as TZ minute
  boolean setting = true; // Are we still setting the TZ offset?
  int8_t displayDir; // Display TZ in CW or CCW direction? + = CW, - = CCW
  uint8_t displayStartH; // Where do we start drawing the hour/minute marks?
  uint8_t displayStartM;
  uint32_t displayColor; // Hold the color we'll use depending on displayDir.
  
  // Which target timezone are we after, and what color should the indidcator LED be?
  switch(t_targetTz) {
    case TZ_A_ID:
      thisTzR = F_SECL_TZA_R;
      thisTzG = F_SECL_TZA_G;
      thisTzB = F_SECL_TZA_B;
      thisHr = tz_a[0];
      thisMin = tz_a[1];
      beginningTzHr = tz_a[0];
      beginningTzMin = tz_a[1];
      break;
    case TZ_B_ID:
      thisTzR = F_SECL_TZB_R;
      thisTzG = F_SECL_TZB_G;
      thisTzB = F_SECL_TZB_B;
      thisHr = tz_b[0];
      thisMin = tz_b[1];
      beginningTzHr = tz_b[0];
      beginningTzMin = tz_b[1];
      break;
    default:
      // BAIL!
      return;
      break;
  }
  
  // While we're setiting the time zone keep going.
  while(setting) {
    // Did we hit a button of some kind?
    if(checkTIRQ()) {
      // Grab the state of the touched keys before clearing the IRQ.
      uint8_t touched = getTouched();
      // Clear the touch IRQ since we're handling it now.
      clearTIRQ();
      
      // Determine which key we just pressed, and what to do.
      switch(touched) {
        case T_KEY1:
          // Subtract hour
          thisHr--;
          break;
        case T_KEY2:
          // Add hour
          thisHr++;
          break;
        case T_KEY3:
          // Subtract minute at increment specified by F_TZSET_MIN_I
          thisMin -= F_TZSET_MIN_I;
          break;
        case T_KEY4:
          // Add minute at increment specified by F_TZSET_MIN_I
          thisMin += F_TZSET_MIN_I;
          break;
        case T_KEY7:
          // Cancel
          // Break out of the loop, don't save time zone to EEPROM.
          setting = false;
          break;
        case T_KEY8:
          // Save time zone to EEPROM and break out of the loop.
          setting = false;
          // If the time zone actually changed write it to EEPROM.
          if (thisHr != beginningTzHr || thisMin != beginningTzMin) {
            // Save the time zone.
            saveTZ(t_targetTz, thisHr, thisMin);
            // Reconfigure time zones.
            configTZ();
          }
          break;
        default:
          // Don't care about anything else pressed.
          break;
        
      }
    }
    
    // Boundary check the time zone data the user set
    // This will "loop" the time as it crosses a positive or negative
    // boundary back to zero.
    if(thisHr < -11 || thisHr > 11) {
      thisHr = 0;
    }
    
    if(thisMin < -59 || thisMin > 59) {
      thisMin = 0;
    }
    
    // Also make sure the hour and minute have the same "sign".
    // Take the hour's sign if a conflict occurs.
    if(thisHr * thisMin < 0) {
      thisMin *= -1;
    }

    // Determine which color to use, and what direction to draw the hour and minute
    // offsets.

    // Determine which direction we're moving.
    if(thisHr >= 0) {
      displayDir = 1;
      displayColor = face.Color(F_TZSET_PLS_R, F_TZSET_PLS_G, F_TZSET_PLS_B);
      displayStartH = F_HRSTART;
      displayStartM = F_MINSTART;
    } else {
      displayDir = -1;
      displayColor = face.Color(F_TZSET_MNS_R, F_TZSET_MNS_G, F_TZSET_MNS_B);
      displayStartH = F_HRSTART + F_HRLEN;
      displayStartM = F_MINSTART + F_MINLEN;
    }
    
    // First blank the clock face.
    setFaceRange(0, F_LENGTH -1, F_DEFAULT_R, F_DEFAULT_G, F_DEFAULT_B);
    // Draw the "zero line" for hours.
    face.setPixelColor(F_HRSTART, F_TZSET_ZRO_R, F_TZSET_ZRO_G, F_TZSET_ZRO_B);
    // Do the same for minutes.
    face.setPixelColor(F_MINSTART, F_TZSET_ZRO_R, F_TZSET_ZRO_G, F_TZSET_ZRO_B);
    // Set the seconds indicator LED to show which time zone we're setting.
    face.setPixelColor(F_SECLSTART, thisTzR, thisTzG, thisTzB);
    
    // Show the time zone offset hour as a single pixel, assuming it's > 0.
    if (thisHr != 0) {
      face.setPixelColor(displayStartH + (thisHr), displayColor);
    }
    
    // Show the time zone offset.
    face.show();
  }
    
  #ifdef DEBUGON
    Serial.print("Timezone ID ");
    Serial.print(t_targetTz);
    Serial.print(" is now GMT ");
    Serial.print(thisHr);
    Serial.print(":");
    Serial.println(thisMin);
  #endif
  
  return;
}


/************************
 * CLOCK FACE FUNCTIONS *
 ************************/
// Show the clock face.
void showClockFace() {
  // Seconds will be an interrupt-driven animation
  // so they don't show up here.
  
  #ifdef DEBUGON
    Serial.println("Updating clock face.");
  #endif
  
  // Blank the clock.
  setFaceColor(faceDefColor);
  
  // Display hour marks.
  showFaceHrMarks(hrMarkColor);
  // Display our hours
  showFaceHr(currentHr);
  // Show our minutes
  showFaceMin(currentMin);
  // Show our seconds.
  showFaceSec(r_sec);
  
  // Draw the clock face since we have all the relevant stuff ready.
  drawClockFace();
  
  // And dow the seconds animation thing
  showSecFlash();
  
  return;
}

// Draw the clock face.
void drawClockFace() {
  // Draw the face.
  face.show();

  return;
}

// Show hour ticks at 0:00, 6:00, 12:00, and 18:00 hour marks
void showFaceHrMarks(uint32_t t_color) {
  #ifdef DEBUGON
    Serial.println("Create 6-hour marks.");
  #endif
  face.setPixelColor(0, t_color);
  face.setPixelColor(6, t_color);
  face.setPixelColor(12, t_color);
  face.setPixelColor(18, t_color);
  
  return;
}

// Show the hour..
void showFaceHr(int t_hr) {
  // Store the target pixel for the hour trail.
  int x_tgt;
  
  #ifdef DEBUGON
    Serial.println("Create hour marks.");
  #endif
 
  // Figure out how to add a cool trail to the hour.
  //face.setPixelColor(t_hr, face.Color(gamma[F_HR_R], gamma[F_HR_G], gamma[F_HR_B]) | face.getPixelColor(t_hr));

  // Draw the trail.
  for (int i = 0; i < F_HR_TRLEN; i++) {
   
    // Scale the pixels brightness.
    int scaledR = map(i, F_HR_TRLEN, 0, F_DEFAULT_R, F_HR_R);
    int scaledG = map(i, F_HR_TRLEN, 0, F_DEFAULT_G, F_HR_G);
    int scaledB = map(i, F_HR_TRLEN, 0, F_DEFAULT_B, F_HR_B);

    // Get our target pixel
    x_tgt = (F_HRSTART + t_hr) - i;

    // Quick boundary check to make sure the pixel doesn't
    // fall of the hour track.
    if (x_tgt < F_HRSTART) {
      // Loop the tail back to the end of the hours track.
      x_tgt = (F_HRSTART + F_HRLEN) + (F_HRSTART + t_hr - i);
    }
   
    // Set our target pixel.
    face.setPixelColor(x_tgt, face.Color(gamma[scaledR], gamma[scaledG], gamma[scaledB]) | face.getPixelColor(x_tgt));
  }
  
  return;
}

// Zero out the clock's face so we can start over, but don't waste
// cycles showing it.
void setFaceColor(uint32_t t_color) {
  #ifdef DEBUGON
    Serial.print("Setting clock face to color ");
    Serial.println(t_color, HEX);
  #endif
  
  // Set each element of the face to a specific color.
  for(int i = 0; i < F_LENGTH; i++) {
    face.setPixelColor(i, t_color);
  }
  
  return;
}

// Set the minute pixels.
void showFaceMin(int t_min) {

  // We want 5 dimming stages using the minute mask value
  // determined by min % 5.

  #ifdef DEBUGON
    Serial.println("Create minute marks.");
  #endif

  // Set the minute LED "cursor".
  int x_minPix = t_min / 5;
  // Figure out how bright to make the
  // LED the cursor points to.
  int brightIndex = t_min % 5;
 
  int scaledR = map(brightIndex, 0, 4, F_DEFAULT_R, F_MIN_R);
  int scaledG = map(brightIndex, 0, 4, F_DEFAULT_G, F_MIN_G);
  int scaledB = map(brightIndex, 0, 4, F_DEFAULT_B, F_MIN_B);

  // Set our color mask based
  uint32_t cursorColor = face.Color(scaledR, scaledG, scaledB);


  // Set "current" minute pixel
  face.setPixelColor(F_MINSTART + x_minPix, cursorColor | face.getPixelColor(F_MINSTART + x_minPix));


  // Set previous minute pixels for fill-in effect.
  for (int i = x_minPix - 1; i >= 0; i--) {
    face.setPixelColor(i + F_MINSTART, face.Color(F_MIN_R, F_MIN_G, F_MIN_B) | face.getPixelColor(i + F_MINSTART));

  }
  
  return;
}

// Set the hour pixel.
void showFaceSec(int t_sec) {

  #ifdef DEBUGON
    Serial.println("Create second marks.");
  #endif
  
  // Set the second LED "cursor".
  int x_secPix = t_sec / 5;
  // Figure out how bright to make the
  // LED the cursor points to.
  int brightIndex = t_sec % 5;
 
  int scaledR = map(brightIndex, 0, 4, F_DEFAULT_R, F_SEC_R);
  int scaledG = map(brightIndex, 0, 4, F_DEFAULT_G, F_SEC_G);
  int scaledB = map(brightIndex, 0, 4, F_DEFAULT_B, F_SEC_B);

  // Set our color mask based
  uint32_t cursorColor = face.Color(scaledR, scaledG, scaledB);

  // Set "current" minute pixel
  face.setPixelColor(F_SECSTART + x_secPix, cursorColor ^ face.getPixelColor(F_SECSTART + x_secPix));

  // Set previous minute pixels for fill-in effect.
  for (int i = x_secPix - 1; i >= 0; i--) {
    face.setPixelColor(i + F_SECSTART, face.Color(F_SEC_R, F_SEC_G, F_SEC_B) | face.getPixelColor(i + F_SECSTART));

  }
  
  return;
}

// Show a little flash when we register a new second.
void showSecFlash() {
  // Hold our scaled values here.
  int scaledR;
  int scaledG;
  int scaledB;
  
  #ifdef DEBUGON
    Serial.println("Doing seconds blink effect.");
  #endif
  
  // Light and fade to black.
  for (int i = F_SECL_STEP; i >= 0; i--) {
    // Use map() to dim LED in a porportional fasion.
    scaledR = map(i, 0, F_SECL_STEP, F_DEFAULT_R, f_secl_r);
    scaledG = map(i, 0, F_SECL_STEP, F_DEFAULT_G, f_secl_g);
    scaledB = map(i, 0, F_SECL_STEP, F_DEFAULT_B, f_secl_b);
    
    // Write the color.
    face.setPixelColor(F_SECLSTART, face.Color(gamma[scaledR], gamma[scaledG], gamma[scaledB]));
    
    // And draw the clock face since we're in the loop still.
    drawClockFace();
    
    // Since we're in this animation for a significant amount of time
    // touch commands may not register, so we should make sure we're dealing
    // with them.
    if(checkTIRQ()) {
      #ifdef DEBUGON
        Serial.println("Got touch IRQ in showSecFlash().");
      #endif
      
      // Grab the state of the touched keys before clearing the IRQ.
      uint8_t touched = getTouched();
            
      // Clear the touch IRQ since we're handling the touch event.
      clearTIRQ();
      
      // Do our main menu thing.
      handleMain(touched);
      
      // If we've detected another second passing by since
      // going to the main menu break out of the animation
      // to have it start over after the clock face is updated.
      if(checkRIRQ()) {
        break;
      }
    }

    // Wait a bit.
    delay(F_SECL_DELAY);
  }

  return;
}

// Set a range of pixels
void setFaceRange(int t_first, int t_last, int t_r, int t_g, int t_b) {
  #ifdef DEBUGON
    Serial.print("Setting pixels ");
    Serial.print(t_first, DEC);
    Serial.print(" - ");
    Serial.println(t_last, DEC);
  #endif
  
  // Set the pixels.
  for (int i = t_first; i <= t_last; i++) {
    // Assign the current pixel the color.
    face.setPixelColor(i, t_r, t_g, t_b);
  }
  
  return;
}


/***************************
 * HARDWARE COMM FUNCTIONS *
 ***************************/

// Update the time from the RTC.
void getRTCTime() {

  #ifdef DEBUGON
    Serial.println("Getting RTC time...");
  #endif

  DateTime now = rtc.now();

  // We have to convert these bytes from BCD to an integer.
  r_sec = now.second(); // First byte is seconds in BCD format.
  r_min = now.minute(); // Second byte is minutes in BCD format.
  r_hr = now.hour(); // Last byte is hours in BDC format.

  #ifdef DEBUGON
    Serial.print("Got RTC time: ");
    Serial.print(r_hr);
    Serial.print(":");
    Serial.print(r_min);
    Serial.print(":");
    Serial.println(r_sec);
  #endif
  
  return;
}

// Get out touched keys.
uint8_t getTouched() {
  // Grab the touched keys from the sensor.
  uint8_t touched = cap.touched();

  #ifdef DEBUGON
    Serial.print("Got touched keys: ");
    Serial.println(touched, HEX);
  #endif
  
  
  // Return the keys touched.
  return touched;
}

// Get the stored values in EEPROM for time zone offset
void loadTZ(int t_tzID, int t_tzDatArray[]) {
  // Create our storage variables.
  int h_addr;
  int m_addr;
  // Account for signed ints by using 2 bytes, not 1.
  byte hiByte;
  byte loByte;

  // Figure out which time zone we're requesting.
  switch(t_tzID) {
    case 0:
      // This is the default time zone displayed.
      h_addr = EEP_TZ_HR_A;
      m_addr = EEP_TZ_MIN_A;
      break;
    case 1:
      h_addr = EEP_TZ_HR_B;
      m_addr = EEP_TZ_MIN_B;
      break;
    default:
      // The default behavior is to just read the first address set.
      h_addr = EEP_TZ_HR_A;
      m_addr = EEP_TZ_MIN_A;
      t_tzID = 0;
      break;
  }
  
  // Read time zone data at the specified addresses, accouting for 2's compliment bytes.
  // See: http://forum.arduino.cc/index.php/topic,37470.0.html
  loByte = EEPROM.read(h_addr);
  hiByte = EEPROM.read(h_addr + 1);
  t_tzDatArray[0] = ((loByte << 0) & 0xFF) + ((hiByte << 8) & 0xFF00);
  loByte = EEPROM.read(m_addr);
  hiByte = EEPROM.read(m_addr + 1);
  t_tzDatArray[1] = ((loByte << 0) & 0xFF) + ((hiByte << 8) & 0xFF00);
  
  // Do a quick boundary check for our read values, and if they're not
  // right due to uninitialized EEPROM or previously written EEPROM
  // data reset them.
  if(t_tzDatArray[0] > 11 || t_tzDatArray[0] < -11 || t_tzDatArray[1] > 59 || t_tzDatArray[1] < -59) {
    #ifdef DEBUGON
      Serial.print("Got invalid time zone data for ID ");
      Serial.print(t_tzID);
      Serial.print(" - ");
      Serial.print(t_tzDatArray[0]);
      Serial.print(":");
      Serial.println(t_tzDatArray[1]);
    #endif

    // Set our system-wide time zone to GMT.
    t_tzDatArray[0] = 0;
    t_tzDatArray[1] = 0;
    
    // And write GMT as the offset to EEPROM so next time we have good data.
    //saveTZ(t_tzID, 0, 0);
  }

  #ifdef DEBUGON
    Serial.print("Loaded time zone ");
    Serial.print(t_tzID);
    Serial.print(" as GMT ");
    Serial.print(t_tzDatArray[0]);
    Serial.print(":");
    Serial.println(t_tzDatArray[1]);
  #endif

  return;
}

// Ssve time zone offset data.
void saveTZ(int t_tzID, int t_offsetHrs, int t_offsetMins) {
  // Dummy function, do nothing until implemented.
  #ifdef DEBUGON
    Serial.print("Setting time zone ");
    Serial.print(t_tzID);
    Serial.print(" as GMT ");
    Serial.print(t_offsetHrs);
    Serial.print(":");
    Serial.println(t_offsetMins);
  #endif
  // Set our good data flag
  boolean goodTzID = false;
  
  // Create our storage variables.
  int h_addr;
  int m_addr;
  // Account for using signed ints by using 2 bytes, not 1.
  byte hiByte;
  byte loByte; 

  // Figure out which time zone we're trying to save.
  switch(t_tzID) {
    case 0:
      goodTzID = true;
      h_addr = EEP_TZ_HR_A;
      m_addr = EEP_TZ_MIN_A;
      break;
    case 1:
      goodTzID = true;
      h_addr = EEP_TZ_HR_B;
      m_addr = EEP_TZ_MIN_B;
      break;
    default:
      // Don't set any variables.
      #ifdef DEBUGON
        Serial.print("Invalid time zone ID: ");
        Serial.println(t_tzID);
      #endif
      break;
  }
  
  // If we got a good time we can write the offset data to the EEPROM
  if(goodTzID) {
    // Write time zone data at the specified addresses.
    loByte = ((t_offsetHrs >> 0) & 0xFF);
    hiByte = ((t_offsetHrs >> 8) & 0xFF);
    EEPROM.write(h_addr, loByte);
    EEPROM.write(h_addr + 1, hiByte);
    loByte = ((t_offsetMins >> 0) & 0xFF);
    hiByte = ((t_offsetMins >> 8) & 0xFF);
    EEPROM.write(m_addr, loByte);
    EEPROM.write(m_addr + 1, hiByte);

  }
  
  #ifdef DEBUGON
    Serial.print("Saved time zone ");
    Serial.print(t_tzID);
    Serial.print(" as GMT ");
    Serial.print(t_offsetHrs);
    Serial.print(":");
    Serial.println(t_offsetMins);
  #endif
  
  return;
}

// This is called on boot to grab the timezone from EEPROM.
void configTZ() {
  #ifdef DEBUGON
    Serial.println("Configuring time zones...");
  #endif
  
  #ifdef DEBUGON
    Serial.print("Global time zone ID 0 is now GMT ");
    Serial.print(tz_a[0]);
    Serial.print(":");
    Serial.println(tz_a[1]);
    Serial.print("Global time zone ID 1 is now GMT ");
    Serial.print(tz_b[0]);
    Serial.print(":");
    Serial.println(tz_b[1]);
  #endif

  // Grab both time zone values from EEPROM, and set tz_a and tz_b with them.
  loadTZ(TZ_A_ID, tz_a);
  loadTZ(TZ_B_ID, tz_b);
  
  #ifdef DEBUGON
    Serial.print("Global time zone ID 0 is now GMT ");
    Serial.print(tz_a[0]);
    Serial.print(":");
    Serial.println(tz_a[1]);
    Serial.print("Global time zone ID 1 is now GMT ");
    Serial.print(tz_b[0]);
    Serial.print(":");
    Serial.println(tz_b[1]);
  #endif
  
  // Set our current time zone
  setCurrentTZ(currentTzID);
  
  return;
}


/**********************************
 * DEVICE CONFIGURATION FUNCTIONS *
 **********************************/

// Configure the touch sensor.
void configTouch() {
#ifdef DEBUGON
  Serial.println("Configuring touch sensor...");
#endif

  // Configure the necessary registers.
  cap.writeRegister(0x00, 0x00); // Set main config register to make sure everything is zero, including the INT bit.
  cap.writeRegister(0x27, 0xFF); // Activate IRQ for all touch channels!
  cap.writeRegister(0x28, 0x00); // Turn off interrupt on hold.
  cap.writeRegister(0x2A, 0x00); // Allow multiple touches because the diagnostic LEDs look cool.
  cap.writeRegister(0x41, 0x40); // "Speedup" off.
  cap.writeRegister(0x44, 0x41); // Set interrupt on press but not release?
  
  return;
}

// Configure the RTC.
void configRTC() {
#ifdef DEBUGON
  Serial.println("Configuring RTC...");
#endif
  // Turn off the 32kHz wave generator
  rtc.enable32kHz(false);
  // Turn on the SQW pin so we can get an IRQ every second.
  rtc.SQWEnable(true);
  // We don't want battery backup AND SQW running.
  rtc.BBSQWEnable(false);
  // Set our IRQ frequency at 1Hz (one pulse per second)
  rtc.SQWFrequency(RTC_SQW_FREQ);
  
  return;
}

// Set active time zone.
void setCurrentTZ(int t_tzID) {
  #ifdef DEBUGON
    Serial.print("Switching to timezone ID: ");
    Serial.println(t_tzID);
  #endif
  
  // Select the time zone we want to use and set the color of the second flash LED and the offset.
  switch(t_tzID) {
    case 0:
      // Set the seconds LED color
      f_secl_r = F_SECL_TZA_R;
      f_secl_g = F_SECL_TZA_G;
      f_secl_b = F_SECL_TZA_B;
      // Set the GMT offset.
      currentTz[0] = tz_a[0]; // Hour offset
      currentTz[1] = tz_a[1]; // Minute offset
      break;
    case 1:
      // Set the seconds LED color
      f_secl_r = F_SECL_TZB_R;
      f_secl_g = F_SECL_TZB_G;
      f_secl_b = F_SECL_TZB_B;
      // Set the GMT offset.
      currentTz[0] = tz_b[0]; // Hour offset
      currentTz[1] = tz_b[1]; // Minute offset
      break;
    default:
      // do nothing.
      break;
  }
  
  return;
}


/******************************
 * CONVERSIONS AND "WRAPPERS" *
 ******************************/
 
// Compensate for GMT offset in hours by wrapping from 0-23 hrs, 0-59 mins, and vice versa.
void setCurrentTime() {
  // Hour carry variable to be used if we need to add or subtract an hour based on the minutes.
  int hrCarry = 0;
  
  // Adjusted hour and minute to be returned.
  int adjHr;
  int adjMin;
  
  // Set up variables to test with and use for adjusting the "current" time.
  int offsetTestMin = r_min + currentTz[1];
  int offsetTestHr = r_hr + currentTz[0];
  
  #ifdef DEBUGON
    Serial.print("Adjusting ");
    Serial.print(r_hr);
    Serial.print(":");
    Serial.print(r_min);
    Serial.print(" for GMT offset ");
    Serial.print(currentTz[0]);
    Serial.print(":");
    Serial.println(currentTz[1]);
  #endif
  
  // If we're out of bounds on the minute after adjusting the offset let's fix it.
  if (offsetTestMin <= 59 && offsetTestMin >= 0) {
    // Set the returned minute as-is.
    adjMin = offsetTestMin;
  } else {
    // If it's too big trim it.
    if (offsetTestMin > 59) {
      adjMin = offsetTestMin - 59;
      // Also carry an hour because we just rolled up one hour.
      hrCarry++;
    } else {
      // It's too small so let's add 59 seconds.
      adjMin = offsetTestMin + 59;
      // Roll back one hour since we rolld back an hour.
      hrCarry--;
    }
  }
  
  // Adjust our offset before looking at the hour value.
  offsetTestHr += hrCarry;
  
  // If we're out of bounds on the hour after adjusting th eoffset let's fix it, too.
  if (offsetTestHr <= 23 && offsetTestHr >= 0) {
    // We're good as-is so let's just return what we've already calculated.
    adjHr = offsetTestHr;
  } else {
    // If the hour was too "big"
    if (offsetTestHr > 23) {
      // Adjust it.
      adjHr = offsetTestHr - 23;
    } else {
      // If it was too small adjust it.
      adjHr = offsetTestHr + 23;
    }
  }
  
  #ifdef DEBUGON
    Serial.print("... and got ");
    Serial.print(adjHr);
    Serial.print(":");
    Serial.println(adjMin);
  #endif
  
  // Now update the time we were fed.
  currentHr = adjHr;
  currentMin = adjMin;
  
  return;
}

 
 /**********************
 * INTERRUPT HANDLERS *
 **********************/

// Set our touch IRQ flag.
void setTIrqFlag() {
  // Set the flag
  t_IrqFlag = 1;
  
  return;
}

// Clear the touch IRQ flag and reset the IRQ on the sensor.
void clearTIRQ() {
  #ifdef DEBUGON
    Serial.println("Clearing touch IRQ.");
  #endif
  
  // Clear the flag.
  t_IrqFlag = 0;
  
  // Clear the INT bit on the main configuration register,
  // leaving the rest of the register in tact.
  cap.writeRegister(0x00, cap.readRegister(0x00) & 0xFE);
  
  return;
}

// Check the touch IRQ
boolean checkTIRQ() {
  // Return the flag state.
  return t_IrqFlag;
}

// Set our RTC IRQ flag.
void setRIrqFlag() {
  // Note: this gets set every time the RTC
  // ticks one second.
  // Set the flag
  r_IrqFlag = 1;
  
  return;
}

// Clear the RTC IRQ flag.
void clearRIRQ() {
  // Clear the RTC IRQ flag.
  r_IrqFlag = 0;
  
  return;
}

// Check the RTC IRQ
boolean checkRIRQ() {
  // Return the IRQ flag.
  return r_IrqFlag;
}
