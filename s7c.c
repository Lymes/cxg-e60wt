//
//  s7c.c
//  CXG60EWT
//
//  Created by Leonid Mesentsev on 26/06/2019.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//

#include <s7c.h>
#include <stm8s_pins.h>
#include <delay.h>

#define BLANK_IDX 36 // Must match with 'digitCodeMap'
#define DASH_IDX 37
#define PERIOD_IDX 38
#define ASTERISK_IDX 39

static const long powersOf10[] = {
    1, // 10^0
    10,
    100,
    1000,
    10000,
    100000,
    1000000,
    10000000,
    100000000,
    1000000000}; // 10^9

static const long powersOf16[] = {
    0x1, // 16^0
    0x10,
    0x100,
    0x1000,
    0x10000,
    0x100000,
    0x1000000,
    0x10000000}; // 16^7

// digitCodeMap indicate which segments must be illuminated to display
// each number.
static const uint8_t digitCodeMap[] = {
    //  GFEDCBA  Segments      7-segment map:
    0B00111111, // 0   "0"          AAA
    0B00000110, // 1   "1"         F   B
    0B01011011, // 2   "2"         F   B
    0B01001111, // 3   "3"          GGG
    0B01100110, // 4   "4"         E   C
    0B01101101, // 5   "5"         E   C
    0B01111101, // 6   "6"          DDD
    0B00000111, // 7   "7"
    0B01111111, // 8   "8"
    0B01101111, // 9   "9"
    0B01110111, // 65  'A'
    0B01111100, // 66  'b'
    0B00111001, // 67  'C'
    0B01011110, // 68  'd'
    0B01111001, // 69  'E'
    0B01110001, // 70  'F'
    0B00111101, // 71  'G'
    0B01110110, // 72  'H'
    0B00000110, // 73  'I'
    0B00001110, // 74  'J'
    0B01110110, // 75  'K'  Same as 'H'
    0B00111000, // 76  'L'
    0B00000000, // 77  'M'  NO DISPLAY
    0B01010100, // 78  'n'
    0B00111111, // 79  'O'
    0B01110011, // 80  'P'
    0B01100111, // 81  'q'
    0B01010000, // 82  'r'
    0B01101101, // 83  'S'
    0B01111000, // 84  't'
    0B00111110, // 85  'U'
    0B00111110, // 86  'V'  Same as 'U'
    0B00000000, // 87  'W'  NO DISPLAY
    0B01110110, // 88  'X'  Same as 'H'
    0B01101110, // 89  'y'
    0B01011011, // 90  'Z'  Same as '2'
    0B00000000, // 32  ' '  BLANK
    0B01000000, // 45  '-'  DASH
    0B10000000, // 46  '.'  PERIOD
    0B01100011, // 42 '*'  DEGREE ..
};

// Constant pointers to constant data
const uint8_t *const numeralCodes = digitCodeMap;
const uint8_t *const alphaCodes = digitCodeMap + 10;

// SevSeg Constructor
/******************************************************************************/

static uint8_t digitOnVal, digitOffVal, segmentOnVal, segmentOffVal;
static uint8_t resOnSegments = 0, updateWithDelays = 0, leadingZeros;
static uint8_t digitPins[MAXNUMDIGITS];
static uint8_t segmentPins[8];
static uint8_t numDigits = 4;
static uint8_t numSegments;
static uint8_t prevUpdateIdx = 0;        // The previously updated segment or digit
static uint8_t digitCodes[MAXNUMDIGITS]; // The active setting of each segment of each digit
static uint64_t prevUpdateTime = 0;      // The time (millis()) when the display was last updated
static int ledOnTime = 1;                // The time (us) to wait with LEDs on
static int waitOffTime = 0;              // The time (us) to wait with LEDs off
static uint8_t waitOffActive = 0;        // Whether  the program is waiting with LEDs off

