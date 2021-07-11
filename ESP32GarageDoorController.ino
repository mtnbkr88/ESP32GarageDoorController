/**********************************************************************************
 * 07/11/2021 Edward Williams
 * This sketch provides a web page to control my garage door.
 * It includes the ability to send emails on open/close events
 * and do an auto close at a specific time.
 * Connect the ESP32-CAM as shown in the accompanying diagram.
 * GPIO2 should be connected to the garage door relay pin IN2.
 * GPIO14 should be connected to the garage door relay pin IN1.
 * GPIO13 should be connected to the garage door reed sensor.
 **********************************************************************************/

/**********************************************************************************
 * 06/21/2020 Edward Williams
 * This is a shell which includes an Over-The-Air firmware update web server which
 * includes the option to erase EEPROM, fixed IP address, a major fail flashing led 
 * notice with sleep reboot, time set and mount of SD card. I use this as a starting 
 * point for all my sketches. Visit the web site <IP>/updatefirmware and enter the 
 * password to upload new firmware to the ESP32. Compile using the "Default" Partition 
 * Scheme.
 **********************************************************************************/
 
// edit the below for local settings 

// wifi info
const char* ssid = "YourSSID";
const char* password = "YourSSIDPwd";
// fixed IP info
const uint8_t IP_Address[4] = {192, 168, 2, 32};
const uint8_t IP_Gateway[4] = {192, 168, 2, 1};
const uint8_t IP_Subnet[4] = {255, 255, 255, 0};
const uint8_t IP_PrimaryDNS[4] = {8, 8, 8, 8};
const uint8_t IP_SecondaryDNS[4] = {8, 8, 4, 4};

const char* TZ_INFO = "PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00";

const int SERVER_PORT = 80;  // port the main web server will listen on

const char* appName = "ESP32GarageDoorController";
const char* appVersion = "1.0.0";
const char* firmwareUpdatePassword = "87654321";

// edit email server info 
const char* emailhost = "smtp.gmail.com";
const int emailport = 465;
const char* emailsendaddr = "YourEmail\@gmail.com";
const char* emailsendpwd = "YourEmailPwd";
char email[40] = "YourNotificationEmail\@hotmail.com";  // this is notification emails go

// should not need to edit the below

#include "esp_http_server.h"
httpd_handle_t webserver_httpd = NULL;   

#include <WiFi.h>

WiFiEventId_t eventID;

#include "soc/soc.h"  //disable brownout problems
#include "soc/rtc_cntl_reg.h"  //disable brownout problems

#include "time.h"
#include "lwip/err.h"
#include "lwip/apps/sntp.h"
struct tm timeinfo;
time_t now;

#include <EEPROM.h>  // for erasing EEPROM ATO, assumes EEPROM is 512 bytes in size
#include <Update.h>  // for flashing new firmware

#define uS_TO_S_FACTOR 1000000LL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP5S  5        // Time ESP32 will go to sleep (in seconds)

#include <Preferences.h>
Preferences preferences;

// GDC variables
char GDStatus[20];
char SensorStatus[20];
char GDDate[30];
char GDHistory[10][100];
const char* ClosedPng = "<img src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEwAAABMCAYAAADHl1ErAAAB2UlEQVR42u3cParCQBSG4QELSSG4BSshtWDhLtJY27gQSy3EwsJGXIUbkBQW4iIsREQr++OdQMSrXhMlP2cur/AVVmMe5psYkWPM3etyucj5fJbT6UR+Yi2siXn1Auo9HFjfotktB0i6RPVkd324y4D4LIABBhhggAFGAAMMMMAAI4ABBhhgiZnP5zIejwFLyn6/l16vZ38KjhIEgex2O8BeZbPZiO/7N6w4jUZDVqsVYI8VrNVqT1hxqtWqmooaTRVMioaKGm0VTErZFTUaK5iUMitqNFdQY0VNkRVstVqZYcVpNpuFVrQQsMViIfV6PXOsOJ7nyWQycR/MVrDf7+cG9Zhut5t7RY1rFSy7osbFCpZZUeNyBcuoqHG9gmkqGoahPrDtdhtVQRuY/Uzr9VpnJYfDoTqwwWCg9ww7Ho/S6XTUYLXbbTkcDrrvknE1K5WKjEajXxeQ263+bg37nGrXz7qKuX6tmM1mslwuny6mCDD73h7y0+nUzUejV2BZVe7dGs4+SwIGGGDqwVxcAzAqCRhggAHGoQ8YleQMAwwwwPhTMGCAAQYYYAQwwAADDDACGGCAAfZPwBjplz7RSD+GRqbPbaYruyzl7mKW65dYjFb+G+pxtPIV8/HiUHpI7zMAAAAASUVORK5CYII='/>";
const char* ClosingPng = "<img src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEwAAABMCAYAAADHl1ErAAACCUlEQVR42u3cParCQBiF4QELsRDcgpVgLYi4AWsbaxsXYqmFWFjYiKtwA2JhIS7CQkS0sp97JxDxRmMmkpgvd97AKazGPMyZ/BBGqYfjdrvp6/WqL5cL+Y2xMCbq1QHUeziwPkUzUw4Qu3j1ZHbFnGVAxAtggAEGGGCAEcAAAwwwwAhggAEGWGQWi4WeTCaAReV4POp+v29eBXvpdrv6cDgA9iq73U7X6/U7lp9qtarX6zVgwQqWy+UnLD/FYlFMRZWkCkZFQkWVtApGJeuKKokVjEqWFVWSKyixouqbFWw0Golh+anVal+t6FfAlsulrlQqiWP5KZVKejqd5h/MVHAwGKQGFUyv10u9oipvFcy6oiqPFcyyoirPFcyioirvFbSp6GazkQe23++9KkgDM/9pu93KrORoNBIHNhwO5a5h5/NZt9ttMVjNZlOfTifZV0m/moVCQY/H4z8nkNql/mEM85xqxk+6iqneVsznc71arZ5O5htg5rdZ5GezWT4fjeKAhVUryTEAAwwwwAADDLB/CWbebnQ6Hes791ar9fLtg1MzzBYtDMvJSkahvcNydg0LQ4vCcnrRD6LZYDkN9ohmi+U8mI8W512882ASxwCMj4L5ihowwAAjgAEGGGCAEcAAAwwwwJwEY0s/+3hb+rFppH3ue7oyyyxnF3u5fojF1srhUMGtlX8AM0xYzHigrfkAAAAASUVORK5CYII='/>";
const char* OpenPng = "<img src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEwAAABMCAYAAADHl1ErAAAB0ElEQVR42u3cMarCQBSF4QELsRDcgpVgHbBwFzbWNi4kpRbBwsImuAo3IBYW4iIsRIJW9ve9CUR8UV+imOQO/IFTWMV8zEkcCdeYu+N6vcrlcpHz+Ux+Yy2siXl2APU/HFifotklB0i+xPVkdb25yoB4L4ABBhhggAFGAAMMMMAAI4ABBhhgmQnDUIIgACwrx+NRRqOR/Ss4zmAwkMPhANiz7HY76Xa7N6wk7XZb1us1YOkKNpvNB6wk9XpdTUWNpgpmRUNFjbYKZqXqihqNFcxKlRU1miuosaKmzAp6nvc1rCSdTqfUipYCtlwupdVqfR0rSaPRkNls5j6YreB4PC4MKp3hcFh4RY1rFay6osbFClZZUeNyBauoqHG9gnkqutls9IHt9/u4CtrA7Hfabrc6KzmZTNSB+b6v9x4WRZH0+301WL1eT06nk+6nZFLNWq0m0+n0zwUU9qi/O4fdp9rzf7uKhf6sWCwWslqtHi6mDDD72d7k5/O5m1ujKsCc3ksCBhhggAEGGGCAAQYYYIABBhhggAHGS8G8sgkYYAQwwAADDDDAQAAMMMAAA4y8AmOkX/7EI/0YGpk/t5murLKcq4tZrh9iMVr5NVR6tPIPNEI6ZamDXTMAAAAASUVORK5CYII='/>";
const char* OpeningPng = "<img src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAEwAAABMCAYAAADHl1ErAAACRUlEQVR42u3cv64BQRgF8EkUoiBeQKGSqBQSES+g1qg1Kg8hoqIQhUIj4iEUWlEoxBtoFCJCpf/u/TYZcdcyg/0ze/dscgrJJsvPnN0ZkRHi7rher3S5XOh8PiO/YQs2EU4HoF7DAetTNB5yANGLVU+MrjdHGSDeC8AABjCAAQxgCMAABjCAAQwBGMAABjBlJpMJDQYDgKlyOByo0WjwT8FWarUa7fd7gDlls9lQPp+/Yclks1laLpcAs1cwmUw+YMnE43FjKipMqqAqJlRUmFZBVYKuqDCxgqoEWVFhcgVNrKjws4LFYtE1LJlcLudrRX0Bm06nlE6nXceSSSQSNBwOww/GFWw2m55B2VOv1z2vqAhbBYOuqAhjBYOsqAhzBYOoqAh7BXUqulqtzAPbbrdWFUwD4/e0Xq/NrGSv1zMOrNPpmHsPO51OVKlUjMEqlUp0PB7NfkrKasZiMer3+38+gGeP+rtr8DqVr+92FT2dVozHY5rP5w8fxg8wfs03+dFoFM6lkROYm7Xz60sxGqzValG73QaYDhhjyfN00CINJrF4xSBn6Cq0SICpllfVapXK5bL2siayYBJLnq+LFqlKMpATlhOa08I+UmCZTIZ2u91TLDsan1coFKIJxiuBxWKhxLKj8UoilUpFD6zb7Wpj2dFmsxmekq+mG3hKAuy7iasuGGb6AAMYwPCnYIABDGAAAxgCMIABDGAAQwAGMIAB7F+BYUs//Vhb+mHTSP3c9nTFKNMcXdjL9UMsbK38HMq+tfIPtZsIexFL2QkAAAAASUVORK5CYII='/>";
int ActionTimer;
int ocf = 0;  // 00 do nothing, 1 email on close, 10 email on open, 11 email on open or close
int athm = 0;  // > 10000 enabled, xx-- hour, --xx minute
int hp = 0;  // pointer to most recent history record


