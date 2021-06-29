/*
 * SparkFun MicroMod Input and Display Calendar
 * 
 * To use this code you will need to provide the SSID and password of the network to connect to,
 * along with the deployment ID for the Google Apps Script. 
 * 
 * After the ESP32 MicroMod Processor board connects to the Internet it will connect to an NTP server
 * and talks to a Google Apps Script to retreive:
 * - Total number of calendar events for the current day
 * - Name, start time, end time of those calendar events
 * - Number of events for the next day
 * - Name, start time, end time for the first calendar event of the next day
 * - Number of unread emails in the inbox
 * 
 * Depending on the display image, it will show the current time, next meeting along with how far
 * away that meeting is, up to 3 future meetins along with their start times, and how many unread
 * emails are in your inbox.
 * 
 * The 5-way switch on the on the Input and Display board can be used to change the display in the 
 * following ways:
 * - Left/Right cycles between showing or hiding the current time
 * - Up/Down changes between 10 backlight brightness levels
 * - Center rotates the display 180 degrees and swaps backlight controls
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>

#include <APA102.h>                   //http://librarymanager/All#APA102 by Pololu
#include <Timezone.h>                 //http://librarymanager/All#Timezone by Jack Christensen, which depends on the TimeLib library from Paul Stoffregen https://github.com/PaulStoffregen/Time
#include <SparkFun_MicroMod_Button.h> //http://librarymanager/All#SparkFun_MicroMod_Button

#include <Adafruit_GFX.h>             //http://librarymanager/All#Adafruit_GFX
#include <Adafruit_ILI9341.h>         //http://librarymanager/All#Adafruit_ILI9341
#include <Fonts/FreeSans9pt7b.h>      //Front are provided in the GFX library
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

const char* ssid = "";
const char* pass = "";
String deploy_id = "";

//TimeChangeRule Structure: {Abbrev (5 Char max), Week (First, Second, Third, Fourth, or Last), DayOfWeek (1=Sun, 2=Mon, etc), Month, hour (0-23), offset (from UTC in minutes)}
// US Mountain Time Zone (Denver)
TimeChangeRule usMDT = {"MDT", Second, dowSunday, Mar, 2, -360}; //Daylight Savings Time rule
TimeChangeRule usMST = {"MST", First, dowSunday, Nov, 2, -420};  //Standard Time rule
Timezone usMT_DST(usMDT, usMST);

#define BACKLIGHT_PIN PWM0
#define TFT_CS_PIN D0
#define TFT_DC_PIN D1
#define LED_DATA_PIN G1
#define LED_CLOCK_PIN G0

WiFiClientSecure client;
char calendarServer[] = "script.google.com";

time_t utc;
time_t prevNow=0;

WiFiUDP Udp;                    //Initialize UDP library
unsigned int localPort = 8888;  // local port to listen for UDP packets

/* Array of ntp servers - I've had troubles in the past reliably connecting to time.gov
 * and using an array of servers seems to work better for me at least.
*/
const char ntpServerName[14][25] = {
  //Maryland
  "time-a-g.nist.gov",
  "time-b-g.nist.gov",
  "time-c-g.nist.gov",
  "time-d-g.nist.gov",
  //Fort Collins
  "time-a-wwv.nist.gov",
  "time-b-wwv.nist.gov",
  "time-c-wwv.nist.gov",
  "time-d-wwv.nist.gov",
  //Boulder
  "time-a-b.nist.gov",
  "time-b-b.nist.gov",
  "time-c-b.nist.gov",
  "time-d-b.nist.gov",
  //University of Colorado, Boulder
  "utcnist.colorado.edu",
  "utcnist2.colorado.edu"
};

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

APA102<LED_DATA_PIN, LED_CLOCK_PIN> ledStrip;
const uint16_t ledCount = 6;
rgb_color colors[ledCount];

MicroModButton button;
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS_PIN,TFT_DC_PIN);

//Data to get from Google
int totalEventsToday = 0;
String calEntToday[10][3];  //calEntToday[Event #][Name,Start Time, End Time]
int totalEventsTomorrow = 0;
String calEntTomorrow[3];   //calEntTomorrow[Name,Start Time, End Time]
int unreadMail = 0;

