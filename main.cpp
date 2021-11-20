/* WiFi Example
 * Copyright (c) 2016 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstring>
#define MQTTClient_QOS2 1

#include "mbed.h"
#include <MQTTClientMbedOs.h>
#include <cstdint>
#include "PinDetect.h"
#include "ntp-client/NTPClient.h"




#define LEDON 0
#define LEDOFF 1
#define MQTT_BROKER "192.168.1.176"
#define THING_NAME "ThisThing"
#define LIGHT_LEVEL_TOPIC "mytopic/light"
#define LIGHT_STATE_TOPIC "mytopic/lightState"
#define LIGHT_SWITCH_TOPIC "mytopic/lightSwitch"
#define REDLED_TOPIC   "mytopic/redled" 
#define GREENLED_TOPIC   "mytopic/greenled" 
#define BLUELED_TOPIC   "mytopic/blueled" 
#define ANNOUNCE_TOPIC "mytopic/announce"
#define LIGHT_THRESH_TOPIC "mytopic/lthresh"
#define LOCAL_TOPIC "mytopic/local"
#define MANUAL_TOPIC "mytopic/manual"

WiFiInterface *wifi;

AnalogIn lightLevel(P10_0);
DigitalOut statusLed(LED1);
DigitalOut rxLed(LED2);
DigitalOut lightsR(LED3);
DigitalOut lightsG(LED4);
DigitalOut lightsB(LED5);
DigitalOut brdR(P13_2);
DigitalOut brdG(P13_3);
DigitalOut brdB(P13_4);
uint32_t rxCount = 0;
uint32_t lthresh = 50;
bool manual = false;
bool local = false;
bool lightState = false;
bool lightSwitch = false;

const char *sec2str(nsapi_security_t sec) {
  switch (sec) {
  case NSAPI_SECURITY_NONE:
    return "None";
  case NSAPI_SECURITY_WEP:
    return "WEP";
  case NSAPI_SECURITY_WPA:
    return "WPA";
  case NSAPI_SECURITY_WPA2:
    return "WPA2";
  case NSAPI_SECURITY_WPA_WPA2:
    return "WPA/WPA2";
  case NSAPI_SECURITY_UNKNOWN:
  default:
    return "Unknown";
  }
}
/* Data Structures */

struct dataSet {
    float highTempThresh = 26.0;
    float lowTempThresh = 23.0;
    float ambientTemp;
// Heating and Lighting controls
    bool heatingStatus = 0;  //  off = 0,  on = 1
    bool heatingMode = 0;    // auto = 0, man = 1
    float highLightThresh = 80.0;
    float lowLightThresh = 20.0;
    float ambientLight;
    bool lightingStatus = 0; //  off = 0,  on = 1
    bool lightingMode = 0;   // auto = 0, man = 1
    bool wifiStatus = 0;
    bool timeStatus = 0;
} myData;

PinDetect pb1(BUTTON1);
volatile bool keypressed = true;
volatile uint8 counter = 0;

// prototypes ******************************** //

// select graphic line drawing mode default to UTF-8
//#define VT100
//#define POOR_MANS
#define UTF8 // select vt100 line drawing set
#include "VT100.h"

// Page elements positions
#define TLINE   4  // line where temperature values are displayed
#define LLINE   6  //   "    "   light......          
#define TLPOS   2  // Temperature  and light level displays start on this line
#define FCOL    3  // First column for text
#define TLVAL  25  // Start sensor reading displays in this column
#define TLLED  32  // column for starting led simulations display
#define TLLTH  39  // column for start of display of low threshold values 
#define TLHTH  56  // column for start of display of high threshold values 
#define HEATL   8  // Status display line for the heater
#define LIGHTL 10  // Status display line for lighting
#define STATUS 12  // Status line for user input response
#define USRHLP 14  // line where user help is displayed
#define MODEC  18  // column start for lighting and heating mode Auto/Manual
#define ONOFF  27  // column start for on or off status
#define INSTR  35  // column start for toggling instruction
#define RTCCOL 43  // column Start for Real Time Clock

// Blinking rate in milliseconds
#define BLINKING_RATE_MS 500
#define SW2 P0_4
// Specify different pins to test printing on UART other than the console UART.
#define TARGET_TX_PIN USBTX
#define TARGET_RX_PIN USBRX