/**********************************************************************************
 * 
 *  From here to end comment below is the contents of sendemail.h
 *
 *  If an SD card is used, it is assumed to be mounted on /sdcard.
 * 
 */

#ifndef __SENDEMAIL_H
#define __SENDEMAIL_H

// uncomment for debug output on Serial port
//#define DEBUG_EMAIL_PORT

// uncomment if using SD card
#define USING_SD_CARD

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <base64.h>

#ifdef USING_SD_CARD
#include <FS.h>
#include "SD_MMC.h"
#endif


class SendEmail
{
  private:
    const String host;
    const int port;
    const String user;
    const String passwd;
    const int timeout;
    const bool ssl;
    WiFiClient* client;
    String readClient();

    // stuff for attaching buffers (could be images and videos held in memory)
    int attachbuffercount;  // current number of buffer attachments
    static const int attachbuffermaxcount = 10;  // max number of buffers that can be attached
    struct attachbufferitem {
      char * buffername;  // name for buffer
      char * buffer;  // pointer to buffer
      size_t buffersize;  // number of bytes in buffer
    };
    attachbufferitem attachbufferitems[attachbuffermaxcount];
    
#ifdef USING_SD_CARD
    // stuff for attaching files (assumes SD card is mounted as /sdcard)
    int attachfilecount;  // current number of file attachments
    static const int attachfilemaxcount = 10;  // max number of file that can be attached
    struct attachfileitem {
      char * filename;  // name for file
    };
    attachfileitem attachfileitems[attachfilemaxcount];
#endif
    
  public:
    SendEmail(const String& host, const int port, const String& user, const String& passwd, const int timeout, const bool ssl);

    void attachbuffer(char * name, char * bufptr, size_t bufsize);

    void attachfile(char * name);
    
    bool send(const String& from, const String& to, const String& subject, const String& msg);

    void close();
};

#endif

/* end of sendemail.h */


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 

