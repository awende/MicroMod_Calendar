# MicroMod Calendar Firmware

To display your calendar on the SparkFun Input and Display Carrier board, you will need of course the firmware for the ESP32, but also a script that will arregrate the data from your Google account to a single file. The ESP32 then reads the data, and parses it to show on the display. Each peice of firmware goes into more deail below.

========================================
## Google Apps Script

There are a few items that the ESP32 needs to access from your Google account:

* Total number of unread emails in your Inbox
* Total number of meetings for the current day
* Name, start time, and end time of each meeting
* Total number of meetings for the next day
* Name, state time, and end time of your first meeting for the next day

In order of the ESP32 to read this information, we'll need to add the script from the CalendarEvents.gs file. To add the script, head over to https://script.google.com and click on new project.

![New Project](https://github.com/awende/MicroMod_Calendar/blob/main/images/Script%20New%20Project.jpg)

From there, add a name for the project. I just called mine "CalendarEvents". Now we can add the code from CalendarEvents.gs file by pasting it into the script editor. Find the variable `_calendarName`, and inside the quotes, add the name of your calendar to add, e.g. 'Work', or your email address. We can test to make sure everything is working by pressing save and then pressing the run button.

![Save and Run Icons](https://github.com/awende/MicroMod_Calendar/blob/main/images/Script%20save%20and%20run.jpg)

The first time you do this, a window will pop up saying "Authorizaton required". After allowing the script to access your Gmail and Calendar apps, the script will run and an execution log will pop up from the bototm of the screen which should show all of the information outlined above.

![Script Test Run Text Output](https://github.com/awende/MicroMod_Calendar/blob/main/images/Script%20test%20run%20text%20output.jpg)

By default, to get the number of unread emails, the script searches for `is:unread label:all AND NOT label:Spam`. If there are specific unread emails, you can modify the search operators as outlined [here](https://support.google.com/mail/answer/7190?hl=en). Once the scipt is working as expected, we need to let the ESP32 get access to the data. To do that the script needs to be deployed by clicking on "Deploy" -> New deployment. 

![New Deployment](https://github.com/awende/MicroMod_Calendar/blob/main/images/Script%20New%20Deployment.jpg)

Click on the gear icon and select Web app as your deployment type. You can provide a description if you want, but under web app, we want to make sure to have your email selected for "execute as", and for "Who has access", select anyone because the ESP32 won't be signing into Google. The last step is to click on deploy, which will generate the Deployment ID that the ESP32 uses.

If you make any changes to the script after deploying, you will have to save the changes and either create a new deployment, which will create a new ID, or manage deployment and create a new version which will use the same ID.

## ESP32 Firmware

Before uploading code to the ESP32, there are a few things to check. The first is to make sure you have all of the libraries installed. Most come with the package install of the ESP32 core, but the ones that need to be manually added are:

* [APA102.h from Pololu](https://github.com/pololu/apa102-arduino)
* [Timezone.h from Jack Christensen](https://github.com/JChristensen/Timezone)
* [TimeLib from Paul Stoffregen](https://github.com/PaulStoffregen/Time)
* [SparkFun MicroMod Button from SparkFun](https://github.com/sparkfun/SparkFun_MicroMod_Button_Arduino_Library)
* [GFX](https://github.com/adafruit/Adafruit-GFX-Library) and [ILI9341](https://github.com/adafruit/Adafruit_ILI9341) from Adafruit

Aside from the TimeLib library, each library can be found in Arduino's Library manager. For the TimeLib, head over to Paul's github page and download the library, and add the .zip to your library.

Once all of that is done, enter the network credentials for your WiFi network, and paste in the deployment ID from earier. To make sure you get the alerts on time, update the timezone information for your area. US Mountain Time Zone is there for a reference to help fill it in correctly. If your area doesn't do daylight savings you can just fill in standard time rule, and use that rule for both inputs for the timezone.

And that should be it, you can upload the code to your board. Once the board finishes uploading, it will reboot and try to connect to your Wi-Fi network to get the current time and get your calendar information. Every minute it will check the calendar to see if anything changed. It can take a little bit of time to get the information from your calendar, so to try and keep the user interface smooth, Core0 of the ESP32 will check for button presses from the switches, and update the diplay every minute, even if the other core is checking for any updates from the calendar.


