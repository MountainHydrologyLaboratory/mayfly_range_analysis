//Code to measure Stage, record in SD card and display in real time. //

#include <Arduino.h>
#include <SoftwareSerial_PCINT12.h>
#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <Sodaq_DS3231.h> //RTC
#include <SDL_Arduino_SSD1306.h>    // For display - Modification of Adafruit_SSD1306 for ESP8266 compatibility
#include <AMAdafruit_GFX.h>   // For display - Needs a little change in original Adafruit library (See README.txt file)
SDL_Arduino_SSD1306 display(4); // FOR I2C

// ---------------------------------------------------------------------------
// Board setup info
// ---------------------------------------------------------------------------
const long SERIAL_BAUD = 57600;  // Serial port baud rate
const int GREEN_LED = 8;  // Pin for the green LED
const int RED_LED = 9;  // Pin for the red LED
const int BUTTON_PIN = 21;  // Pin for the button
const int RTC_PIN = A7;  // RTC Interrupt/Alarm pin
const int SD_SS_PIN = 12;  // SD Card Chip Select/Slave Select Pin

// ---------------------------------------------------------------------------
// RTC
// ---------------------------------------------------------------------------


/*  code to process time sync messages from the serial port   */
#define TIME_HEADER  'T'   // Header tag for serial time sync message

String getDateTime()
{
  String dateTimeStr;

  //Create a DateTime object from the current time
  DateTime dt(rtc.makeDateTime(rtc.now().getEpoch()));

  //Convert it to a String
  dt.addToString(dateTimeStr);

  return dateTimeStr;
}
// ---------------------------------------------------------------------------
// Sonar sensor
// ---------------------------------------------------------------------------

// Change to the proper excite (power) and receive pin for MaxBotix Sonar.
int _dataPin = 11;
int _powerPin = 22;    // sensor power is pin 22 on Mayfly
int _triggerPin = -1;    // sensor power is pin 22 on Mayfly
bool useTrigger = false;
const int MAX_INPUT = 30;

// define serial port for receiving data
// output from maxSonar is inverted requiring true to be set.
SoftwareSerial sonarSerial(_dataPin, -1);


// here to process incoming serial data after a terminator received
void process_data (const char * data)
{
  // for now just display it
  // (but you could compare it to some value, convert to an integer, etc.)
  Serial.println(data);
}  // end of process_data

void processIncomingByte (const byte inByte)
{
  static char input_line [MAX_INPUT];
  static unsigned int input_pos = 0;

  switch (inByte)
  {

    case '\r':   // end of text
      input_line [input_pos] = 0;  // terminating null byte

      // terminator reached! process input_line here ...
      process_data (input_line);

      // reset buffer for next time
      input_pos = 0;
      break;

    case '\n':   // discard carriage return
      break;

    default:
      // keep adding if not full ... allow for terminating null byte
      if (input_pos < (MAX_INPUT - 1))
        input_line [input_pos++] = inByte;
      break;

  }  // end of switch

} // end of processIncomingByte


int SonarRead_char(void)
{
  // Sonar sends a result just above it's max range when it gets a bad reading
  // For 10m models, this is 9999, for 5m models it's 4999
  int badResult = 9999;
  int result = badResult;  // initialize the result with a bad reading
  char inData[5];  // char array to read data into
  int index = 0;
  bool stringComplete = false;
  int rangeAttempts = 0;

  Serial.println(F("Beginning detection for Sonar"));  // debug line
  sonarSerial.flush();  // Clear cache ready for next reading
  while (stringComplete == false)
  {
    while (sonarSerial.available())
    {
      delay(3);  // It just works better with this delay.  4 is too much, 2 is too little.
      char rByte = sonarSerial.read();  //read serial input for "R" to mark start of data
      if (rByte == 'R')
      {
        Serial.println(F("'R' Byte found, reading next 4 characters:")); // Debug line
        while (index < 4)  //read next three character for range from sensor
        {
          if (sonarSerial.available())
          {
            inData[index] = sonarSerial.read();
            Serial.print(inData[index]);  // Debug line
            index++;  // Increment where to write next
          }
        }
        inData[index] = 0x00;  //add a padding byte at end for atoi() function
        Serial.println();  // Debug line
      }
      rByte = 0;  // Reset the rByte ready for next reading
      index = 0;  // Reset index ready for next reading

      // Make sure R is not part of the header, part number, or RoHS warning line
      // ie, "HRXL-MaxSonar-WRL" or "RoHS 1.8b078  0713"
      if (inData[0] == 0) {}
      else if (inData[1] != 'X' && inData[1] != 'L' && inData[1] != 'S' &&
               inData[1] != 'o' && inData[1] != 'H' && inData[1] != '\r')
      {
        stringComplete = true;  // Set completion of read to true
        result = atoi(inData);  // Changes string data into an integer for use
        memset(&inData[0], 0, sizeof(inData));  // Empty the inData array.
        if ((result == 300 || result == 500 || result == 4999 || result == 9999)
            && rangeAttempts < 30)
        {
          result = badResult;
          stringComplete = false;
          rangeAttempts++;
          Serial.print(F("Bad or Suspicious Result, Retry Attempt #")); // Debug line
          Serial.println(rangeAttempts); // Debug line
        }
      }
      else
        Serial.println(F("Ignoring header line")); // Debug line
      memset(&inData[0], 0, sizeof(inData));  // Empty the inData array.

    }
  }
  sonarSerial.flush();  // Clear cache ready for next reading
  return result;
}