void S7C_init()
{
  uint8_t numDigits = 4;
  uint8_t digitPins[] = {PD0, PD1, PD2, PD3};
  uint8_t segmentPins[] = {PC7, PC5, PC3, PE5, PC2, PC6, PC1, PC4};
  uint8_t resistorsOnSegments = false;   // 'false' means resistors are on digit pins
  uint8_t hardwareConfig = COMMON_ANODE; // See README.md for options
  uint8_t updateWithDelays = false;      // Default. Recommended
  uint8_t leadingZeros = false;          // Use 'true' if you'd like to keep the leading zeros
  S7C_begin(hardwareConfig, numDigits, digitPins, segmentPins, resistorsOnSegments, updateWithDelays, leadingZeros, 1);
}

// begin
/******************************************************************************/
// Saves the input pin numbers to the class and sets up the pins to be used.
// If you use current-limiting resistors on your segment pins instead of the
// digit pins, then set resOnSegments as true.
// Set updateWithDelays to true if you want to use the 'pre-2017' update method
// In that case, the processor is occupied with delay functions while refreshing
// leadingZerosIn indicates whether leading zeros should be displayed
// disableDecPoint is true when the decimal point segment is not connected, in
// which case there are only 7 segments.
void S7C_begin(uint8_t hardwareConfig, uint8_t numDigitsIn, uint8_t digitPinsIn[],
               uint8_t segmentPinsIn[], uint8_t resOnSegmentsIn,
               uint8_t updateWithDelaysIn, uint8_t leadingZerosIn, uint8_t disableDecPoint)
{

  resOnSegments = resOnSegmentsIn;
  updateWithDelays = updateWithDelaysIn;
  leadingZeros = leadingZerosIn;

  numDigits = numDigitsIn;
  numSegments = disableDecPoint ? 7 : 8; // Ternary 'if' statement
  //Limit the max number of digits to prevent overflowing
  if (numDigits > MAXNUMDIGITS)
    numDigits = MAXNUMDIGITS;

  switch (hardwareConfig)
  {

  case 0: // Common cathode
    digitOnVal = LOW;
    segmentOnVal = HIGH;
    break;

  case 1: // Common anode
    digitOnVal = HIGH;
    segmentOnVal = LOW;
    break;

  case 2: // With active-high, low-side switches (most commonly N-type FETs)
    digitOnVal = HIGH;
    segmentOnVal = HIGH;
    break;

  case 3: // With active low, high side switches (most commonly P-type FETs)
    digitOnVal = LOW;
    segmentOnVal = LOW;
    break;
  }

  digitOffVal = !digitOnVal;
  segmentOffVal = !segmentOnVal;

  // Save the input pin numbers to library variables
  for (uint8_t segmentNum = 0; segmentNum < numSegments; segmentNum++)
  {
    segmentPins[segmentNum] = segmentPinsIn[segmentNum];
  }

  for (uint8_t digitNum = 0; digitNum < numDigits; digitNum++)
  {
    digitPins[digitNum] = digitPinsIn[digitNum];
  }

  // Set the pins as outputs, and turn them off
  for (uint8_t digit = 0; digit < numDigits; digit++)
  {
    pinMode(digitPins[digit], OUTPUT);
    setPin(digitPins[digit], digitOffVal);
  }

  for (uint8_t segmentNum = 0; segmentNum < numSegments; segmentNum++)
  {
    pinMode(segmentPins[segmentNum], OUTPUT);
    setPin(segmentPins[segmentNum], segmentOffVal);
  }

  S7C_blank(); // Initialise the display
}

// refreshDisplay
/******************************************************************************/
// Turns on the segments specified in 'digitCodes[]'
// There are 4 versions of this function, with the choice depending on the
// location of the current-limiting resistors, and whether or not you wish to
// use 'update delays' (the standard method until 2017).
// For resistors on *digits* we will cycle through all 8 segments (7 + period),
//    turning on the *digits* as appropriate for a given segment, before moving on
//    to the next segment.
// For resistors on *segments* we will cycle through all __ # of digits,
//    turning on the *segments* as appropriate for a given digit, before moving on
//    to the next digit.
// If using update delays, refreshDisplay has a delay between each digit/segment
//    as it cycles through. It exits with all LEDs off.
// If not using updateDelays, refreshDisplay exits with a single digit/segment
//    on. It will move to the next digit/segment after being called again (if
//    enough time has passed).