// page setup
#define PAGEWIDTH 70
#define PAGELENGTH 19

/*******************************************************************************
 *
 * page[][] 2d representation of the screen border display
 *
 * 
 * j - Bottom right _|
 * k - Top right corner
 * l - top left corner
 * m - Bottom left |_
 * n - cross +
 * q - horizontal bar
 * t - left hand |-
 * u - right hand -|
 * v - Bottom with line up
 * w - top with line down
 * x - vertical bar |
 *
 ******************************************************************************/
char page[PAGELENGTH][PAGEWIDTH] = {
    "lqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqqqwqqqqqqqqqqqqqqqqk\r\n",
    "x                               x               x                x\r\n",
    "tqqqqqqqqqqqqqqqqqqqqqwqqqqqqqwqnqqqqqqqqqqqqqqqnqqqqqqqqqqqqqqqqu\r\n",
    "x                     x       x x               x                x\r\n",
    "tqqqqqqqqqqqqqqqqqqqqqnqqqqqqqnqnqqqqqqqqqqqqqqqnqqqqqqqqqqqqqqqqu\r\n",
    "x                     x       x x               x                x\r\n",
    "tqqqqqqqqqqqqqqwqqqqqqvqwqqqqqnqnqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqu\r\n",
    "x              x        x     x x                                x\r\n",
    "tqqqqqqqqqqqqqqnqqqqqqqqnqqqqqnqnqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqu\r\n",
    "x              x        x     x x                                x\r\n",
    "tqqqqqqqqqqqqqqvqqqqqqqqvqqqqqvqvqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqqu\r\n",
    "x                                    x                           x\r\n",
    "tqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqqqqqqqqqqqu\r\n",
    "x                                                                x\r\n",
    "x                                                                x\r\n",
    "x                                                                x\r\n",
    "x                                                                x\r\n",
    "x                                                                x\r\n",
    "mqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqj\r\n",
};


time_t currentTime;

void posCursor(int col, int line);
void displayAt( int col, int line, char *strBuffer );
void initialise();
void displayData();
void drawBorders();
void updateRealTimeClock(char *timeBuffer);


// Callback routine is interrupt activated by a debounced pb1 hit
void pb1_hit_callback (void)
{
    if (manual) {
        lightState = !lightState;
        lightSwitch = true;
    }

}

