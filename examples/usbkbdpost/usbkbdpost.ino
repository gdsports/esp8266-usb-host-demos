#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

ESP8266WiFiMulti wifiMulti;

// https://github.com/felis/USB_Host_Shield_2.0 library
#include <hidboot.h>
#include <usbhub.h>

const char SSID[] = "xxxxxxxx";
const char PASSWORD[] = "yyyyyyyyyyyyyyy";

char aLine[80 + 1]; //global line buffer
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
    aLine[sizeof(aLine) - 1] = '\0';
    aLineReady = true;
  }
};

USB     Usb;
HIDBoot<USB_HID_PROTOCOL_KEYBOARD>    HidKeyboard(&Usb);

KbdRptParser Prs;

void setup()
{
  int loops;

  Serial.begin( 115200 );
#if !defined(__MIPSEL__)
  while (!Serial); // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
#endif
  Serial.println();

  wifiMulti.addAP(SSID, PASSWORD);
  //wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
  //wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");
  Serial.println("Connecting Wifi...");
  loops = 200;
  while ((wifiMulti.run() != WL_CONNECTED) && loops--) {
    Serial.print(',');
    delay(100);
  }
  if (loops == 0) {
    Serial.println();
    Serial.print("WiFi connection FAILED : ");
    Serial.println(SSID);
    return;
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

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

  if (wifiMulti.run() == WL_CONNECTED) {
    if (aLineReady) {
      HTTPClient http;

      Serial.println(aLine);
      Serial.print("[HTTP] begin...\n");
      http.begin("http://posttestserver.com/post.php");
      http.addHeader("Content-Type", "application/x-www-form-urlencoded", false, true);

      Serial.print("[HTTP] GET...\n");
      // start connection and send HTTP header
      char postargs[89] = "barcode=";
      strcat(postargs, aLine);
      int httpCode = http.POST(postargs);

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK) {
          String payload = http.getString();
          Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();

      aLineIndex = 0;
      aLineReady = false;
    }
  }
  else {
    Serial.println("WiFi not connected!");
    delay(1000);
  }
}