void S7C_refreshDisplay(uint64_t us)
{

  if (!updateWithDelays)
  {
    // Exit if it's not time for the next display change
    if (waitOffActive)
    {
      if (us - prevUpdateTime < waitOffTime)
        return;
    }
    else
    {
      if (us - prevUpdateTime < ledOnTime)
        return;
    }
    prevUpdateTime = us;

    if (!resOnSegments)
    {
      /**********************************************/
      // RESISTORS ON DIGITS, UPDATE WITHOUT DELAYS

      if (waitOffActive)
      {
        waitOffActive = false;
      }
      else
      {
        // Turn all lights off for the previous segment
        S7C_segmentOff(prevUpdateIdx);

        if (waitOffTime)
        {
          // Wait a delay with all lights off
          waitOffActive = true;
          return;
        }
      }

      prevUpdateIdx++;
      if (prevUpdateIdx >= numSegments)
        prevUpdateIdx = 0;

      // Illuminate the required digits for the new segment
      S7C_segmentOn(prevUpdateIdx);
    }
    else
    {
      /**********************************************/
      // RESISTORS ON SEGMENTS, UPDATE WITHOUT DELAYS

      if (waitOffActive)
      {
        waitOffActive = false;
      }
      else
      {
        // Turn all lights off for the previous digit
        S7C_digitOff(prevUpdateIdx);

        if (waitOffTime)
        {
          // Wait a delay with all lights off
          waitOffActive = true;
          return;
        }
      }

      prevUpdateIdx++;
      if (prevUpdateIdx >= numDigits)
        prevUpdateIdx = 0;

      // Illuminate the required segments for the new digit
      S7C_digitOn(prevUpdateIdx);
    }
  }

  // else
  // {
  //   if (!resOnSegments)
  //   {
  //     /**********************************************/
  //     // RESISTORS ON DIGITS, UPDATE WITH DELAYS
  //     for (uint8_t segmentNum = 0; segmentNum < numSegments; segmentNum++)
  //     {

  //       // Illuminate the required digits for this segment
  //       S7C_segmentOn(segmentNum);

  //       // Wait with lights on (to increase brightness)
  //       delayMicroseconds(ledOnTime);

  //       // Turn all lights off
  //       S7C_segmentOff(segmentNum);

  //       // Wait with all lights off if required
  //       if (waitOffTime)
  //         delayMicroseconds(waitOffTime);
  //     }
  //   }
  //   else
  //   {
  //     /**********************************************/
  //     // RESISTORS ON SEGMENTS, UPDATE WITH DELAYS
  //     for (uint8_t digitNum = 0; digitNum < numDigits; digitNum++)
  //     {

  //       // Illuminate the required segments for this digit
  //       S7C_digitOn(digitNum);

  //       // Wait with lights on (to increase brightness)
  //       delayMicroseconds(ledOnTime);

  //       // Turn all lights off
  //       S7C_digitOff(digitNum);

  //       // Wait with all lights off if required
  //       if (waitOffTime)
  //         delayMicroseconds(waitOffTime);
  //     }
  //   }
  // }
}

// segmentOn
/******************************************************************************/
// Turns a segment on, as well as all corresponding digit pins
// (according to digitCodes[])
void S7C_segmentOn(uint8_t segmentNum)
{
  setPin(segmentPins[segmentNum], segmentOnVal);
  for (uint8_t digitNum = 0; digitNum < numDigits; digitNum++)
  {
    if (digitCodes[digitNum] & (1 << segmentNum))
    { // Check a single bit
      setPin(digitPins[digitNum], digitOnVal);
    }
  }
}