// ---------------------------------------------------------------------------
// SD card:
// ---------------------------------------------------------------------------

#define FILE_BASE_NAME "Sonar"

File myFile;

const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
char fileName[] = FILE_BASE_NAME "00.txt";

const int chipSelect = 12; //SD card chip select

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup()
{
  Wire.begin();
  pinMode(_powerPin, OUTPUT);
  pinMode(_dataPin, INPUT);
  digitalWrite(_powerPin, LOW);
  if (useTrigger)
  {
    pinMode(_triggerPin, OUTPUT);
    digitalWrite(_triggerPin, LOW);
  }
  else
  {
    pinMode(_triggerPin, OUTPUT);
    digitalWrite(_triggerPin, HIGH);
  }

  Serial.begin(SERIAL_BAUD);
  sonarSerial.begin(9600);

  //display stuff:
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false);  // initialize with the I2C addr 0x3C (for the 128x64)
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Mayfly");
  display.println("Sonar Sensor");
  display.display();
  
  rtc.begin();
  // Set up pins for the LED's
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

//  //sync RTC time if connected to PC:
  if (Serial.available())
  {
    //Sync time
    syncRTCwithBatch();
  }
  // Empty the serial buffer
  while (Serial.available() > 0)
  {
    Serial.read();
  }

  //--------------------------------------------------------
  //SD card initialization, creating file with incremented name:
  //--------------------------------------------------------

  Serial.print("Initializing SD card...");
  pinMode(chipSelect, OUTPUT);

  if (!SD.begin(chipSelect))
  {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");


  while (SD.exists(fileName))
  {
    if (fileName[BASE_NAME_SIZE + 1] != '9')
    {
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9')
    {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } else {
      Serial.println(F("Can't create file name"));
      return;
    }
  }
  
}

// ---------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------
void loop()
{

  Serial.println(F("-------------------"));  // For debugging

  digitalWrite(GREEN_LED, HIGH); //Turn on LED to show we're taking a reading
  digitalWrite(_powerPin, HIGH);
 // delay(200);  // Published start-up time is 160ms;

  DateTime now = rtc.now();
  uint32_t ts = now.getEpoch();
  String date_time = (String(now.month()) + '-' + String(now.date()) + '-' + String(now.year()) + ' ' + String(now.hour()) + (':') + String(now.minute()) + (':') + String(now.second()));


  readMaxbotixHeader(); //sonar sensor debug information to ensure that it's attached properly
  int sonar_data = (readMaxbotixData()); //read data from sonar sensor
  digitalWrite(_powerPin, LOW);
  digitalWrite(GREEN_LED, LOW); //Turn off LED once reading is finished

  Serial.println(date_time); Serial.print("Sonar Data: "); Serial.print(sonar_data); Serial.println(" mm");
int reading = analogRead(A6);
float voltage = reading*3.3/pow(2,10);
float battery = voltage/10*(4.7+10);
Serial.println(battery);
rtc.convertTemperature();             //convert current temperature into registers
    Serial.print(rtc.getTemperature()); //read registers and display the temperature
    Serial.println("deg C");

  myFile = SD.open(fileName, FILE_WRITE);
  if (!myFile)
  {
    Serial.println(F("open failed"));
    return;
  }
  myFile.print(date_time); myFile.print(","); myFile.print(battery); myFile.print(",");myFile.print(sonar_data); myFile.println();

  myFile.close();

  update_display(sonar_data, date_time, battery);

  sonar_data = 0; //clear

  Serial.println(F("------------------\n"));  // For debugging


  delay(299000); //delay between samples
}

// ---------------------------------------------------------------------------
// Update the display
// ---------------------------------------------------------------------------
int update_display(int x, String y, float z)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(y);
  display.setTextSize(2);
  display.print("Range:");
  display.print(x);
  display.println("mm");
  display.print("Volt:");
  display.print(z);
  display.print("v");
  display.display();
}