void sendNotifyEmail() {  
  SendEmail e(emailhost, emailport, emailsendaddr, emailsendpwd, 5000, true); 
  // Send Email
  char cmsg[100];
  char send[40];
  sprintf(send,"\<%s\>", emailsendaddr);
  char recv[40];
  sprintf(recv,"\<%s\>", email);
  sprintf(cmsg, "Garage Door %s at %s", GDStatus, GDDate);
  String msg = String(cmsg);

  int j = 1;  // used to loop five times trying to send email
  bool result = e.send(send, recv, "Garage Door Event", msg); 
  while ( !result && j < 5 ) {  // retry sending email 4 more times
    delay( j * 1000 );
    result = e.send(send, recv, "Garage Door Event", msg); 
    j++;   
  }
  if ( result ) { Serial.print( cmsg ); Serial.println(" email sent"); }
  else { 
    Serial.print("Failed to send "); Serial.print( cmsg );
    Serial.println(" email"); 
  }
  e.close(); // close email client
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 

void trigger_GDRelay() {  //trigger the garage door relay for 0.2 seconds
  //pinMode(2, OUTPUT);     // GPIO2 should be connected to the garage door relay
                          // set in setup  
  digitalWrite(2, LOW);   // set low to enable trigger relay
  delay(200);             // delete 0.2 seconds
  digitalWrite(2, HIGH);  // set high to disable trigger relay

  Serial.println("Garage Door relay triggered");
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 

int get_GDSensor() {  //get current value from garage door reed sensor
  //pinMode(13, INPUT);   // GPIO13 should be connected to the garage door reed sensor
                          // set in setup  
  return digitalRead(13);  // low (0) door is closed, high (1) door is open (0 False, 1 True)
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 

void update_GDHistory() {  //load latest GDStatus into GDHistory
  for ( int i = 9; i > 0; i-- ) {
    strcpy(GDHistory[i], GDHistory[i-1]);  
  }
  char msg[100];
  getLocalTime(&timeinfo);
  strftime(msg, sizeof msg, "%b %d %Y %I:%M %p", &timeinfo);
  strcpy(GDDate, msg);
  if ( !strcmp( GDStatus, "Open" ) ) {
    sprintf(msg, "<tr><td>Opened on %s</td></tr>", GDDate);
  } else {
    sprintf(msg, "<tr><td>Closed on %s</td></tr>", GDDate);
  }
  strcpy(GDHistory[0], msg);
  // save nvs flash history record
  hp++;
  if ( hp > 9 ) { hp = 0; }
  preferences.putInt("hp", hp);
  switch(hp) {
    case 0 : preferences.putString("h0", String(msg)); break;
    case 1 : preferences.putString("h1", String(msg)); break;
    case 2 : preferences.putString("h2", String(msg)); break;
    case 3 : preferences.putString("h3", String(msg)); break;
    case 4 : preferences.putString("h4", String(msg)); break;
    case 5 : preferences.putString("h5", String(msg)); break;
    case 6 : preferences.putString("h6", String(msg)); break;
    case 7 : preferences.putString("h7", String(msg)); break;
    case 8 : preferences.putString("h8", String(msg)); break;
    default : preferences.putString("h9", String(msg)); break;
  }
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 

int cnt = 0;
void check_GDStatus() {  //check GD and take action if needed

  if (get_GDSensor()) {  // 0 - False - Closed, 1 - True - Open
    strcpy(SensorStatus, "Open");
  } else {
    strcpy(SensorStatus, "Closed");    
  }

  if (strcmp(SensorStatus,GDStatus)) {  // if not equal, take action

    if (!strcmp(GDStatus, "Opening")) {  // handle Opening
      if (!strcmp(SensorStatus, "Closed") && (ActionTimer == 0) ) {  // trigger GD open
        trigger_GDRelay();  // trigger garage door relay
        ActionTimer = 15;
      } else {  // waiting for GD open to finish
        ActionTimer--;
        if (ActionTimer <= 0) {
          strcpy(GDStatus, "Open");
          update_GDHistory();
          ActionTimer = 0;
          Serial.println("Garage Door opened");
          if ( ocf > 9 ) {  // email open notification
            sendNotifyEmail();
          }
        } 
      }
    }

    if (!strcmp(GDStatus, "Closed")) {  // handle if GD was opened not by GDC
      strcpy(GDStatus, "Opening");
      ActionTimer = 15;
    }
    
    if (!strcmp(GDStatus, "Closing")) {  // handle if GD is closing
      if (ActionTimer == 0) {  // start closing
        trigger_GDRelay();  // trigger GD to close
        ActionTimer = 20;
      } else {
        ActionTimer--;
        if (ActionTimer <= 0) {  // garage door didn't close
          strcpy(GDStatus, "Open");
          ActionTimer = 0;
        } 
      }
    }
    
    if (!strcmp(SensorStatus, "Closed") && (ActionTimer < 5)) {  // handle if GD just closed
      strcpy(GDStatus, "Closed");
      update_GDHistory();
      ActionTimer = 0;
      Serial.println("Garage Door closed");
      if ( ( ocf % 10 ) == 1 ) { // email close notification
        sendNotifyEmail();  
      }
    }
  }
  
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 

void major_fail() {  //flash the led on the ESP32 in case of error, then sleep and reboot

  for  (int i = 0;  i < 5; i++) {
    digitalWrite(33, LOW);   delay(150);
    digitalWrite(33, HIGH);  delay(150);
    digitalWrite(33, LOW);   delay(150);
    digitalWrite(33, HIGH);  delay(150);
    digitalWrite(33, LOW);   delay(150);
    digitalWrite(33, HIGH);  delay(150);

    delay(150);

    digitalWrite(33, LOW);  delay(500);
    digitalWrite(33, HIGH); delay(150);
    digitalWrite(33, LOW);  delay(500);
    digitalWrite(33, HIGH); delay(150);
    digitalWrite(33, LOW);  delay(500);
    digitalWrite(33, HIGH); delay(150);

    delay(150);

    digitalWrite(33, LOW);   delay(150);
    digitalWrite(33, HIGH);  delay(150);
    digitalWrite(33, LOW);   delay(150);
    digitalWrite(33, HIGH);  delay(150);
    digitalWrite(33, LOW);   delay(150);
    digitalWrite(33, HIGH);  delay(150);

    delay(450);
  }

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP5S * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 

bool init_wifi()
{
  int connAttempts = 0;
  
  // this is the fixed ip stuff 
  IPAddress local_IP(IP_Address);

  // Set your Gateway IP address
  IPAddress gateway(IP_Gateway);

  IPAddress subnet(IP_Subnet);
  IPAddress primaryDNS(IP_PrimaryDNS); // optional
  IPAddress secondaryDNS(IP_SecondaryDNS); // optional

  //WiFi.persistent(false);

  WiFi.mode(WIFI_STA);

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
    major_fail();
  }

  //WiFi.setSleep(false);  // turn off wifi power saving, makes response MUCH faster
  
  WiFi.printDiag(Serial);

  // uncomment the below to use DHCP
  //WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE); // workaround for bug in WiFi class

  WiFi.begin(ssid, password);
  
  char hostname[12];
  sprintf( hostname, "ESP32CAM%d", IP_Address[3] );
  WiFi.setHostname(hostname);  // only effective when using DHCP

  while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
    if (connAttempts > 10) {
      Serial.println("Cannot connect");
      WiFi.printDiag(Serial);
      major_fail();
      return false;
    }
    connAttempts++;
  }
  return true;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 

void init_time() {

  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_setservername(1, "time.windows.com");
  sntp_setservername(2, "time.nist.gov");

  sntp_init();

  // wait for time to be set
  time_t now = 0;
  timeinfo = { 0 };
  int retry = 0;
  const int retry_count = 10;
  while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
    Serial.printf("Waiting for system time to be set... (%d/%d) -- %d\n", retry, retry_count, timeinfo.tm_year);
    delay(2000);
    time(&now);
    localtime_r(&now, &timeinfo);
    Serial.println(ctime(&now));
  }

  if (timeinfo.tm_year < (2016 - 1900)) {
    major_fail();
  }
}


char * the_page = NULL;  // used to hold complete response page before its sent to browser

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 
void do_main() {

  Serial.println("do_main ");

  // build main display
  strcpy(the_page, "<br><table><tr><td align='center'>");

  char msg[100];
  if ( !strcmp( GDStatus, "Opening" ) ) {
    strcat(the_page, OpeningPng);
    sprintf(msg, "</td></tr><tr><td>Opening as of %s</td></tr>", GDDate);
    strcat(the_page, msg);
    strcat(the_page, "</table><form><br><table><tr><td>&nbsp;</td></tr></table></form>");
  } else if ( !strcmp( GDStatus, "Open" ) ) {
    strcat(the_page, OpenPng);
    sprintf(msg, "</td></tr><tr><td>Opened as of %s</td></tr>", GDDate);
    strcat(the_page, msg);
    strcat(the_page, "</table><form><br><table><tr><td><input type='button' id='Closing' value='CLOSE DOOR' onclick='send_Close();' /></td></tr></table></form>");
  } else if ( !strcmp( GDStatus, "Closing" ) ) {
    strcat(the_page, ClosingPng);
    sprintf(msg, "</td></tr><tr><td>Closing as of %s</td></tr>", GDDate);
    strcat(the_page, msg);
    strcat(the_page, "</table><form><br><table><tr><td>&nbsp;</td></tr></table></form>");
  } else {
    strcat(the_page, ClosedPng);
    sprintf(msg, "</td></tr><tr><td>Closed as of %s</td></tr>", GDDate);
    strcat(the_page, msg);
    strcat(the_page, "</table><form><br><table><tr><td><input type='button' id='Opening' value='OPEN DOOR' onclick='send_Open();' /></td></tr></table></form>");
  }
  strcat(the_page, "<br><table><tr><td align='center'>Recent History</td></tr>");

  // add last 10 history events
  for ( int i = 0; i < 10; i++ ) {
    strcat(the_page, GDHistory[i]);
  }
  strcat(the_page, "</table><br><form><table><tr><td><input type='button' id='settings' value='SETTINGS' onclick='do_settings();' /></td></tr></table></form>");
  
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 
void do_settings() {

  Serial.println("do_settings ");

  char buf[10];
    
  // build settings display
  strcpy(the_page, "<br><table><tr><td align='center'>Settings</td></tr>");
  strcat(the_page, "<tr><td align='center'><br>Notifications will be sent to ewilliams41@hotmail.com</td></tr></table>");
  strcat(the_page, "<form><br><table><tr><tr><td><input type='checkbox' id='openflag' name='OpenFlag' value='Open' ");
  if ( ocf > 9 ) { strcat(the_page, "checked"); }
  strcat (the_page, "> Notify on garage door open</td></tr><tr><td><input type='checkbox' id='closeflag' name='CloseFlag' value='Close' ");
  if ( (ocf % 10 ) == 1 ) { strcat(the_page, "checked"); }
  strcat(the_page, "> Notify on garage door close</td></tr></table>");
  strcat(the_page, "<br><table><tr><td><input type='checkbox' id='autoclose' name='AutoClose' value='AutoClose' ");;
  if ( athm > 9999 ) { strcat(the_page, "checked"); }
  int th = (( athm % 10000 ) / 100 );
  int tm = (athm % 100);
  strcat(the_page, "> Auto close garage door each day if open<br>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; at ");
  strcat(the_page, "(0-23H : 0-59M): ");
  strcat(the_page, "<input type='number' id='th' value='");
  sprintf(buf,"%i",th);
  strcat(the_page, buf);
  strcat(the_page, "' oninput='javascript: if (isNaN(parseInt(this.value))) { this.value=0; }; if (this.value > 23) { this.value=23; }""' min='0' max='23' style='width: 3em'/>H : ");
  strcat(the_page, "<input type='number' id='tm' value='");
  sprintf(buf,"%i",tm);
  strcat(the_page, buf);
  strcat(the_page, "' oninput='javascript: if (isNaN(parseInt(this.value))) { this.value=0; }; if (this.value > 59) { this.value=59; }""' min='0' max='59' style='width: 3em'/>M ");
  strcat(the_page, "</td></tr></td></tr></table>");
  strcat(the_page, "<br><table><tr><td align='center'><input type='button' id='closesettings' value='SAVE AND CLOSE' onclick='close_settings();' /></td></tr></table></form>");

}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 
void do_GDC_main() {

  Serial.println("do_GDC_main ");

  const char msg[] PROGMEM = R"rawliteral(<!doctype html>
<html>
<head>
<meta http-equiv='Content-Type' content='text/html; charset=utf-8'/>
<title>Garage Door Controller</title>
</head>
<body onload="zoom(); do_ready();">
<center>
<div id = "main"></div>
</center>
<script>
  var GDStatus = "%s";
  
  function send_Close() {
    var XHR = new XMLHttpRequest();
    XHR.open( "GET", `/GDC?action=closing`, true );  
    XHR.onloadend = function(){
      DBStatus = XHR.responseText;
    }
    XHR.send( null );
    document.getElementById('Closing').disabled = true;
  }  

  function send_Open() {
    var XHR = new XMLHttpRequest();
    XHR.open( "GET", `/GDC?action=opening`, true );  
    XHR.onloadend = function(){
      DBStatus = XHR.responseText;
    }
    XHR.send( null );
    document.getElementById('Opening').disabled = true;
  }  

  function do_ready() {
    do_main();
    var refreshId = setInterval(function(){
      var XHR = new XMLHttpRequest();
      XHR.open( "GET", `/GDC?action=status`, true );  
      XHR.onloadend = function(){
        var status = XHR.responseText;
        if ( GDStatus !== status) {
          GDStatus = status;
          do_main();
        }
      }
      XHR.send( null );      
    }, 3000);
  }
  
  function do_main() {
    var XHR = new XMLHttpRequest();
    XHR.open( "GET", `/GDC?action=main`, true );  
    XHR.onloadend = function(){
      document.getElementById('main').innerHTML = XHR.responseText;
    }
    XHR.send( null );
  }  

  function do_settings() {
    var XHR = new XMLHttpRequest();
    XHR.open( "GET", `/GDC?action=settings`, true );  
    XHR.onloadend = function(){
      document.getElementById('main').innerHTML = XHR.responseText;
    }
    XHR.send( null );
  }  

  function close_settings() {
    var openflag = document.getElementById("openflag");
    var closeflag = document.getElementById("closeflag");
    var autoclose = document.getElementById("autoclose");
    var th = parseInt(document.getElementById("th").value);
    var tm = parseInt(document.getElementById("tm").value);
    var athm = (th * 100) + tm;
    if (autoclose.checked) { athm = athm + 10000; }
    var ocf = 0;
    if (openflag.checked) { ocf = ocf + 10; }
    if (closeflag.checked) { ocf = ocf + 1; }

    var XHR = new XMLHttpRequest();
    XHR.open( "GET", `/GDC?action=savesettings&ocf=`+ocf+`&athm=`+athm, true );  
    XHR.onloadend = function(){
      document.getElementById('main').innerHTML = XHR.responseText;
    }
    XHR.send( null );
  }  

  // force reload when going back to page
  if (!!window.performance && window.performance.navigation.type == 2) {
    window.location.reload();
  }
  
  // size the page to fit nicely on my cell phone
  function zoom() {
    document.body.style.zoom = '250%%';
  }
</script>
</body>
</html>)rawliteral";

  sprintf(the_page, msg, GDStatus);
  //sprintf(the_page, msg, BatteryVoltage);
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 
static esp_err_t GDC_main_handler(httpd_req_t *req) {

  //Serial.print(F("in GDC_main_handler, Heap is: ")); Serial.println(ESP.getFreeHeap());


  char buf[500];
  size_t buf_len;
  char param[60];
  char action[20];
  int nocf;
  int nathm;
  
  // default action, show GDC main page
  strcpy(action, "show");

  // query parameters - get action
  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "action", param, sizeof(param)) == ESP_OK) {
        strcpy(action, param);
      }
      if (httpd_query_key_value(buf, "ocf", param, sizeof(param)) == ESP_OK) {
        nocf = atoi(param);
      }
      if (httpd_query_key_value(buf, "athm", param, sizeof(param)) == ESP_OK) {
        nathm = atoi(param);
      }
    }
  }

  //Serial.print(F("GDC_main_hander with action: ")); Serial.println( action );

  if ( !strcmp( action, "main" ) ) {  // fill main div on home page
    do_main();
    httpd_resp_send(req, the_page, strlen(the_page));
        
  } else if ( !strcmp( action, "settings" ) ) {  // fill main div with settings page
    do_settings();
    httpd_resp_send(req, the_page, strlen(the_page));

  } else if ( !strcmp( action, "savesettings" ) ) {  // save settings and show main page
    sprintf(buf, "ocf=%i, athm=%i", nocf, nathm);
    Serial.println(buf);
    if ( ocf != nocf ) {  // new open close flag values
      ocf = nocf;
      preferences.putInt("ocf", ocf);
      Serial.print("Saving new ocf value "); Serial.println(ocf);
    }
    if ( athm != nathm ) {  // new auto close values
      athm = nathm;
      preferences.putInt("athm", athm);
      Serial.print("Saving new athm value "); Serial.println(athm);
    }
    do_main();
    httpd_resp_send(req, the_page, strlen(the_page));

  } else if ( !strcmp( action, "status" ) ) {  // send current GDStatus
    strcpy(the_page, GDStatus);    
    httpd_resp_send(req, the_page, strlen(the_page));

  } else if ( !strcmp( action, "closing" ) ) {  // do Closing
    char msg[30];
    getLocalTime(&timeinfo);
    strftime(msg, sizeof msg, "%b %d %Y %I:%M %p", &timeinfo);
    strcpy(GDDate, msg);
    strcpy(GDStatus, "Closing");
    strcpy(the_page, GDStatus);    
    httpd_resp_send(req, the_page, strlen(the_page));

  } else if ( !strcmp( action, "opening" ) ) {  // do Opening
    char msg[30];
    getLocalTime(&timeinfo);
    strftime(msg, sizeof msg, "%b %d %Y %I:%M %p", &timeinfo);
    strcpy(GDDate, msg);
    strcpy(GDStatus, "Opening");
    strcpy(the_page, GDStatus);    
    httpd_resp_send(req, the_page, strlen(the_page));

  } else if ( !strcmp( action, "history" ) ) {
    sprintf(the_page, "Notifications button pressed");    
    httpd_resp_send(req, the_page, strlen(the_page));

  } else if ( !strcmp( action, "notifications" ) ) {
    sprintf(the_page, "Notifications button pressed");    
    httpd_resp_send(req, the_page, strlen(the_page));

  } else {
    // display GDC main page
    do_GDC_main();
    httpd_resp_send(req, the_page, strlen(the_page));
  }
  
  return ESP_OK;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 

static esp_err_t updatefirmware_get_handler(httpd_req_t *req) {

  Serial.println("In updatefirmware_get_handler");

  const char msg[] PROGMEM = R"rawliteral(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Firmware Updater</title>
<script>

function clear_status() {
  document.getElementById('status').innerHTML = " &nbsp; ";
}

function uploadfile(file) {
  if ( !document.getElementById("updatefile").value ) {
    alert( "Choose a valid firmware file" );
  } else {
    let xhr = new XMLHttpRequest();
    document.getElementById('updatebutton').disabled = true;
    document.getElementById('status').innerText = "Progress 0%%";
    let eraseEEPROMvalue = document.getElementById('EraseEEPROM').checked;
    let upwd = document.getElementById('upwd').value;
    // track upload progress
    xhr.upload.onprogress = function(event) {
      if (event.lengthComputable) {
        var per = event.loaded / event.total;
        document.getElementById('status').innerText = "Progress " + Math.round(per*100) + "%%";
      }
    };
    // track completion: both successful or not
    xhr.onloadend = function() {
      if (xhr.status == 200) {
        document.getElementById('status').innerText = xhr.response;
      } else {
        document.getElementById('status').innerText = "Firmware update failed";
      }
      document.getElementById('updatebutton').disabled = false;
      document.getElementById('upwd').value = "";
    };
    xhr.open("POST", "/updatefirmware");
    xhr.setRequestHeader('EraseEEPROM', eraseEEPROMvalue);
    xhr.setRequestHeader('UPwd', upwd);
    xhr.send(file);
  }
}

</script>
</head>
<body><center>
<h1>ESP32 Firmware Updater</h1>

Select an ESP32 firmware file (.bin) to update the ESP32 firmware<br><br>

<table>
<tr><td align="center"><input type="file" id="updatefile" accept=".bin" onclick="clear_status();"><br><br></td></tr>
<tr><td align="center"><input type="checkbox" id="EraseEEPROM" onclick="clear_status();"> Erase EEPROM/Preferences<br><br></td></tr>
<tr><td align="center">Update Password <input type="password" id="upwd" maxlength="20"><br><br></td></tr>
<tr><td align="center"><input type="button" id="updatebutton" onclick="uploadfile(updatefile.files[0]);" value="Update"><br><br></td></tr>
<tr><td align="center"><div id="status"> &nbsp; </div><br><br></td></tr>
<tr><td align="center">%s Version %s</td></tr>
</table>
</center></body>
</html>)rawliteral";

  //strcpy(the_page, msg);
  sprintf(the_page, msg, appName, appVersion);

  httpd_resp_send(req, the_page, strlen(the_page));

  return ESP_OK;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 
#define UPLOAD_CACHE_SIZE 1600

static esp_err_t updatefirmware_post_handler(httpd_req_t *req) {

  Serial.println("In updatefirmware_post_handler");

  char contentBuffer[UPLOAD_CACHE_SIZE];
  size_t recv_size = sizeof(contentBuffer);
  size_t contentLen = req->content_len;

  char eraseEeprom[10];
  httpd_req_get_hdr_value_str(req, "EraseEEPROM", eraseEeprom, sizeof(eraseEeprom));
  Serial.println((String) "EraseEEPROM/Preferences " + eraseEeprom );
  char upwd[20];
  httpd_req_get_hdr_value_str(req, "UPwd", upwd, sizeof(upwd));
  Serial.println((String) "Update password " + upwd );

  Serial.println((String) "Content length is " + contentLen);

  if ( !strcmp( firmwareUpdatePassword, upwd ) ) {
    // update password is good, do the firmware update

    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start flash with max available size
      Update.printError(Serial);
      strcpy(the_page, "Firmware update failed - Update failed begin step");
      Serial.println( the_page );
      httpd_resp_send(req, the_page, strlen(the_page));
      return ESP_OK;
    }
      
    size_t bytes_recvd = 0;
    while (bytes_recvd < contentLen) {
      //if ((contentLen - bytes_recvd) < recv_size) recv_size = contentLen - bytes_recvd;
      int ret = httpd_req_recv(req, contentBuffer, recv_size);
      if (ret <= ESP_OK) {  /* ESP_OK return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
          httpd_resp_send_408(req);
        }
        return ESP_FAIL;
      }
      if (Update.write((uint8_t *) contentBuffer, ret) != ret) {
        Update.printError(Serial);
        strcpy(the_page, "Firmware update failed - Update failed write step");
        Serial.println( the_page );
        httpd_resp_send(req, the_page, strlen(the_page));
        return ESP_OK;
      }
      bytes_recvd += ret;
    }

    if (!Update.end(true)) { //true to set the size to the current progress
      Update.printError(Serial);
      strcpy(the_page, "Firmware update failed - Update failed end step");
      Serial.println( the_page );
      httpd_resp_send(req, the_page, strlen(the_page));
      return ESP_OK;
    }

    if ( !strcmp( "true", eraseEeprom ) ) { // erase EEPROM
      EEPROM.end();
      EEPROM.begin(512);
      for (int i = 0 ; i < 512 ; i++) {
        EEPROM.write(i, 0xFF);  // set all EEPROM memory to FF
      }
      EEPROM.end();
      preferences.end();
      preferences.begin("my_app", false);  // open nvs flash area to save data
      preferences.clear();
      preferences.end();
      strcpy(the_page, "Firmware update and EEPROM/Preferences erase successful - Rebooting");
    } else {
      strcpy(the_page, "Firmware update successful - Rebooting");
    }
    Serial.println( the_page );
    httpd_resp_send(req, the_page, strlen(the_page));

    delay(5000);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP5S * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
  } else {
    strcpy(the_page, "Firmware update failed - Invalid password");
  }

  Serial.println( the_page );
  httpd_resp_send(req, the_page, strlen(the_page));
  return ESP_OK;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 

void startWebServer() {

  // start a web server for OTA updates
  // this same web server can be used for all other normal web server stuff
  // just add the appropriate uri handlers
  
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = SERVER_PORT;
  config.stack_size = 16384;

  httpd_uri_t GDC_main_uri = {
    .uri       = "/GDC",
    .method    = HTTP_GET,
    .handler   = GDC_main_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t updatefirmware_get_uri = {
    .uri       = "/updatefirmware",
    .method    = HTTP_GET,
    .handler   = updatefirmware_get_handler,
    .user_ctx  = NULL
  };

  httpd_uri_t updatefirmware_post_uri = {
    .uri       = "/updatefirmware",
    .method    = HTTP_POST,
    .handler   = updatefirmware_post_handler,
    .user_ctx  = NULL
  };

  if (httpd_start(&webserver_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(webserver_httpd, &GDC_main_uri);
    httpd_register_uri_handler(webserver_httpd, &updatefirmware_get_uri);
    httpd_register_uri_handler(webserver_httpd, &updatefirmware_post_uri);
  }
}

/*
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 

// MicroSD
#include "SD_MMC.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

static esp_err_t init_sdcard()
{
  esp_err_t ret = ESP_FAIL;
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    //.max_files = 10,
    .max_files = 6,
  };
  sdmmc_card_t *card;

  Serial.println("Mounting SD card...");
  ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

  if (ret == ESP_OK) {
    Serial.println("SD card mount successfully!");
  }  else  {
    Serial.printf("Failed to mount SD card VFAT filesystem. Error: %s", esp_err_to_name(ret));
    major_fail();
  }

  Serial.print("SD_MMC Begin: "); Serial.println(SD_MMC.begin());
}
*/


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// idle_task0() - used to calculate system load on CPU0 (run at priority 0)
//
static unsigned long idle_cnt0 = 0LL;

static void idle_task0(void *parm) {
  while(1==1) {
    int64_t now = esp_timer_get_time();     // time anchor
    vTaskDelay(0 / portTICK_RATE_MS);
    int64_t now2 = esp_timer_get_time();
    idle_cnt0 += (now2 - now) / 1000;        // diff
  }
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// idle_task1() - used to calculate system load on CPU1 (run at priority 0)
//
static unsigned long idle_cnt1 = 0LL;

static void idle_task1(void *parm) {
  while(1==1) {
    int64_t now = esp_timer_get_time();     // time anchor
    vTaskDelay(0 / portTICK_RATE_MS);
    int64_t now2 = esp_timer_get_time();
    idle_cnt1 += (now2 - now) / 1000;        // diff
  }
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// mon_task() - used to display system load (run at priority 10)
//
static int mon_task_freq = 60;  // how often mon_task will wake in seconds

static void mon_task(void *parm) {
  while(1==1) {
    float new_cnt0 =  (float)idle_cnt0;    // Save the count for printing it ...
    float new_cnt1 =  (float)idle_cnt1;    // Save the count for printing it ...
        
    // Compensate for the 100 ms delay artifact: 900 ms = 100%
    float cpu_percent0 = ((99.9 / 90.) * new_cnt0) / (mon_task_freq * 10);
    float cpu_percent1 = ((99.9 / 90.) * new_cnt1) / (mon_task_freq * 10);
    printf("Load (CPU0, CPU1): %.0f%%, %.0f%%\n", 100 - cpu_percent0, 
      100 - cpu_percent1); fflush(stdout);
    idle_cnt0 = 0;                        // Reset variable
    idle_cnt1 = 0;                        // Reset variable
    vTaskDelay((mon_task_freq * 1000) / portTICK_RATE_MS);
  }
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  Serial.begin(115200);

  pinMode(33, OUTPUT);    // little red led on back of chip
  digitalWrite(33, LOW);  // turn on the red LED on the back of chip
  pinMode(4, OUTPUT);     // flash led on chip
  digitalWrite(4, LOW);   // turn off the flash LED on chip

  pinMode(2, OUTPUT);     // GPIO2 should be connected to the garage door relay IN2
  digitalWrite(2, HIGH);  // set low for 0.2 seconds to trigger garage door relay
  pinMode(14, OUTPUT);     // GPIO14 should be connected to the garage door relay IN1
  digitalWrite(14, HIGH);  // always high
  pinMode(13, INPUT_PULLUP);  // GPIO13 should be connected to the garage door reed sensor


  // setup event watcher to catch if wifi disconnects
  eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.print("WiFi lost connection. Reason: ");
    Serial.println(info.disconnected.reason);
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("*** connected/disconnected issue!   WiFi disconnected ???...");
      WiFi.disconnect();
    } else {
      Serial.println("*** WiFi disconnected ???...");
    }
  }, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);

  if (init_wifi()) { // Connected to WiFi
    Serial.println("Internet connected");

    init_time();
    time(&now);

    // set timezone
    setenv("TZ", TZ_INFO, 1); 
    tzset();
    time(&now);
    localtime_r(&now, &timeinfo);

    Serial.print("After timezone : "); Serial.println(ctime(&now));
  }

/*  
  // SD card init
  esp_err_t card_err = init_sdcard();
  if (card_err != ESP_OK) {
    Serial.printf("SD Card init failed with error 0x%x", card_err);
    major_fail();
    return;
  }
*/

  //uncomment the section below to report CPU load every 60 seconds
/*
  // create tasks for monitoring system load  
  xTaskCreatePinnedToCore(idle_task0, "idle_task0", 1024 * 2, NULL,  0, NULL, 0);
  xTaskCreatePinnedToCore(idle_task1, "idle_task1", 1024 * 2, NULL,  0, NULL, 1);
  xTaskCreate(mon_task, "mon_task", 1024 * 2, NULL, 10, NULL);
*/

  // load current GDC data from nvs flash
  preferences.begin("my_app", false);  // open nvs flash area to save data
  ocf = preferences.getInt("ocf", 0);  // open close flags
  athm = preferences.getInt("athm", 0);  // auto close time
  // load GDC history data from nvs flash
  hp = preferences.getInt("hp", 0);  // pointer to most recent history record
  String h[10];  // nvs flash history records
  h[0] = preferences.getString("h0", "<tr><td>&nbsp;</td></tr>");
  h[1] = preferences.getString("h1", "<tr><td>&nbsp;</td></tr>");
  h[2] = preferences.getString("h2", "<tr><td>&nbsp;</td></tr>");
  h[3] = preferences.getString("h3", "<tr><td>&nbsp;</td></tr>");
  h[4] = preferences.getString("h4", "<tr><td>&nbsp;</td></tr>");
  h[5] = preferences.getString("h5", "<tr><td>&nbsp;</td></tr>");
  h[6] = preferences.getString("h6", "<tr><td>&nbsp;</td></tr>");
  h[7] = preferences.getString("h7", "<tr><td>&nbsp;</td></tr>");
  h[8] = preferences.getString("h8", "<tr><td>&nbsp;</td></tr>");
  h[9] = preferences.getString("h9", "<tr><td>&nbsp;</td></tr>");
  int j = hp;
  for ( int i = 0; i < 10; i++) {
    strcpy(GDHistory[i], h[j].c_str());  // load GDHistory from nvs history records
    j--;
    if ( j < 0 ) { j = 9; }  
  }
  
  // allocate PSRAM for holding web page content before sending
  the_page = (char*) heap_caps_calloc(8000, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

  startWebServer();

  // setup GDC stuff
  if (get_GDSensor()) {  // 0 - False - Closed, 1 - True - Open 
    strcpy(GDStatus, "Open");
  } else {
    strcpy(GDStatus, "Closed");    
  }
  char msg[100];
  getLocalTime(&timeinfo);
  strftime(msg, sizeof msg, "%b %d %Y %I:%M %p", &timeinfo);
  strcpy(GDDate, msg);
  ActionTimer = 0;

  digitalWrite(33, HIGH);  // turn off the red LED on the back of chip

  Serial.print("ESP32 Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");

}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// 
unsigned long last_wakeup_1sec = 0;
unsigned long last_wakeup_1min = 0;

void loop() {

  if ( (millis() - last_wakeup_1sec) > 1000 ) {       // 1 second
    last_wakeup_1sec = millis();

    if (WiFi.status() != WL_CONNECTED) {
      init_wifi();
      Serial.println("***** WiFi reconnect *****");
    }

    check_GDStatus();
  }

  if ( millis() - last_wakeup_1min > (1 * 60 * 1000) ) {       // 1 minute
    last_wakeup_1min = millis();

    // check if time to auto close garage door
    if ( athm > 9999 ) {
      // get current time
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed to obtain time");
      } else {
        char hc[3];  // current hour
        strftime(hc, sizeof(hc), "%H", &timeinfo);
        int h = atoi(hc);
        char mc[3];  // current minute
        strftime(mc, sizeof(mc), "%M", &timeinfo);
        int m = atoi(mc);

        if ( ((( athm % 10000 ) / 100 ) == h ) && (( athm % 100 ) == m ) && strcmp(GDStatus, "Closed") ) {
          char msg[30];
          getLocalTime(&timeinfo);
          strftime(msg, sizeof msg, "%b %d %Y %I:%M %p", &timeinfo);
          strcpy(GDDate, msg);
          strcpy(GDStatus, "Closing"); 
          Serial.println("Garage Door found open, doing auto close");
        }
      }
    }
  }

  delay(200);
}


/*****************************************************************************
 * 
 *  From here to the end is the contents of sendemail.cpp
 *  This version can send text or binary objects (jpg, avi) from memory as attachments
 *  and can send files from the SD card mounted on /sdcard.
 *  
 *  *** Uncomment the #include if the below is moved back into sendemail.cpp ***
 *
 */

//#include "sendemail.h"

SendEmail::SendEmail(const String& host, const int port, const String& user, const String& passwd, const int timeout, const bool ssl) :
    host(host), port(port), user(user), passwd(passwd), timeout(timeout), ssl(ssl), client((ssl) ? new WiFiClientSecure() : new WiFiClient())
{
  attachbuffercount = 0;
#ifdef USING_SD_CARD
  attachfilecount = 0;
#endif
}

String SendEmail::readClient()
{
  String r = client->readStringUntil('\n');
  r.trim();
  while (client->available()) r += client->readString();
  return r;
}

void SendEmail::attachbuffer(char * name, char * bufptr, size_t bufsize)
{
  attachbufferitems[attachbuffercount].buffername = (char *) malloc( strlen(name)+1 );
  strcpy( attachbufferitems[attachbuffercount].buffername, name ); 
  attachbufferitems[attachbuffercount].buffer = bufptr;
  attachbufferitems[attachbuffercount].buffersize = bufsize;
  
  attachbuffercount++;
}

#ifdef USING_SD_CARD
void SendEmail::attachfile(char * name)
{
  attachfileitems[attachfilecount].filename = (char *) malloc( strlen(name)+8 );
  strcpy( attachfileitems[attachfilecount].filename, "/sdcard" );
  strcat( attachfileitems[attachfilecount].filename, name );
  
  attachfilecount++;
}
#endif


bool SendEmail::send(const String& from, const String& to, const String& subject, const String& msg)
{
  if (!host.length())
  {
    return false;
  }
  String buffer2((char *)0);
  buffer2.reserve(800);  // really should only use 780 of it
  client->stop();
  client->setTimeout(timeout);
  // smtp connect
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("Connecting: "));
  Serial.print(host);
  Serial.print(":");
  Serial.println(port);
#endif
  if (!client->connect(host.c_str(), port))
  {
    return false;
  }
  String buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("SERVER->CLIENT: "));
  Serial.println(buffer);
#endif
  // note: F(..) as used below puts the string in flash instead of RAM
  if (!buffer.startsWith(F("220")))
  {
    return false;
  }
  buffer = F("HELO ");
  buffer += client->localIP();
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
  buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("SERVER->CLIENT: "));
  Serial.println(buffer);
#endif
  if (!buffer.startsWith(F("250")))
  {
    return false;
  }
  if (user.length()>0  && passwd.length()>0 )
  {
    buffer = F("AUTH LOGIN");
    client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
    buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("SERVER->CLIENT: "));
  Serial.println(buffer);
#endif
    if (!buffer.startsWith(F("334")))
    {
      return false;
    }
    base64 b;
    buffer = user;
    buffer = b.encode(buffer);
    client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
    buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("SERVER->CLIENT: "));
  Serial.println(buffer);
#endif
    if (!buffer.startsWith(F("334")))
    {
      return false;
    }
    buffer = this->passwd;
    buffer = b.encode(buffer);
    client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
    buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("SERVER->CLIENT: "));
  Serial.println(buffer);