// segmentOff
/******************************************************************************/
// Turns a segment off, as well as all digit pins
void S7C_segmentOff(uint8_t segmentNum)
{
  for (uint8_t digitNum = 0; digitNum < numDigits; digitNum++)
  {
    setPin(digitPins[digitNum], digitOffVal);
  }
  setPin(segmentPins[segmentNum], segmentOffVal);
}

// digitOn
/******************************************************************************/
// Turns a digit on, as well as all corresponding segment pins
// (according to digitCodes[])
void S7C_digitOn(uint8_t digitNum)
{
  setPin(digitPins[digitNum], digitOnVal);
  for (uint8_t segmentNum = 0; segmentNum < numSegments; segmentNum++)
  {
    if (digitCodes[digitNum] & (1 << segmentNum))
    { // Check a single bit
      setPin(segmentPins[segmentNum], segmentOnVal);
    }
  }
}

// digitOff
/******************************************************************************/
// Turns a digit off, as well as all segment pins
void S7C_digitOff(uint8_t digitNum)
{
  for (uint8_t segmentNum = 0; segmentNum < numSegments; segmentNum++)
  {
    setPin(segmentPins[segmentNum], segmentOffVal);
  }
  setPin(digitPins[digitNum], digitOffVal);
}

// setNumber
/******************************************************************************/
// This function only receives the input and passes it to 'setNewNum'.
// It is overloaded for all number data types, so that floats can be handled
// correctly.

// void S7C_setNumber(int numToShow, uint8_t decPlaces, uint8_t hex)
// { //int
//   S7C_setNewNum(numToShow, decPlaces, hex);
// }

// setNewNum
/******************************************************************************/
// Changes the number that will be displayed.
// void S7C_setNewNum(long numToShow, uint8_t decPlaces, uint8_t hex)
// {
//   uint8_t digits[4];
//   S7C_findDigits(numToShow, decPlaces, hex, digits);
//   S7C_setDigitCodes(digits, decPlaces);
// }

// setSegments
/******************************************************************************/
// Sets the 'digitCodes' that are required to display the desired segments.
// Using this function, one can display any arbitrary set of segments (like
// letters, symbols or animated cursors). See setDigitCodes() for common
// numeric examples.
//
// Bit-segment mapping:  0bHGFEDCBA
//      Visual mapping:
//                        AAAA          0000
//                       F    B        5    1
//                       F    B        5    1
//                        GGGG          6666
//                       E    C        4    2
//                       E    C        4    2        (Segment H is often called
//                        DDDD  H       3333  7      DP, for Decimal Point)
// void S7C_setSegments(uint8_t segs[])
// {
//   for (uint8_t digit = 0; digit < numDigits; digit++)
//   {
//     digitCodes[digit] = segs[digit];
//   }
// }

// setChars
/******************************************************************************/
// Displays the string on the display, as best as possible.
// Only alphanumeric characters plus '-' and ' ' are supported
void S7C_setChars(char str[])
{
  for (uint8_t digit = 0; digit < numDigits; digit++)
  {
    digitCodes[digit] = 0;
  }

  uint8_t strIdx = 0; // Current position within str[]
  for (uint8_t digitNum = 0; digitNum < numDigits; digitNum++)
  {
    char ch = str[strIdx];
    if (ch == '\0')
      break; // NULL string terminator
    if (ch >= '0' && ch <= '9')
    { // Numerical
      digitCodes[digitNum] = numeralCodes[ch - '0'];
    }
    else if (ch >= 'A' && ch <= 'Z')
    {
      digitCodes[digitNum] = alphaCodes[ch - 'A'];
    }
    else if (ch >= 'a' && ch <= 'z')
    {
      digitCodes[digitNum] = alphaCodes[ch - 'a'];
    }
    else if (ch == ' ')
    {
      digitCodes[digitNum] = digitCodeMap[BLANK_IDX];
    }
    else if (ch == '.')
    {
      digitCodes[digitNum] = digitCodeMap[PERIOD_IDX];
    }
    else if (ch == '*')
    {
      digitCodes[digitNum] = digitCodeMap[ASTERISK_IDX];
    }
    else
    {
      // Every unknown character is shown as a dash
      digitCodes[digitNum] = digitCodeMap[DASH_IDX];
    }

    strIdx++;
    // Peek at next character. It it's a period, add it to this digit
    if (str[strIdx] == '.')
    {
      digitCodes[digitNum] |= digitCodeMap[PERIOD_IDX];
      strIdx++;
    }
  }
}

