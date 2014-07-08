Ez Clock
=====

This is a clock project written for Arduino uCs, with a Creative Commons 4.0 Attribution license by Josh Lucy of Lucy Industrial. This clock project is designed to use Adafruit NeoPixel elements as the clock display, a ChronoDot 2.0/2.1 real time clock board, and a CAP1188 touch sensor to accept user input.

Features:
 - Clock code is open source with a simple Creative Commons 4.0 Attribution license.
 - RGB LED clock face
 - Minutes and seconds are displayed like a circular progress bar or gauge
 - Flash as each second passes
 - Features dual independently-configured time zones, configured as GMT offsets
 - Daylight savings time changes are easily implemented by changing only the GMT offset for a given time zone.
 - GMT offset includes the ability to set hours and minutes
 - Flashlight with three brightness settings
 - Animations
 - Customizable color scheme
 - Designed for open-source hardware with open source software
 - Adaptable and easy to re-create with minimal soldering skills

This project uses the following 3rd party Arduino libraries:
 - Adafruit CAP1188 Library: https://github.com/adafruit/Adafruit_CAP1188_Library/
 - Adafruit NeoPixel Library: https://github.com/adafruit/Adafruit_NeoPixel/
 - Mizraith's RTC Library: https://github.com/mizraith/RTClib/

Minimum parts list for the project as-designed:
 - A computer that has the Arduino IDE installed.
 - Arduino Uno or Adafruit Flora microcontroller and related cabling/programmer.
 - 37 Adafruit NeoPixels connected as a single strand
 - A ChronoDot 2.0/2.1 RTC module (based on the DS3231 RealTime Clock IC) and suitable battery.
 - Adafruit CAP1188 capacitive touch sesnor module
 - 10K Ohm resistor to be used as a pull-up resistor for the SQW pin on the ChronoDot module
 - 5 or 3.3V power supply suitable for the microcontroller being used and above hardware
 - An optional switch to turn the LEDs, microcontroller, and touch sensor off.
 - Hookup wire and related tools
 - Solder and related tools