void messageLightArrived(MQTT::MessageData &md) {
  MQTT::Message &message = md.message;
  uint32_t len = md.message.payloadlen;
  char rxed[len+1];

  strncpy(&rxed[0], (char *) (&md.message.payload)[0], len);
  lthresh = atoi(rxed);
  rxCount++;
  rxLed = !rxLed;
}
void messageLightStateArrived(MQTT::MessageData &md) {
  MQTT::Message &message = md.message;
  uint32_t len = md.message.payloadlen;
  char rxed[len+1];

  strncpy(&rxed[0], (char *) (&md.message.payload)[0], len);
  if (!local && !manual) lightState = atoi(rxed);
  rxCount++;
  rxLed = !rxLed;
}
void messageLightSwitchArrived(MQTT::MessageData &md) {
  MQTT::Message &message = md.message;
  uint32_t len = md.message.payloadlen;
  char rxed[len+1];

  strncpy(&rxed[0], (char *) (&md.message.payload)[0], len);
  if (manual) {
      lightState = !lightState;
      lightSwitch = true;
  }
  rxCount++;
  rxLed = !rxLed;
}
void messageRedLedArrived(MQTT::MessageData &md) {
  MQTT::Message &message = md.message;
  uint32_t len = md.message.payloadlen;
  char rxed[len+1];

  strncpy(&rxed[0], (char *) (&md.message.payload)[0], len);
  if (!local) lightsR = atoi(rxed);
  rxCount++;
  rxLed = !rxLed;
}
void messageGreenLedArrived(MQTT::MessageData &md) {
  MQTT::Message &message = md.message;
  uint32_t len = md.message.payloadlen;
  char rxed[len+1];

  strncpy(&rxed[0], (char *) (&md.message.payload)[0], len);
  if (!local) lightsG = atoi(rxed);
  rxCount++;
  rxLed = !rxLed;
}
void messageBlueLedArrived(MQTT::MessageData &md) {
  MQTT::Message &message = md.message;
  uint32_t len = md.message.payloadlen;
  char rxed[len+1];

  strncpy(&rxed[0], (char *) (&md.message.payload)[0], len);
  if (!local) lightsB = atoi(rxed);
  rxCount++;
  rxLed = !rxLed;
}
void messageManualArrived(MQTT::MessageData &md) {
  MQTT::Message &message = md.message;
  uint32_t len = md.message.payloadlen;
  char rxed[len+1];

  strncpy(&rxed[0], (char *) (&md.message.payload)[0], len);
  manual = atoi(rxed);
  rxCount++;
  rxLed = !rxLed;
}
void messageLocalArrived(MQTT::MessageData &md) {
  MQTT::Message &message = md.message;
  uint32_t len = md.message.payloadlen;
  char rxed[len+1];

  strncpy(&rxed[0], (char *) (&md.message.payload)[0], len);
  local = atoi(rxed);
  rxCount++;
  rxLed = !rxLed;
}
int scan_demo(WiFiInterface *wifi) {
  WiFiAccessPoint *ap;

  printf("Scan:\n");

  int count = wifi->scan(NULL, 0);

  if (count <= 0) {
    printf("scan() failed with return value: %d\n", count);
    return 0;
  }

  /* Limit number of network arbitrary to 15 */
  count = count < 15 ? count : 15;

  ap = new WiFiAccessPoint[count];
  count = wifi->scan(ap, count);

  if (count <= 0) {
    printf("scan() failed with return value: %d\n", count);
    return 0;
  }

  for (int i = 0; i < count; i++) {
    printf("Network: %s secured: %s BSSID: %hhX:%hhX:%hhX:%hhx:%hhx:%hhx RSSI: "
           "%hhd Ch: %hhd\n",
           ap[i].get_ssid(), sec2str(ap[i].get_security()),
           ap[i].get_bssid()[0], ap[i].get_bssid()[1], ap[i].get_bssid()[2],
           ap[i].get_bssid()[3], ap[i].get_bssid()[4], ap[i].get_bssid()[5],
           ap[i].get_rssi(), ap[i].get_channel());
  }
  printf("%d networks available.\n", count);

  delete[] ap;
  return count;
}