bool orientation = 0; //0-USB on left side, 1-USB on right side
bool displayType = 1; //0-Don't show the time, 1-Do show the time
int backlight = 9;  //Backlight brightness level (0-9)
double prevTime = 0;

/* Pin button presses and updating display to Core0
 * This allows the display to change without having to wait for responses from Google if
 * the ESP32 is trying to get the latest calendar and mail data.
*/
void Task1code( void * pvParameters )
{
  while(true)
  {
    vTaskDelay(10);

    //Check to see if a button has been pressed
    if(button.getPressed() != 0)
    {
      uint8_t pressed = button.getPressed(); //Read which button has been pressed
      
      if(pressed & 0x04 || pressed & 0x08)
      {
        displayType = !displayType;
        
        EEPROM.write(1,displayType);
        EEPROM.commit();
        
        tft.fillScreen(ILI9341_BLACK);
        updateDisplay(displayType);
      }
      else if(pressed & 0x20) //Change Backlight
      {
        byte pwmVal = 0;

        if(orientation) //orientation = 1
        {
          backlight++;
          if(backlight > 9) backlight = 9;
        }
        else //orientation = 0
        {
          backlight--;
          if(backlight < 0) backlight = 0;
        }

        EEPROM.write(0,backlight);
        EEPROM.commit();
        
        pwmVal = map(backlight,0,9,255,0);
        ledcWrite(1,pwmVal);
      }
      else if(pressed & 0x10) //Change Backlight
      {
        byte pwmVal = 0;
        
        if(orientation) //orientation = 1
        {
          backlight--;
          if(backlight < 0) backlight = 0;
        }
        else //orientation = 0
        {
          backlight++;
          if(backlight > 9) backlight = 9;
        }

        EEPROM.write(0,backlight);
        EEPROM.commit();
        
        pwmVal = map(backlight,0,9,255,0);
        ledcWrite(1,pwmVal);
      }
      else if(pressed & 0x40)
      {
        orientation = !orientation;
        EEPROM.write(2,orientation);
        EEPROM.commit();

        tft.setRotation(orientation*2 + 1); //Sets the display in landscape (1 or 3)
        updateDisplay(displayType);
      }
      
      while(button.getPressed() != 0) { vTaskDelay(10); }
    }

    //Update Display every minute
    if((now()/60 != prevNow/60) && (WiFi.status() == WL_CONNECTED))  
    {
      Serial.println(currentTime());
      updateDisplay(displayType);
  
      time_t t = getLocal();
      int currentTime = hour(t)*60 + minute(t);
      
      for(byte i=0; i<totalEventsToday; i++)
      {
        int startTime = calEntToday[i][1].substring(0,2).toInt()*60 + calEntToday[i][1].substring(3).toInt();
        if(((startTime - currentTime) <= 10) && (startTime >= currentTime))
        {
          if((startTime - currentTime) == 10) displayAlert(0);
          else if((startTime - currentTime) == 5) displayAlert(1);
          else if((startTime - currentTime) == 0) displayAlert(2);
          break;
        }
      }
    }
  }
}