// ---------------------------------------------------------------------------
// Read sonar header - for debugging
// ---------------------------------------------------------------------------
void readMaxbotixHeader(void)
{
  Serial.println(F("Parsing Header Lines"));  // For debugging
  while (sonarSerial.available())
  {
    Serial.println(sonarSerial.readStringUntil('\r'));
    // sonarSerial.readStringUntil('\r');
  }
  Serial.println(F("  -----  "));  // For debugging
}

// ---------------------------------------------------------------------------
// Read data from sonar
// ---------------------------------------------------------------------------
int readMaxbotixData()
{
  // Sonar sends a result just above it's max range when it gets a bad reading
  // For 10m models, this is 9999, for 5m models it's 4999
  int badResult = 9999;
  int result = badResult;  // initialize the result with a bad reading
  bool stringComplete = false;
  int rangeAttempts = 0;

  Serial.println(F("Beginning detection for Sonar"));  // For debugging
  while (stringComplete == false && rangeAttempts < 50)
  {
    if (useTrigger)
    {
      Serial.println(F("Triggering Sonar"));  // For debugging
      digitalWrite(_triggerPin, HIGH);
      delay(1);
      digitalWrite(_triggerPin, LOW);
      delay(160);  // Published return time is 158ms
    }

    result = sonarSerial.parseInt();
    sonarSerial.read();  // To throw away the carriage return
    //Serial.println(result);  // For debugging
    //sonar_data = result;
    rangeAttempts++;

    if (result == 0 || result == 300 || result == 500 || result == 4999 || result == 9999)
    {
      result = badResult;
      Serial.print(F("Bad or Suspicious Result, Retry Attempt #"));  // For debugging
      Serial.println(rangeAttempts);  // For debugging
    }
    else
    {
      Serial.println(F("Good result found"));  // For debugging
      stringComplete = true;  // Set completion of read to true
    }
  }
  return result;
}



// ---------------------------------------------------------------------------
// RTC time sync
// ---------------------------------------------------------------------------
void syncRTCwithBatch() //sync computer's time to RTC
{
  // Read the timestamp from the PC's batch program
  uint32_t newTs = processSyncMessage();

  if (newTs > 0)
  {
    //Serial.println(newTs);

    // Add the timezone difference plus a few seconds
    // to compensate for transmission and processing delay
    //newTs += SYNC_DELAY + TIME_ZONE_SEC;

    //Get the old time stamp and print out difference in times
    uint32_t oldTs = rtc.now().getEpoch();
    int32_t diffTs = newTs - oldTs;
    int32_t diffTs_abs = abs(diffTs);
    Serial.println("RTC is Off by " + String(diffTs_abs) + " seconds");

    //Display old and new time stamps
    Serial.print("Updating RTC, old = " + String(oldTs));
    Serial.println(" new = " + String(newTs));

    //Update the rtc
    rtc.setEpoch(newTs);
  }
}

unsigned long processSyncMessage() {
  unsigned long pctime = 0L;
  const unsigned long DEFAULT_TIME = 1451606400; // Jan 1 2016 00:00:00.000
  const unsigned long MAX_TIME = 2713910400; // Jan 1 2056 00:00:00.000

  if (Serial.find(TIME_HEADER)) {
    pctime = Serial.parseInt();
    Serial.println("Received:" + String(pctime));
    if ( pctime < DEFAULT_TIME) // check the value is a valid time (greater than Jan 1 2016)
    {
      Serial.println("Time out of range");
      pctime = 0L; // return 0 to indicate that the time is not valid
    }
    if ( pctime > MAX_TIME) // check the value is a valid time (greater than Jan 1 2016)
    {
      Serial.println("Time out of range");
      pctime = 0L; // return 0 to indicate that the time is not valid
    }
  }
  return pctime;
}
