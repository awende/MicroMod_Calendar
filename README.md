# MicroMod_Calendar

========================================

![MicroMod Input and Display Calendar](https://github.com/awende/MicroMod_Calendar/blob/main/images/Input%20and%20Display%20Calendar.JPG)

The MicroMod Calendar uses the SparkFun Input and Display MicroMod Carrier board, paired with the SparkFun ESP32 Processor board to display your Google calendar. To get this project working, the first thing to do is to add the Google Apps Script to your script library, which just needs the name of your calendar, i.e. "Work" or "Your Name". After the script is deployed, which you can read more here, head over to your Arduino IDE, install a couple of libraries, enter your unique information for your Wi-Fi network, script ID, and timezone information. Hit upload and that's it!

With the Input and Display board, you can use the 5-way switch to switch between the two display views, and adjust the brightness of backlight. The MicroMod Calendar connects to your Wi-Fi network, and then gets the following:

* Current time
* Up to 10 meetings for the current day (choses somewhat arbirarily) with their start and end times.
* The first meeting for the following day with it's start and end time - which can help you plan for the next day.
* The unread email count from your inbox

This information is displayed in one of two views. The default view shows the current time, and will automatically adjust for daylight savings. Both views will show up to four upcoming meetings, with the next meeting telling you how long until it starts, and the others displaying their start times. Lastly, at the bottom it shows your current unread email count. If you're the type of person that has hundreds of unread emails, this might not be terribly useful, but the Google Apps Script could be pretty easily modified to only show you important messages.

Your computer can send you alerts to let you know that you have a meeting coming up, but with APA102 LEDs on the board, it will also give you alerts 10 minutes, 5 minutes, and when the meeting starts by flashing the colors to green, yellow, and red alerts to get your attention a little more easily.

Repository Contents
-------------------
* **/Case Files** - .stl files for the case, along with button caps
* **/Firmware** - Arduino code for the ESP32 Micromod, along with the script that needs to be connected to your Google account.

Hardware Used
-------------------
* SparkFun ESP32 MicroMod Processor Board
* SparkFun MicroMod Input and Display Carrier Board