void setup()
{
  //Initialize Communication Buses and EEPROM
  Serial.begin(115200);
  Wire.begin();
  EEPROM.begin(3);

  ledcAttachPin(BACKLIGHT_PIN,1);
  ledcSetup(1,10000,8);
  
  //Read backlight memory value
  backlight = EEPROM.read(0);
  if(backlight > 9) backlight = 9;

  //Set backlight
  ledcWrite(1,map(backlight,0,9,255,0));

  //Read display view memory value
  displayType = EEPROM.read(1);

  //Read orientation of display
  orientation = EEPROM.read(2);
  
  Serial.println("\nMICROMOD GOOGLE CALENDAR Display");

  //Initialize buttons
  button.begin();

  //Initialize display
  tft.begin();
  tft.invertDisplay(true);
  tft.setRotation(orientation*2 + 1); //Sets the display in landscape (1 or 3)
  tft.setTextColor(ILI9341_WHITE);
  tft.fillScreen(ILI9341_BLACK);

  // Boot Screen
  tft.setFont(&FreeSans18pt7b);
  tft.setCursor(8,60);
  tft.print("MicroMod Calendar");

  //Connect to WiFi
  tft.setFont(&FreeSans12pt7b);
  tft.setCursor(0,160);
  tft.print("Status: Connecting to WiFi");
  connectToWifi();

  //Get calendar from Google
  tft.fillRect(0,140,320,30,ILI9341_BLACK);
  tft.setCursor(0,160);
  tft.print("Status: Fetching Calendar");
  tft.print("\n(this can take a little bit)");
  fetchData();

  //Show main display
  updateDisplay(displayType);

  //function to get the current time and sync up every x number of seconds
  setSyncProvider(getNtpTime);
  setSyncInterval(300);
  
  Serial.println(currentTime());
  
  //Pin task function to Core0
  TaskHandle_t Task1;
  xTaskCreatePinnedToCore(
                    Task1code,   /* Function to implement the task */
                    "Task1", /* Name of the task */
                    10000,      /* Stack size in words */
                    NULL,       /* Task input parameter */
                    1,          /* Priority of the task */
                    &Task1,       /* Task handle. */
                    0);  /* Core where the task should run */
  Serial.println("Task created");

  prevTime = millis();
}

void loop() 
{
  //Check for calendar/mail update every minute (60000ms)
  if((millis()-prevTime > 60000) && (WiFi.status() == WL_CONNECTED))
  {
    fetchData();
    prevTime = millis();
  }
}

