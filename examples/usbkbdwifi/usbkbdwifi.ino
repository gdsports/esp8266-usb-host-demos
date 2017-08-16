/**************************************************************************
MIT License

Copyright (c) 2017 gdsports625@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
**************************************************************************/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

ESP8266WiFiMulti wifiMulti;

// https://github.com/felis/USB_Host_Shield_2.0 library
#include <hidboot.h>
#include <usbhub.h>

const char SSID[] = "xxxxxxxx";
const char PASSWORD[] = "yyyyyyyyyyyyyyy";

//how many clients should be able to telnet to this ESP8266
#define MAX_SRV_CLIENTS 1
WiFiServer server(23);
WiFiClient serverClients[MAX_SRV_CLIENTS];

char aLine[80+1]; //global line buffer
int aLineIndex = 0;
bool aLineReady = false;

class KbdRptParser : public KeyboardReportParser
{
  protected:

    void OnKeyDown  (uint8_t mod, uint8_t key);
    void OnKeyPressed(uint8_t key);
};


void KbdRptParser::OnKeyDown(uint8_t mod, uint8_t key)
{
  uint8_t c = OemToAscii(mod, key);

  if (c)
    OnKeyPressed(c);
}

void KbdRptParser::OnKeyPressed(uint8_t key)
{
  if (!aLineReady) {
    if (aLineIndex < sizeof(aLine)) {
      if ((int)key == 19) // Carriage Return
      {
        aLine[aLineIndex] = '\0';
        aLineReady = true;
      }
      else {
        aLine[aLineIndex] = (char)key;
      }
      aLineIndex++;
    }
    else {
      aLine[sizeof(aLine)-1] = '\0';
      aLineReady = true;
    }
  }
};

USB     Usb;
HIDBoot<USB_HID_PROTOCOL_KEYBOARD>    HidKeyboard(&Usb);

KbdRptParser Prs;

void setup()
{
  Serial.begin( 115200 );
#if !defined(__MIPSEL__)
  while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
#endif
  Serial.println();

  wifiMulti.addAP(SSID, PASSWORD);
  //wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
  //wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");
  Serial.println("Connecting Wifi...");
  while(wifiMulti.run() != WL_CONNECTED) {
    Serial.print(',');
    delay(100);
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();
  server.setNoDelay(true);
  Serial.print("Ready! Use 'telnet ");
  Serial.print(WiFi.localIP());
  Serial.println(" 23' to connect");

  if (Usb.Init() == -1) {
    Serial.println(F("OSC did not start."));
    return;
  }
  Serial.println(F("USB host up"));

  HidKeyboard.SetReportParser(0, &Prs);
}

void loop()
{
  uint8_t i;

  Usb.Task();
  
  if(wifiMulti.run() == WL_CONNECTED) {
    //check if there are any new clients
    if (server.hasClient()){
      for(i = 0; i < MAX_SRV_CLIENTS; i++){
        //find free/disconnected spot
        if (!serverClients[i] || !serverClients[i].connected()){
          if(serverClients[i]) serverClients[i].stop();
          serverClients[i] = server.available();
          Serial.print("New client: "); Serial.print(i);
          continue;
        }
      }
      //no free/disconnected spot so reject
      WiFiClient serverClient = server.available();
      serverClient.stop();
    }
    //check clients for data
    for(i = 0; i < MAX_SRV_CLIENTS; i++){
      if (serverClients[i] && serverClients[i].connected()){
        if(serverClients[i].available()){
          //get data from the telnet client and discard it 
          while(serverClients[i].available()) serverClients[i].read();
        }
      }
    }
    if (aLineReady) {
      Serial.println(aLine);
      for(i = 0; i < MAX_SRV_CLIENTS; i++){
        if (serverClients[i] && serverClients[i].connected()){
          serverClients[i].println(aLine);
        }
      }
      aLineIndex = 0;
      aLineReady = false;
    }
  }
  else {
    Serial.println("WiFi not connected!");
    delay(1000);
  }
}
