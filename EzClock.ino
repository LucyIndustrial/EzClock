/*
 Clock code by Josh Lucy <joshATlucyindustrialDAWTcom>.
 
 This work is licensed under a Creative Commons Attribution 4.0
 International License.
 This code is designed for an Adafruit Flora using a ChronoDot 2.1 RTC,
 Adafruit CAP1188 touch sensor, and Adafruit NeoPixels, but is set up to
 be prototyped and debugged on an Arduino Uno.
 */

/*
 * - TODO -
 *
 * + Fix touch IRQ, check config registers, may have to do with Pull-up resistor
 * + Add set time code, etc.
 * + Add GMT offset code, etc.
 * + Remove gamma correction from minutes and seconds. One step is hidden b/c of
 *   the gamma table.
 *
 */

/************
 * INCLUDES *
 ************/

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
#define T_KEY1       0x01
#define T_KEY2       0x02
#define T_KEY3       0x04
#define T_KEY4       0x08
#define T_KEY5       0x10
#define T_KEY6       0x20
#define T_KEY7       0x40
#define T_KEY8       0x80

// Touch sensor IRQ pin
#ifdef USINGFLORA
  #define T_IRQPIN   9
#else
  #define T_IRQPIN   2
#endif

// ChronoDot 2.0/DS3231 RTC config
#define RTC_SQW_FREQ DS3231_SQW_FREQ_1

// ChronoDot RTC SQW/IRQ pin
#ifdef USINGFLORA
  #define R_IRQPIN   10
#else
  #define R_IRQPIN   3
#endif

// Turn this LED off after boot.
#ifdef USINGFLORA
  #define INDLED     7  // On-board Flora LED PIN
#else
  #define INDLED     13 // On-board Arduino Uno LED PIN
#endif


// Clock face config - Part of this assumes pixels are hooked up like hr -> min -> sec
#define F_LENGTH     37 // How many NeoPixels do we have in the face?

#define F_DEFAULT_R  0 // The "background" color for the clock face.
#define F_DEFAULT_G  0
#define F_DEFAULT_B  0

// Clock face hour marks
#define F_HR_MARK_R  135 // Tick mark RGB values.
#define F_HR_MARK_G  90
#define F_HR_MARK_B  0

// Configure hours stuff.
#define F_HRSTART    0  // Which pixel does the hour start at?
#define F_HRLEN      24 // The hour is 24 pixels long.
#define F_HR_R       0  // RGB values for the hour
#define F_HR_G       90
#define F_HR_B       90
#define F_HR_TRLEN   6   // Add a 6-pixel trail behind the current hour pixel.


#define F_MINSTART   24 // Which pixel does the minute start at?
#define F_MINLEN     12 // The minute is 12 pixels long.
#define F_MIN_R      90   // RGB values for the minutes
#define F_MIN_G      0
#define F_MIN_B      0

#define F_SECSTART   24 // Which pixel is for seconds?
#define F_SECLEN     12 // The seconds is 1 pixel long
#define F_SEC_R      0   // RGB values for the seconds
#define F_SEC_G      90
#define F_SEC_B      0

#define F_SECLSTART  36 // Where is our center second LED
#define F_SECL_DELAY 10 // How long do we wait before dimming again?
#define F_SECL_STEP  10 // How many dimming steps do we have?
#define F_SECL_R     100 // Second pixel RGB values
#define F_SECL_G     0
#define F_SECL_B     50

#define F_LIGHT_A    85 // When the clock face is used as a light, this is the first brightness setting.
#define F_LIGHT_B    169 // Second brightness setting
#define F_LIGHT_C    255 // Last brightness setting
#define F_LIGHT_DLY  100 // Light mode animation delay.

// NeoPixel clock face data pin
#ifdef USINGFLORA
  #define F_DATAPIN  6
#else
  #define F_DATAPIN  4
#endif


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
int r_hr;
int r_min;
int r_sec;

/*******************************
 * ARDUINO SETUP AND MAIN LOOP *
 *******************************/