void S7C_setSymbol(uint8_t digitNum, uint8_t symbol)
{
  digitCodes[digitNum >= numDigits ? numDigits - 1 : digitNum] = symbol;
}

void S7C_setDigit(uint8_t digitNum, uint8_t symbol)
{
  digitCodes[digitNum >= numDigits ? numDigits - 1 : digitNum] = digitCodeMap[symbol];
}

// blank
/******************************************************************************/
void S7C_blank(void)
{
  for (uint8_t digitNum = 0; digitNum < numDigits; digitNum++)
  {
    digitCodes[digitNum] = digitCodeMap[BLANK_IDX];
  }
  S7C_segmentOff(0);
  S7C_digitOff(0);
}

// findDigits
/******************************************************************************/
// Decides what each digit will display.
// Enforces the upper and lower limits on the number to be displayed.

// void S7C_findDigits(long numToShow, uint8_t decPlaces, uint8_t hex, uint8_t digits[])
// {
//   const long *powersOfBase = hex ? powersOf16 : powersOf10;
//   const long maxNum = powersOfBase[numDigits] - 1;
//   const long minNum = -(powersOfBase[numDigits - 1] - 1);

//   // If the number is out of range, just display dashes
//   if (numToShow > maxNum || numToShow < minNum)
//   {
//     for (uint8_t digitNum = 0; digitNum < numDigits; digitNum++)
//     {
//       digits[digitNum] = DASH_IDX;
//     }
//   }
//   else
//   {
//     uint8_t digitNum = 0;

//     // Convert all number to positive values
//     if (numToShow < 0)
//     {
//       digits[0] = DASH_IDX;
//       digitNum = 1; // Skip the first iteration
//       numToShow = -numToShow;
//     }

//     // Find all digits for base's representation, starting with the most
//     // significant digit
//     for (; digitNum < numDigits; digitNum++)
//     {
//       long factor = powersOfBase[numDigits - 1 - digitNum];
//       digits[digitNum] = numToShow / factor;
//       numToShow -= digits[digitNum] * factor;
//     }

//     // Find unnnecessary leading zeros and set them to BLANK
//     if (!leadingZeros)
//     {
//       for (digitNum = 0; digitNum < (numDigits - 1 - decPlaces); digitNum++)
//       {
//         if (digits[digitNum] == 0)
//         {
//           digits[digitNum] = BLANK_IDX;
//         }
//         // Exit once the first non-zero number is encountered
//         else if (digits[digitNum] <= 9)
//         {
//           break;
//         }
//       }
//     }
//   }
// }

// setDigitCodes
/******************************************************************************/
// Sets the 'digitCodes' that are required to display the input numbers

// void S7C_setDigitCodes(uint8_t digits[], uint8_t decPlaces)
// {

//   // Set the digitCode for each digit in the display
//   for (uint8_t digitNum = 0; digitNum < numDigits; digitNum++)
//   {
//     digitCodes[digitNum] = digitCodeMap[digits[digitNum]];
//     // Set the decimal point segment
//     if (digitNum == numDigits - 1 - decPlaces)
//     {
//       digitCodes[digitNum] |= digitCodeMap[PERIOD_IDX];
//     }
//   }
// }

/// END ///
