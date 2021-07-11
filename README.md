# ESP32 Garage Door Controller

ESP32 Garage Door Controller web site

07/11/2021 Ed Williams 

I used an ESP32-CAM board to make a garage door controller web site that requires no camera 
and no SD card. The web page fits nicely as a saved web page on my Smartphone. The site can
open or close the garage door, shows history for the last 10 times the door was active, can
be configured to send email notices of open or close activity, and can set a time to auto
close the garage door if it is open. (For when I forget to close it at the end of a day.)
It also includes a password protected Over-The-Air firmware update web page which includes 
the option to erase EEPROM/Preferences.

Visit the web site at <IP>/GDC for the main page and <IP>/updatefirmware and enter the 
password to upload new firmware to the ESP32. Here is a diagram of how to wire it up:

![ESP32GarageDoorController](https://github.com/mtnbkr88/ESP32GarageDoorController/blob/master/ESP32GarageDoorControllerSchematic.png)

The reed switch is placed on the garage door frame above the middle. The magnet to activate 
the reed switch is attached to the top of the garage door so the reed switch closes when the
garage door is closed.

The two wires to the garage door button are attached to the same screws on the garage door
motor that the button next to the door into the house is attached.