// System setup method
void setup() {
// Set up our diagnostic indicator LED
pinMode(INDLED, OUTPUT);

// Turn on the onboard LED to show we're setting up.
digitalWrite(INDLED, LOW);

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
if (!rtc.isrunning()) {
  #ifdef DEBUGON
    Serial.println("RTC not running...");
  #endif
} else {
  Serial.println("RTC running...");
}

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

  // Set our interrupts.
  attachInterrupt(0, setTIrqFlag, FALLING); // Touch
  attachInterrupt(1, setRIrqFlag, FALLING); // RTC

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
  if (r_IrqFlag) {
    
    // Clear the IRQ flag since we're handling it now.
    r_IrqFlag = 0;

    #ifdef DEBUGON
      Serial.println("Tick, tock.");
    #endif
   
    // Get the new time value from the RTC.
    getRTCTime();
    
    // Show our clock face since we have new time data!
    showClockFace();

  }

  // Do we have a touch IRQ?
  if (t_IrqFlag) {
    // Clear the IRQ flag since we're handling it now.
    t_IrqFlag = 0;

    // Grab the state of the touched keys.
    uint8_t touched = cap.touched();

    // Debug
    #ifdef DEBUGON
      Serial.println("Got touch IRQ.");
      Serial.println(touched, HEX);
    #endif

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
    Serial.println("Handling touched key.");
  #endif
  
  // What was touched?
  switch(t_touched) {
    // Key 1 pressed from normal clock mode.
  case T_KEY1:
    // Display time zone #1
    break;

  // Display time zone #2
  case T_KEY2:
    break;

  // Key 3 pressed from normal clock mode.
  case T_KEY3:
    // Activate light mode.
    handleLight();
    break;

    // Set UTC offset for time zone #1
  case T_KEY6:
    // We change the time zone here.
    break;

      // Set UTC offset for time zone #2
  case T_KEY7:
    // We change the time zone here.
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
}

// If we're trying to turn the light on...
void handleLight() {
  #ifdef DEBUGON
    Serial.println("Handling the LED light.");
  #endif

  // Define our steps based on the #define statements "at the top".
  int lightSteps[] = {F_LIGHT_A, F_LIGHT_B, F_LIGHT_C};

  // Start-up animation
  face.setPixelColor(F_SECLSTART, gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]]);
  face.show();
  delay(F_LIGHT_DLY);
  setFaceRange(F_MINSTART, F_MINLEN - 1, gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]]);
  delay(F_LIGHT_DLY);
  setFaceRange(F_HRSTART, F_HRLEN - 1, gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]]);


  // Go until we've looped through all possible values.
  while(currentStep < 3) {
    // If the IRQ
    if (t_IrqFlag) {
      #ifdef DEBUGON
        Serial.println("Light step up.");
      #endif
    
      // Clear the IRQ because we're handling it.
      t_IrqFlag = 0;

      // Set the light step, 0 for off, 2 for brightest.
      currentStep++;

      // Wind-down animation
      setFaceRange(0, F_LENGTH - 1, gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]], gamma[lightSteps[currentStep]]);
      delay(50);
    }
  }

  #ifdef DEBUGON
    Serial.println("Done lighting.");
  #endif
 
  // Reset our counter.
  currentStep = 0;

  // Wind-down animation
  face.setPixelColor(F_SECSTART, 0, 0, 0);
  face.show();
  delay(F_LIGHT_DLY);
  setFaceRange(F_MINSTART, F_MINLEN - 1, 0, 0, 0);
  delay(F_LIGHT_DLY);
  setFaceRange(F_HRSTART, F_HRLEN - 1, 0, 0, 0);
}


/************************
 * CLOCK FACE FUNCTIONS *
 ************************/
// Show the clock face.
void showClockFace() {
  // Seconds will be an interrupt-driven animation
  // so they don't show up here.
  
  // Blank the clock.
  setFaceColor(faceDefColor);

 
  // Display hour marks.
  showFaceHrMarks(hrMarkColor);
  // Display our hours
  showFaceHr(r_hr);
  // Show our minutes
  showFaceMin(r_min);
  // Show our seconds.
  showFaceSec(r_sec);
  
  // Draw the clock face since we have all the relevant stuff ready.
  drawClockFace();
  
  // And dow the seconds animation thing
  showSecFlash();
}

// Draw the clock face.
void drawClockFace() {
  // Draw the face.
  face.show();

}

// Show hour ticks at 0:00, 6:00, 12:00, and 18:00 hour marks
void showFaceHrMarks(uint32_t t_color) {
  face.setPixelColor(0, t_color);
  face.setPixelColor(6, t_color);
  face.setPixelColor(12, t_color);
  face.setPixelColor(18, t_color);
}

// Show the hour..
void showFaceHr(int t_hr) {
  // Store the target pixel for the hour trail.
  int x_tgt;

 
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
}

// Zero out the clock's face so we can start over, but don't waste
// cycles showing it.
void setFaceColor(uint32_t t_color) {
  // Set each element of the face to a specific color.
  for(int i = 0; i < F_LENGTH; i++) {
    face.setPixelColor(i, t_color);

  }
}