int main() {
  char buffer[80];
  uint32_t rc;
  lightsR = LEDOFF;
  lightsG = LEDOFF;
  lightsB = LEDOFF;
  printf("WiFi-MQTT example\n");
    pb1.mode(PullUp);
    // Delay for initial pullup to take effect
    ThisThread::sleep_for(100);
    // Setup Interrupt callback functions for a pb hit
    pb1.attach_deasserted(&pb1_hit_callback);

    // Start sampling pb inputs using interrupts
    pb1.setSampleFrequency();
/* Initialise display */
//  GUI_Init();
//  GUI_Clear();
//  GUI_SetFont(GUI_FONT_20B_1);
//  GUI_DispStringAt("Telemetry Data", 0, 0);


#ifdef MBED_MAJOR_VERSION
  printf("Mbed OS version %d.%d.%d\n\n", MBED_MAJOR_VERSION, MBED_MINOR_VERSION,
         MBED_PATCH_VERSION);
#endif

  WiFiInterface *wifi = WiFiInterface::get_default_instance();
  if (!wifi) {
    printf("ERROR: No WiFiInterface found.\n");
    return -1;
  }

  int count = scan_demo(wifi);
  if (count == 0) {
    printf("No WIFI APs found - can't continue further.\n");
    return -1;
  }

  printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
  int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD,
                          NSAPI_SECURITY_WPA_WPA2);
  if (ret != 0) {
    printf("\nConnection error: %d\n", ret);
    return -1;
  }

  printf("Success\n\n");
  printf("MAC: %s\n", wifi->get_mac_address());
  printf("IP: %s\n", wifi->get_ip_address());
  printf("Netmask: %s\n", wifi->get_netmask());
  printf("Gateway: %s\n", wifi->get_gateway());
  printf("RSSI: %d\n\n", wifi->get_rssi());
  myData.wifiStatus = ret;

  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
  data.clientID.cstring = (char *)THING_NAME;
  data.keepAliveInterval = 20;
  data.cleansession = 1;
  data.username.cstring = (char *)"";
  data.password.cstring = (char *)"";
  //    char *host = "10.0.0.2";
  char *host = (char *)MQTT_BROKER;
  uint16_t port = 1883;
  TCPSocket socket;
  MQTTClient client(&socket);
  socket.open(wifi);
  rc = socket.connect(host, port);
  if (rc == 0) {
    printf("Succesful connection of socket to Host %s port %d\n", host, port);
  } else {
    printf("Socket connection failed");
        while(socket.connect(host, port)){printf(".");}
  }
  rc = client.connect(data);
  if (rc == 0) {
    printf("Succesful connection of %s to Broker\n", data.clientID.cstring);
  } else {
    printf("Client connection failed");
  }
  MQTT::Message message{};
  sprintf(buffer, "Hello World! from My New Real Thang\r\n");
  message.qos = MQTT::QOS0;
  message.retained = false;
  message.dup = false;
  message.payload = (void *)buffer;
  message.payloadlen = strlen(buffer) + 1;

  rc = client.publish(ANNOUNCE_TOPIC, message);
  if (rc == 0)
    printf("publish announce worked\n");
  else {
    printf("publish announce failed %d\n", rc);
  }
  //message.payloadlen = 80;
  rc = client.subscribe((char *)LIGHT_THRESH_TOPIC, MQTT::QOS0, messageLightArrived);
  if (rc != 0)
    printf("Subscription Error %d\n", rc);
  else
    printf("Subscribed to %s\n", LIGHT_THRESH_TOPIC);
  rc = client.subscribe((char *)LIGHT_STATE_TOPIC, MQTT::QOS0, messageLightStateArrived);
  if (rc != 0)
    printf("Subscription Error %d\n", rc);
  else
    printf("Subscribed to %s\n", LIGHT_STATE_TOPIC);
  rc = client.subscribe((char *)REDLED_TOPIC, MQTT::QOS0, messageRedLedArrived);
  if (rc != 0)
    printf("Subscription Error %d\n", rc);
  else
    printf("Subscribed to %s\n", REDLED_TOPIC);
  rc = client.subscribe((char *)GREENLED_TOPIC, MQTT::QOS0, messageGreenLedArrived);
  if (rc != 0)
    printf("Subscription Error %d\n", rc);
  else
    printf("Subscribed to %s\n", GREENLED_TOPIC);
  rc = client.subscribe((char *)BLUELED_TOPIC, MQTT::QOS0, messageBlueLedArrived);
  if (rc != 0)
    printf("Subscription Error %d\n", rc);
  else
    printf("Subscribed to %s\n", BLUELED_TOPIC);
  client.subscribe((char *)MANUAL_TOPIC, MQTT::QOS0, messageManualArrived);
 if (rc != 0)
    printf("Subscription Error %d\n", rc);
  else
    printf("Subscribed to %s\n", MANUAL_TOPIC);
  sprintf(buffer, "%d", lthresh);
  message.payload = (void *)buffer;
  message.payloadlen = strlen(buffer) + 1;
  rc = client.publish(LIGHT_THRESH_TOPIC, message);
  if (rc == 0)
    printf("publish light threshold  worked\n");
  else {
    printf("publish light threshold failed %d\n", rc);
  }
   
   client.subscribe((char *)LIGHT_SWITCH_TOPIC, MQTT::QOS0, messageLightSwitchArrived);
 if (rc != 0)
    printf("Subscription Error %d\n", rc);
  else
    printf("Subscribed to %s\n", LIGHT_SWITCH_TOPIC);

  uint32_t lastRxCount = 0;
  ThisThread::sleep_for(2000);
  initialise();
  while (1) {

    statusLed = !statusLed;
    uint16_t lightPercent = lightLevel.read_u16() * 100 / 65535;
    sprintf(buffer, "%d", lightPercent);
    message.payload = (void *)buffer;
    message.payloadlen = strlen(buffer) + 1;

    //        if (lightDisplay != lastLightDisplay) {
    rc=client.publish(LIGHT_LEVEL_TOPIC, message);

    if (!manual && local) {
        printf("local and not manual Control\n");
        if (lightPercent < (lthresh-5)) {
            lightsR = LEDON;
            lightsG = LEDOFF;
            lightsB = LEDOFF;
        } else if (lightPercent > (lthresh + 5)) {
            lightsR = LEDOFF;
            lightsG = LEDOFF;
            lightsB = LEDON;
        } else {
            lightsR = LEDOFF;
            lightsG = LEDON;
            lightsB = LEDOFF;
        }

    }
    if (lightSwitch) {
        printf("<->");
        sprintf(buffer, "%d", lightState);
        message.payload = (void *)buffer;
        message.payloadlen = strlen(buffer) + 1;

        rc=client.publish(LIGHT_STATE_TOPIC, message);
        lightSwitch = false;
    }
    brdR = lightState;
    brdG = lightState;
    brdB = lightState;

    if (rc!=0) displayAt(ONOFF, STATUS, "publish failed");
    if (rxCount != lastRxCount) {
      sprintf( buffer, "%d", rxCount); // Current subscribed message received count
      displayAt(TLHTH, LLINE, buffer);
      sprintf(buffer, "%d", lthresh);
      displayAt(TLLTH, LLINE, buffer);
      lastRxCount = rxCount;
    }

    client.yield(1000);
  }
  wifi->disconnect();

  printf("\nDone\n");
}
/*******************************************************************************
 *
 * displayAt( column, line, string )
 *
 * Move the VT100 cursor to new coordinates on the display and print a string
 * of characters.
 *
 ******************************************************************************/