// Main function that controls what is shown on the display
void updateDisplay(uint8_t display)
{  
  time_t t = getLocal();
  prevNow = now();

  int currentTime = hour(t)*60 + minute(t);
  
  if(displayType == 0)
  {
    tft.fillScreen(ILI9341_BLACK);
    tft.setFont(&FreeSans12pt7b);
    tft.setTextSize(1);
  
    //Print first event of tomorrow
    //This will be over written if there is a meeting before tomorrow
    tft.setCursor(0,60);
    tft.print(totalEventsTomorrow);
    tft.print(" meetings tomorrow.");

    if(totalEventsTomorrow != 0)
    {
      tft.setCursor(0,100);
      tft.print("Up next tomorrow:");
      tft.setCursor(0,130);
      tft.print(calEntTomorrow[0].substring(0,27));
      tft.setCursor(260,130);
      int eventStart = calEntTomorrow[1].substring(0,2).toInt();
      if(eventStart > 12) eventStart -= 12; //Convert from 24hr to 12hr format
      tft.print(eventStart/10);
      tft.print(eventStart%10);
      tft.print(':');
      tft.print(calEntTomorrow[1].substring(3));
    }

    for(byte i=0; i<totalEventsToday; i++)
    {
      int eventStart = calEntToday[i][1].substring(0,2).toInt()*60 + calEntToday[i][1].substring(3).toInt();
      int eventEnd = calEntToday[i][2].substring(0,2).toInt()*60 + calEntToday[i][2].substring(3).toInt();
      
      if(currentTime < (eventEnd-5)) //Give 5min buffer for back to back meetings
      {
        tft.fillScreen(ILI9341_BLACK);
        tft.setFont(&FreeSans12pt7b);
        tft.setTextSize(1);

        //Print next event
        tft.setCursor(0,25);
        tft.print(calEntToday[i][0].substring(0,27));
        if(calEntToday[i][0].length() > 27) tft.print("...");
        
        int timeLeft = eventStart - currentTime;

        if(timeLeft > 0)
        {
          tft.print("\nIn ");
          if(timeLeft > 60)
          {
            tft.print(timeLeft/60);
            tft.print("hr ");
          }
          tft.print(timeLeft%60);
          tft.print("min");
        }
        else { tft.print("\nNow"); }
        
        tft.setFont(&FreeSans9pt7b);
        tft.setTextSize(1);

        //Print up to 3 upcoming events after the next meeting
        for(byte j=0;j<3;j++)
        {
          if( (i+j+1)<totalEventsToday )
          {
            int eventStart = calEntToday[i+j+1][1].substring(0,2).toInt();
            if(eventStart > 12) eventStart -= 12;
            tft.setCursor(0,j*20 + 100);
            tft.print(calEntToday[i+j+1][0].substring(0,25));
            if(calEntToday[i+j+1][0].length() > 25) tft.print("...");
            tft.setCursor(260,j*20 + 100);
            tft.print(eventStart/10);
            tft.print(eventStart%10);
            tft.print(':');
            tft.print(calEntToday[i+j+1][1].substring(3));
          }
        }
        break;
      }
    }
    
    //Print unread mail
    tft.setFont(&FreeSans12pt7b);
    tft.setTextSize(1);
    tft.setCursor(0,230);
    tft.print("Unread Mail:");
    tft.print(unreadMail);
  }
  else
  {
    tft.fillScreen(ILI9341_BLACK);
    tft.setFont(&FreeSansBold24pt7b);
    tft.setTextSize(2);

    //Print current time
    tft.setCursor(40,70);
    tft.print(hourFormat12(t)/10);
    tft.print(hourFormat12(t)%10);
    tft.print(':');
    tft.print(minute(t)/10);
    tft.print(minute(t)%10);
    
    tft.setFont(&FreeSans9pt7b);
    tft.setTextSize(1);

    //Print tomorrow's schedule
    //This will be over written if there is a meeting before tomorrow
    tft.setCursor(0,120);
    tft.print(totalEventsTomorrow);
    tft.print(" meetings tomorrow.");

    if(totalEventsTomorrow != 0)
    {
      tft.setCursor(0,160);
      tft.print("Up next tomorrow:");
      tft.setCursor(0,185);
      tft.print(calEntTomorrow[0].substring(0,35));
      tft.setCursor(270,185);
      int eventStart = calEntTomorrow[1].substring(0,2).toInt();
      if(eventStart > 12) eventStart -= 12;
      tft.print(eventStart/10);
      tft.print(eventStart%10);
      tft.print(':');
      tft.print(calEntTomorrow[1].substring(3));
    }

    for(byte i=0; i<totalEventsToday; i++)
    {
      int eventStart = calEntToday[i][1].substring(0,2).toInt()*60 + calEntToday[i][1].substring(3).toInt();
      int eventEnd = calEntToday[i][2].substring(0,2).toInt()*60 + calEntToday[i][2].substring(3).toInt();
      
      if(currentTime < (eventEnd-5)) //Give 5min buffer for back to back meetings
      {
        tft.fillRect(0,80,320,160,ILI9341_BLACK);

        //Print next event
        tft.setCursor(0,95);
        tft.print(calEntToday[i][0].substring(0,35));
        if(calEntToday[i][0].length() > 30) tft.print("...");
        tft.println();
        
        int timeLeft = eventStart - currentTime;
        if(timeLeft > 0)
        {
          tft.print("In ");
          if(timeLeft >= 60)
          {
            tft.print(timeLeft/60);
            tft.print("hr ");
          }
          tft.print(timeLeft%60);
          tft.print("min");
        }
        else { tft.print("Now"); }
        
        //Print up to 3 upcoming events after the next meeting
        for(byte j=0;j<3;j++)
        {
          if( (i+j+1)<totalEventsToday )
          {
            int eventStart = calEntToday[i+j+1][1].substring(0,2).toInt();
            if(eventStart > 12) eventStart -= 12;
            tft.setCursor(0,j*20 + 155);
            tft.print(calEntToday[i+j+1][0].substring(0,27));
            if(calEntToday[i+j+1][0].length() > 27) tft.print("...");
            tft.setCursor(270,j*20 + 155);
            tft.print(eventStart/10);
            tft.print(eventStart%10);
            tft.print(':');
            tft.print(calEntToday[i+j+1][1].substring(3));
          }
        }
        break;
      }
    }
    
    //Print unread mail
    tft.setFont(&FreeSans12pt7b);
    tft.setTextSize(1);
    tft.setCursor(0,230);
    tft.print("Unread Mail:");
    tft.print(unreadMail);
  }
}