// Set the hour pixel.
void showFaceMin(int t_min) {

  // We want 5 dimming stages using the minute mask value
  // determined by min % 5.

  // Set the minute LED "cursor".
  int x_minPix = t_min / 5;
  // Figure out how bright to make the
  // LED the cursor points to.
  int brightIndex = t_min % 5;
 
  int scaledR = map(brightIndex, 0, 4, F_DEFAULT_R, F_MIN_R);
  int scaledG = map(brightIndex, 0, 4, F_DEFAULT_G, F_MIN_G);
  int scaledB = map(brightIndex, 0, 4, F_DEFAULT_B, F_MIN_B);

  // Set our color mask based
  uint32_t cursorColor = face.Color(gamma[scaledR], gamma[scaledG], gamma[scaledB]);


  // Set "current" minute pixel
  face.setPixelColor(F_MINSTART + x_minPix, cursorColor | face.getPixelColor(F_MINSTART + x_minPix));


  // Set previous minute pixels for fill-in effect.
  for (int i = x_minPix - 1; i >= 0; i--) {
    face.setPixelColor(i + F_MINSTART, face.Color(gamma[F_MIN_R], gamma[F_MIN_G], gamma[F_MIN_B]) | face.getPixelColor(i + F_MINSTART));

  }
}

// Set the hour pixel.
void showFaceSec(int t_sec) {

  // Set the second LED "cursor".
  int x_secPix = t_sec / 5;
  // Figure out how bright to make the
  // LED the cursor points to.
  int brightIndex = t_sec % 5;
 
  int scaledR = map(brightIndex, 0, 4, F_DEFAULT_R, F_SEC_R);
  int scaledG = map(brightIndex, 0, 4, F_DEFAULT_G, F_SEC_G);
  int scaledB = map(brightIndex, 0, 4, F_DEFAULT_B, F_SEC_B);

  // Set our color mask based
  uint32_t cursorColor = face.Color(gamma[scaledR], gamma[scaledG], gamma[scaledB]);


  // Set "current" minute pixel
  face.setPixelColor(F_SECSTART + x_secPix, cursorColor ^ face.getPixelColor(F_SECSTART + x_secPix));


  // Set previous minute pixels for fill-in effect.
  for (int i = x_secPix - 1; i >= 0; i--) {
    face.setPixelColor(i + F_SECSTART, face.Color(gamma[F_SEC_R], gamma[F_SEC_G], gamma[F_SEC_B]) | face.getPixelColor(i + F_SECSTART));

  }
}

// Show a little flash when we register a new second.
void showSecFlash() {
  // Hold our scaled values here.
  int scaledR;
  int scaledG;
  int scaledB;
  
  // Light and fade to black.
  for (int i = F_SECL_STEP; i >= 0; i--) {
    // Use map() to dim LED in a porportional fasion. 
    scaledR = map(i, 0, F_SECL_STEP, F_DEFAULT_R, F_SECL_R);
    scaledG = map(i, 0, F_SECL_STEP, F_DEFAULT_G, F_SECL_G);
    scaledB = map(i, 0, F_SECL_STEP, F_DEFAULT_B, F_SECL_B);
    
    // Write the color.
    face.setPixelColor(F_SECLSTART, face.Color(gamma[scaledR], gamma[scaledG], gamma[scaledB]));
    
    // And draw the clock face since we're in the loop still.
    drawClockFace();
    
    // Wait a bit.
    delay(F_SECL_DELAY);
  }

}

// Set a range of pixels
void setFaceRange(int t_first, int t_last, int t_r, int t_g, int t_b) {
  // Set the pixels.
  for (int i = t_first; i <= t_last; i++) {
    // Assign the current pixel the color.
    face.setPixelColor(i, t_r, t_g, t_b);
  }
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
  cap.writeRegister(0x27, 0xFF); // Activate IRQ for all touch channels!
  cap.writeRegister(0x28, 0x00); // Turn off interrupt on hold.
  cap.writeRegister(0x2A, 0x00); // Allow multiple touches because the diagnostic LEDs look cool.
  cap.writeRegister(0x41, 0x40); // "Speedup" off.
  cap.writeRegister(0x44, 0x41); // Set interrupt on press but not release?
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
}

/**********************
 * INTERRUPT HANDLERS *
 **********************/

// Set our touch IRQ flag.
void setTIrqFlag() {
  // Set the flag
  t_IrqFlag = 1;
}

// Set our RTC IRQ flag.
void setRIrqFlag() {
  // Note: this gets set every time the RTC
  // ticks one second.
  // Set the flag
  r_IrqFlag = 1;
}