void displayAt( int col, int line, char *strBuffer )
{
    {
        printf( "\033[%d;%dH%s", line, col, strBuffer);
        fflush(stdout);
    }
}
/*******************************************************************************
 *
 * posCursor( column, line )
 *
 * Move the VT100 cursor to precise coordinates on the display
 *
 ******************************************************************************/
void posCursor(int col, int line)
{

    {
        printf( "\033[%d;%dH", line, col);
        fflush(stdout);
    }
}
/*******************************************************************************
 *
 * initialise()
 *
 * Called once following reset to setup the vt100 screen console display
 *
 ******************************************************************************/

void initialise()
{
    RIS;  // Full terminal reset just in case
    fflush(stdout);
    NTPClient ntpclient(wifi);                     // connect NTP Client
    time_t timestamp = ntpclient.get_timestamp();
    if (timestamp > 0) {                // timesatmp is valid if not less than 0
        set_time( timestamp );          // set local time to current network time
        myData.timeStatus = 1;
    }
    else {                              // unable to get ntp time successfully
        myData.timeStatus = 0;
    }
  
    drawBorders();  // Borders and static text
}

/*******************************************************************************
 *
 * drawBorders()
 *
 * Converts the grid of lines and corners defined in page[][] to a printable
 * string of characters appropriate to the font of the terminal emulator.
 * UTF8 character set is the most commonly used with vt100 on PUTTY or Teraterm.
 * Each shape is defined by three bytes in UTF8.
 * Other supported modes are POOR_MANS  +, -, | and VT100 one byte graphic 
 * characters these are non-standard and not universally supported in all or
 * even many fonts.
 *
 * Prints the Static Text in the appropriate locations on the display
 *
 ******************************************************************************/