// Connects to the WiFi Station based on the provided SSID and password provided
void connectToWifi()
{
  Serial.print("\nConnecting to wifi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);
  uint32_t startTime = millis();
  bool tryingAlt = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nWiFi connected ");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  Udp.begin(localPort);
}

//Connects to script.google.com to get calendar/mail info
bool fetchData()
{  
  // Getting calendar from your published google script
  String calendarRequest = "https://script.google.com/macros/s/"+ deploy_id +"/exec";
  String outputStr = ""; //HTML response from calendarRequest
  String strData = "";   //Used as a buffer to store smaller bits of outputStr
  int indexFrom = 0;
  int indexTo   = 0;  
  int attempts = 0;

  //Sometimes the request doesn't return any data, so we'll retry a few times
  while(outputStr.indexOf("Total Events Today:") == -1 && attempts < 10)
  {
    // If redirected - then follow redirect - google always redirect to a temporary URL by default.
    if(outputStr.indexOf("Location:") > 0 )
    {
      int indexFrom = outputStr.indexOf("Location:") + 10;
      int indexTo = outputStr.indexOf("\n", indexFrom);
      String newUrl = outputStr.substring(indexFrom, indexTo);

      getRequest(calendarServer, newUrl);
      outputStr = client.readString();
    }
    else
    {
      getRequest(calendarServer, calendarRequest);
      outputStr = client.readString();
    }
    attempts++;
  }

  if(outputStr.indexOf("Total Events Today:") > 0)
  {
    Serial.println("Received data");

    //Get the number of unread messages in Email
    indexFrom = outputStr.indexOf("Unread Mail:");
    strData = outputStr.substring(indexFrom);
    indexTo = strData.indexOf('\n');
    unreadMail = strData.substring(12,indexTo).toInt();
  
    //Get the number of calendar events for tomorrow
    indexFrom = outputStr.indexOf("Total Events Tomorrow:");
    strData = outputStr.substring(indexFrom);
    indexTo = strData.indexOf('\n');
    totalEventsTomorrow = strData.substring(22,indexTo).toInt();
  
    //Get the name of the first event tomorrow
    indexFrom = strData.indexOf("\n");
    indexTo = strData.indexOf(";");
    
    for(byte i=0; i<3; i++)
    {
      calEntTomorrow[i] = strData.substring(indexFrom+1,indexTo);
     
      indexFrom = strData.indexOf(";",indexTo);
      indexTo = strData.indexOf(";",indexFrom+1);
    }
  
    //Get the number of events for today
    indexFrom = outputStr.indexOf("Total Events Today:");
    strData = outputStr.substring(indexFrom);
    indexTo = strData.indexOf('\n');
    totalEventsToday = strData.substring(19,indexTo).toInt();
  
    //Get the names of the events for today
    indexFrom = 0;
    indexTo = 1;
  
    for(byte i=0; i<totalEventsToday; i++)
    {
      indexFrom = strData.indexOf("\n",indexTo-1);
      indexTo = strData.indexOf(";",indexFrom);
      for(byte j=0; j<3; j++)
      {
        calEntToday[i][j] = strData.substring(indexFrom+1,indexTo);
        if(j != 2)
        {
          indexFrom = strData.indexOf(";",indexTo);
          indexTo = strData.indexOf(";",indexFrom+1);
        }
      }
    }
  }
  else { Serial.println("Unable to get data"); }

  client.stop();  
  return true;
}
 
// Generic code for getting requests
bool getRequest(char *urlServer, String urlRequest)
{
  client.stop(); // close connection before sending a new request
 
  if (client.connect(urlServer, 443)) //If the connection succeeds
  {
    Serial.println("connecting...");
    // send the HTTP PUT request:
    client.println("GET " + urlRequest); // " HTTP/1.1"
    Serial.println("GET " + urlRequest);
    client.println("User-Agent: ESP32 MicroMod/1.1");
    client.println();
    
    unsigned long timeout = millis();
    while (client.available() == 0) 
    {      
      if (millis() - timeout > 5000) 
      {
        Serial.println(">>> Client Timeout !");
        client.stop();
        Serial.println("Connection timeout");  
        return false;
      }
    }
    Serial.print("Response Time:");
    Serial.println(millis()-timeout);
  } 
  else { Serial.println(F("Calendar connection did not succeed")); }  
  return true;
}

// Controls the APA102 LEDs based on the alert level
// 0-Green (10min) 1-Yellow (5min) 2-Red (0min)
void displayAlert(byte level)
{
  rgb_color alert_color;
  byte brightness = 5;
  
  if(level == 0) alert_color = rgb_color(0,170,0);        //Green
  else if(level == 1) alert_color = rgb_color(255,170,0); //Yellow
  else if(level == 2) alert_color = rgb_color(255,0,0);   //Red
  
  for(uint16_t i = 0; i < ledCount; i++)
  {
    colors[i] = alert_color;
  }
  ledStrip.write(colors, ledCount, brightness);
  delay(100);
  for(uint16_t i = 0; i < ledCount; i++)
  {
    colors[i] = rgb_color(0,0,0);
  }
  ledStrip.write(colors, ledCount, brightness);
  delay(100);
  for(uint16_t i = 0; i < ledCount; i++)
  {
    colors[i] = alert_color;
  }
  ledStrip.write(colors, ledCount, brightness);
  delay(100);
  for(uint16_t i = 0; i < ledCount; i++)
  {
    colors[i] = rgb_color(0,0,0);
  }
  ledStrip.write(colors, ledCount, brightness);
  delay(500);
  for(uint16_t i = 0; i < ledCount; i++)
  {
    colors[i] = alert_color;
  }
  ledStrip.write(colors, ledCount, brightness);
  delay(100);
  for(uint16_t i = 0; i < ledCount; i++)
  {
    colors[i] = rgb_color(0,0,0);
  }
  ledStrip.write(colors, ledCount, brightness);
  delay(100);
  for(uint16_t i = 0; i < ledCount; i++)
  {
    colors[i] = alert_color;
  }
  ledStrip.write(colors, ledCount, brightness);
  delay(100);
  for(uint16_t i = 0; i < ledCount; i++)
  {
    colors[i] = rgb_color(0,0,0);
  }
  ledStrip.write(colors, ledCount, brightness);
}

time_t getNtpTime()
{
  bool goodTimeReturned = 0;
  if(WiFi.status() != WL_CONNECTED) Serial.println("Not Connected to WiFi");
  
  for(byte i=0; i<14; i++)
  {
    IPAddress ntpServerIP;
  
    while (Udp.parsePacket() > 0) ; // discard any previously received packets
    
    WiFi.hostByName(ntpServerName[i], ntpServerIP);
    sendNTPpacket(ntpServerIP);
    
    uint32_t beginWait = millis();
    while ((millis() - beginWait) < 1000) {
      int size = Udp.parsePacket();
      if (size >= NTP_PACKET_SIZE) {
        Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
        unsigned long secsSince1900;
        // convert four bytes starting at location 40 to a long integer
        secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
        secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
        secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
        secsSince1900 |= (unsigned long)packetBuffer[43];
  
        utc = secsSince1900 - 2208988800UL;
        goodTimeReturned = 1;
        break;
      }
    }
    if(goodTimeReturned) break;
  }
  if(!goodTimeReturned)Serial.println("No response from any server");
  return utc;
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

time_t getLocal()
{
  Timezone tz = usMT_DST;  
  TimeChangeRule *tcr;
  return tz.toLocal(now(),&tcr);
}

String currentTime()
{
  time_t t = getLocal();
  
  String timeNow = "";
  timeNow += String(hourFormat12(t)/10);
  timeNow += String(hourFormat12(t)%10);
  timeNow += ':';
  timeNow += String(minute(t)/10);
  timeNow += String(minute(t)%10);
  timeNow += ':';
  timeNow += String(second(t)/10);
  timeNow += String(second(t)%10);
  return timeNow;
}