#endif
    if (!buffer.startsWith(F("235")))
    {
      return false;
    }
  }
  // smtp send mail
  buffer = F("MAIL FROM: ");
  buffer += from;
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
  buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("SERVER->CLIENT: "));
  Serial.println(buffer);
#endif
  if (!buffer.startsWith(F("250")))
  {
    return false;
  }
  buffer = F("RCPT TO: ");
  buffer += to;
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
  buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("SERVER->CLIENT: "));
  Serial.println(buffer);
#endif
  if (!buffer.startsWith(F("250")))
  {
    return false;
  }
  buffer = F("DATA");
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
  buffer = readClient();
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("SERVER->CLIENT: "));
  Serial.println(buffer);
#endif
  if (!buffer.startsWith(F("354")))
  {
    return false;
  }
  buffer = F("From: ");
  buffer += from;
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
  buffer = F("To: ");
  buffer += to;
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
  buffer = F("Subject: ");
  buffer += subject;
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
  // setup to send message body
  buffer = F("MIME-Version: 1.0\r\n");
  buffer += F("Content-Type: multipart/mixed; boundary=\"{BOUNDARY}\"\r\n\r\n");
  buffer += F("--{BOUNDARY}\r\n");
  buffer += F("Content-Type: text/plain\r\n");
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
  buffer = msg;
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
  // process buffer attachments
  for ( int i = 0; i < attachbuffercount; i++ ) {
    char * pos = attachbufferitems[i].buffer;
    size_t alen = attachbufferitems[i].buffersize;
    base64 b;
    
    buffer = F("\r\n--{BOUNDARY}\r\n");
    buffer += F("Content-Type: application/octet-stream\r\n");
    buffer += F("Content-Transfer-Encoding: base64\r\n");
    buffer += F("Content-Disposition: attachment;filename=\"");
    buffer += attachbufferitems[i].buffername;
    buffer += F("\"\r\n");
    client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
  size_t bytessent = 0;
#endif
    // read data from buffer, base64 encode and send it
    // 3 binary bytes (57) becomes 4 base64 bytes (76)
    // plus CRLF is ideal for one line of MIME data
    // 570 bytes will be read at a time and sent as ten lines of base64 data
    size_t flen = 570;  
    uint8_t * fdata = (uint8_t*) malloc( flen );
    if ( alen < flen ) flen = alen;
    // read data from buffer
    memcpy( fdata, pos, flen ); 
    delay(10);
    buffer2 = "";
    size_t bytecount = 0;
    while ( flen > 0 ) {
      while ( flen > 56 ) {
        // convert to base64 in 57 byte chunks
        buffer2 += b.encode( fdata+bytecount, 57 );
        // tack on CRLF
        buffer2 += "\r\n";
        bytecount += 57;
        flen -= 57;
      }
      if ( flen > 0 ) {
        // convert last set of bytes to base64
        buffer2 += b.encode( fdata+bytecount, flen );
        // tack on CRLF
        buffer2 += "\r\n";
      }
      // send the lines in buffer
      client->println( buffer2 );
#ifdef DEBUG_EMAIL_PORT
  bytessent += bytecount + flen;
  Serial.print(F(" bytes sent so far: ")); Serial.println(bytessent);
#endif
      buffer2 = "";
      delay(10);
      // calculate bytes left to send
      alen = alen - bytecount - flen;
      pos = pos + bytecount + flen;
      flen = 570;
      if ( alen < flen ) flen = alen;
      // read data from buffer
      if ( flen > 0 ) memcpy( fdata, pos, flen ); 
      bytecount = 0;
    }
    free( fdata );
    fdata = NULL;
  }

