# MicroMod Calendar

![MicroMod Input and Display Calendar Default View](https://github.com/awende/MicroMod_Calendar/blob/main/images/Input%20and%20Display%20Calendar%20View1.jpg)

The MicroMod Calendar uses the SparkFun Input and Display MicroMod Carrier board, paired with the SparkFun ESP32 MicroMod Processor board to show the current time, display next and upcoming meetings, and provide visual alerts using the on-board LEDs. The 5-way switch changes the backlight level, screen orientation, and a secondary view, which provides a little more space between your next meeting and future meetings.

![MicroMod Input and Display Calendar Alternate View](https://github.com/awende/MicroMod_Calendar/blob/main/images/Input%20and%20Display%20Calendar%20View0.jpg)

To get this project working, there are two sets of code needed. The first is a script which is added to your Google account, and creates a file which pulls in the meetings from your calendar, and checks to see how many unread emails there are. The second runs on the ESP32, to connect to the Internet to read the data in, and get the current time which can automatically adjust for Dalight Savings based on the the timezone information provided.

Repository Contents
-------------------
* **/Case Files** - .stl files for the case, along with button caps
* **/Firmware** - Arduino code for the ESP32 Micromod, along with the script that needs to be connected to your Google account.

Hardware Used
-------------------
* [SparkFun ESP32 MicroMod Processor Board](https://www.sparkfun.com/products/16781)
* [SparkFun MicroMod Input and Display Carrier Board](https://www.sparkfun.com/products/16985)