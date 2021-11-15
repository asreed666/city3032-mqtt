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



#define LEDON 0
#define LEDOFF 1
#define MQTT_BROKER "192.168.1.174"
#define THING_NAME "ThisThing"
#define LIGHT_LEVEL_TOPIC "mytopic/light"
#define ANNOUNCE_TOPIC "mytopic/announce"
#define LIGHT_THRESH_TOPIC "mytopic/lthresh"

WiFiInterface *wifi;

AnalogIn lightLevel(P10_0);
DigitalOut statusLed(LED1);
DigitalOut rxLed(LED2);
DigitalOut lightsR(LED3);
DigitalOut lightsG(LED4);
DigitalOut lightsB(LED5);
uint32_t rxCount = 0;
uint32_t lthresh = 50;

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
void messageArrived(MQTT::MessageData &md) {
  MQTT::Message &message = md.message;
  uint32_t len = md.message.payloadlen;
  //(&md.message.payload)[len-1] = 0;
  //int i = 0;
  char rxed[len+1];
  strncpy(&rxed[0], (char *) (&md.message.payload)[0], len);
  lthresh = atoi(rxed);
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
  message.payloadlen = 80;
  rc = client.subscribe((char *)LIGHT_THRESH_TOPIC, MQTT::QOS0, messageArrived);
  if (rc != 0)
    printf("Subscription Error %d\n", rc);
  else
    printf("Subscribed to %s\n", LIGHT_THRESH_TOPIC);
  sprintf(buffer, "%d", lthresh);
  message.payload = (void *)buffer;
  message.payloadlen = strlen(buffer) + 1;
  rc = client.publish(LIGHT_THRESH_TOPIC, message);
  if (rc == 0)
    printf("publish light threshold  worked\n");
  else {
    printf("publish light threshold failed %d\n", rc);
  }
  uint32_t lastRxCount = 0;
  while (1) {

    statusLed = !statusLed;
    uint16_t lightPercent = lightLevel.read_u16() * 100 / 65535;
    sprintf(buffer, "Light Level is %d\r\n", lightPercent);
    message.payload = (void *)buffer;
    message.payloadlen = strlen(buffer) + 1;

    //        if (lightDisplay != lastLightDisplay) {
    rc=client.publish(LIGHT_LEVEL_TOPIC, message);

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
    if (rc!=0) printf("publish failed\n");
    if (rxCount != lastRxCount) {
      printf("Current subscribed message received count = %d\n", rxCount);
      printf("lthresh is now = %d\n", lthresh);
      lastRxCount = rxCount;
    }

    client.yield(1000);
  }
  wifi->disconnect();

  printf("\nDone\n");
}