#ifdef USING_SD_CARD
  // process file attachments
  for ( int i = 0; i < attachfilecount; i++ ) {
    FILE *atfile = NULL;
    char aname[110];
    char * pos = NULL;  // points to actual file name
    size_t flen;
    base64 b;
    if ( atfile = fopen(attachfileitems[i].filename, "r") ) {
      // get filename from attachment name
      pos = strrchr( attachfileitems[i].filename, '/' ) + 1;
      // attachment will be pulled from the file named
      buffer = F("\r\n--{BOUNDARY}\r\n");
      buffer += F("Content-Type: application/octet-stream\r\n");
      buffer += F("Content-Transfer-Encoding: base64\r\n");
      buffer += F("Content-Disposition: attachment;filename=\"");
      buffer += pos ;
      buffer += F("\"\r\n");
      client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
  size_t bytessent = 0;
#endif
      // read data from file, base64 encode and send it
      // 3 binary bytes (57) becomes 4 base64 bytes (76)
      // plus CRLF is ideal for one line of MIME data
      // 570 bytes will be read at a time and sent as ten lines of base64 data
      uint8_t * fdata = (uint8_t*) malloc( 570 );
      // read data from file
      flen = fread( fdata, 1, 570, atfile );
      delay(10);
      buffer2 = "";
      int lc = 0;
      size_t bytecount = 0;
      while ( flen > 0 ) {
        while ( flen > 56 ) {
          // convert to base64 in 57 byte chunks
          buffer2 += b.encode( fdata+bytecount, 57 );
          // tack on CRLF
          buffer2 += "\r\n";
          bytecount += 57;
          flen -= 57;
        }
        if ( flen > 0 ) {
          // convert last set of bytes to base64
          buffer2 += b.encode( fdata+bytecount, flen );
          // tack on CRLF
          buffer2 += "\r\n";
        }
        // send the lines in buffer
        client->println( buffer2 );
#ifdef DEBUG_EMAIL_PORT
  bytessent += bytecount + flen;
  Serial.print(F(" bytes sent so far: ")); Serial.println(bytessent);
#endif
        buffer2 = "";
        bytecount = 0;
        delay(10);
        flen = fread( fdata, 1, 570, atfile );
      }
      fclose( atfile );
      free( fdata );
      fdata = NULL;
    } 
  }
#endif

  buffer = F("\r\n--{BOUNDARY}--\r\n.");
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
  buffer = F("QUIT");
  client->println(buffer);
#ifdef DEBUG_EMAIL_PORT
  Serial.print(F("CLIENT->SERVER: "));
  Serial.println(buffer);
#endif
  return true;
}

void SendEmail::close() {

  // cleanup buffer attachments
  for ( int i = 0; i < attachbuffercount; i++ ) {
    free( attachbufferitems[i].buffername );
    attachbufferitems[i].buffername = NULL;
  }

#ifdef USING_SD_CARD
  // cleanup file attachments
  for ( int i = 0; i < attachfilecount; i++ ) {
    free( attachfileitems[i].filename );
    attachfileitems[i].filename = NULL;
  }
#endif
  
  client->stop();
  delete client;

}