void drawBorders()
{
    char outStr[1040];
    char outChar[4];
    int where;
    int charLen;
    CLS;
    HOME; // clear screen and move the cursor to 0, 0
    HIDE_CURSOR; // Turn off visible cursor[?25 lower case L
#ifdef GRAPHIC_SET
    printf("%c", GRAPHIC_SET);
#endif
    for (int line = 0; line < PAGELENGTH; line++) {
        where = 0;
        for (int column = 0; column < PAGEWIDTH; column++) {
            switch (page[line][column]) {
                case 'j':
                    charLen = strlen(HR);
                    strcpy(outChar, HR);
                    for (int i=0; i < charLen; i++) {
                        outStr[where++] = outChar[i];
                    }
                    break;
                case 'k':
                    charLen = strlen(TR);
                    strcpy(outChar, TR);
                    for (int i=0; i < charLen; i++) {
                        outStr[where++] = outChar[i];
                    }
                    break;
                case 'l':
                    charLen = strlen(TL);
                    strcpy(outChar, TL);
                    for (int i=0; i < charLen; i++) {
                        outStr[where++] = outChar[i];
                    }
                    break;
                case 'm':
                    charLen = strlen(HL);
                    strcpy(outChar, HL);
                    for (int i = 0; i < charLen; i++) {
                        outStr[where++] = outChar[i];
                    }
                    break;
                case 'n':
                    charLen = strlen(MC);
                    strcpy(outChar, MC);
                    for (int i = 0; i < charLen; i++) {
                        outStr[where++] = outChar[i];
                    }
                    break;
                case 'q':
                    charLen = strlen(HZ);
                    strcpy(outChar, HZ);
                    for (int i = 0; i < charLen; i++) {
                        outStr[where++] = outChar[i];
                    }
                    break;
                case 't':
                    charLen = strlen(VT);
                    strcpy(outChar, VT);
                    for (int i = 0; i < charLen; i++) {
                        outStr[where++] = outChar[i];
                    }
                    break;
                case 'u':
                    charLen = strlen(VR);
                    strcpy(outChar, VR);
                    for (int i = 0; i < charLen; i++) {
                        outStr[where++] = outChar[i];
                    }
                    break;
                case 'v':
                    charLen = strlen(HU);
                    strcpy(outChar, HU);
                    for (int i = 0; i < charLen; i++) {
                        outStr[where++] = outChar[i];
                    }
                    break;
                case 'w':
                    charLen = strlen(HD);
                    strcpy(outChar, HD);
                    for (int i = 0; i < charLen; i++) {
                        outStr[where++] = outChar[i];
                    }
                    break;
                case 'x':
                    charLen = strlen(VB);
                    strcpy(outChar, VB);
                    for (int i = 0; i < charLen; i++) {
                        outStr[where++] = outChar[i];
                    }
                    break;
                default:
                    outStr[where++] = page[line][column];  // Space or blank is needed
            }
        }
        outStr[where] = NULL;
        printf("%s", outStr);
    }
#ifdef TEXT_SET
    printf("%c", TEXT_SET);
#endif
    WHITE_BOLD; // set text color to white bold
    displayAt( FCOL, TLPOS, (char *) "Environmental Control System");
    BLUE_BOLD;
    displayAt( TLLTH-4, TLPOS, (char *)"Low Threshold");
    RED_BOLD;
    displayAt( TLHTH-5, TLPOS, (char *)"High Threshold");
    CYAN_BOLD;
    displayAt(FCOL, TLINE, (char *)"Temperature");
    displayAt(FCOL, LLINE, (char *)"Ambient Light Level");
    displayAt(FCOL, HEATL, (char *)"Heater");
    displayAt(FCOL, LIGHTL, (char *)"Lighting");
    WHITE_TEXT;
    displayAt(FCOL, USRHLP, (char *)"* Press \"Space key\" to adjust threshold values\r\n");
    displayAt(FCOL, USRHLP+1, (char *)"  - Use \"Tab key\" to select each threshold setting\r\n");
    displayAt(FCOL, USRHLP+2, (char *)"  - Hit \"Enter key\" to store new threshold value");
    displayAt(FCOL, USRHLP+3, (char *)"* Press \"M key\" to change auto/manual modes");
    displayAt(FCOL, USRHLP+4, (char *)"* Press \"T key\" to set the current time");
    fflush(stdout); // send the codes to the terminal

}
/*******************************************************************************
 *
 * Update RTC function
 *
 * Take a string from the console thread when the user sets the time.
 * Decodes the string and uses the data to set the time and date.
 *
 * TODO: needs some error checking as any errors in the input string causes 
 *       mbed to crash.
         Could use NTP to set time when wifi connectivity is added
 *
 ******************************************************************************/
void updateRealTimeClock(char *timeBuffer)
{
    char *tp;
    char *timeArray[6];
    int arrayIndex;
    struct tm struct_time;

    // extract number from string
    arrayIndex = 0;
    tp = strtok( timeBuffer, " /:-" );
    timeArray[arrayIndex++] = tp;
    while ( tp != NULL && arrayIndex < 6 ) {  // parse the values and assign to time array
        tp = strtok( NULL," /:-" );           // TODO: do some error checking
        timeArray[arrayIndex++] = tp;
    }
    // store number into time struct
    struct_time.tm_mday = atoi(timeArray[0]);
    struct_time.tm_mon  = atoi(timeArray[1]) - 1;
    struct_time.tm_year = atoi(timeArray[2]) - 1900;
    struct_time.tm_hour = atoi(timeArray[3]);
    struct_time.tm_min  = atoi(timeArray[4]);
    struct_time.tm_sec  = atoi(timeArray[5]);

    currentTime = mktime(&struct_time);
    set_time(currentTime);
}

