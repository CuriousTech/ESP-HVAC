/**************************************************************
 * WiFiManager is a library for the ESP8266/Arduino platform
 * (https://github.com/esp8266/Arduino) to enable easy
 * configuration and reconfiguration of WiFi credentials and
 * store them in EEPROM.
 * inspired by http://www.esp8266.com/viewtopic.php?f=29&t=2520
 * https://github.com/chriscook8/esp-arduino-apboot
 * Built by AlexT https://github.com/tzapu
 * Licensed under MIT license
 **************************************************************/

#include "WiFiManager.h"
#include "ssd1306_i2c.h"
#include "icons.h"
#include "eeMem.h"

#define USE_OLED
#ifdef USE_OLED
extern SSD1306 display;
#endif

WiFiManager::WiFiManager()
{
}

void WiFiManager::autoConnect(char const *apName, const char *pPass) {
    _apName = apName;
    _pPass = pPass;

//  DEBUG_PRINT("");
//    DEBUG_PRINT("AutoConnect");
    
  if ( ee.szSSID[0] ) {
    DEBUG_PRINT("Waiting for Wifi to connect");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ee.szSSID, ee.szSSIDPassword);
    if ( hasConnected() )
    {
      _bCfg = false;
      return;
    }
  }
  //setup AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName);
  DEBUG_PRINT("Started Soft Access Point");
#ifdef USE_OLED
  IPAddress apIp = WiFi.softAPIP();
  display.print("AP started:");
  display.print(apIp.toString());
#endif
  DEBUG_PRINT(WiFi.softAPIP());
  DEBUG_PRINT("Don't forget the port #");

  if (!MDNS.begin(apName))
    DEBUG_PRINT("Error setting up MDNS responder!");
  WiFi.scanNetworks();

  _timeout = true;
  _bCfg = true;
}

boolean WiFiManager::hasConnected(void)
{
  for(int c = 0; c < 50; c++)
  {
    if (WiFi.status() == WL_CONNECTED)
      return true;
    delay(200);
    Serial.print(".");
#ifdef USE_OLED
    display.clear();
    display.drawXbm(34,10, 60, 36, WiFi_Logo_bits);
    display.setColor(INVERSE);
    display.fillRect(10, 10, 108, 44);
    display.setColor(WHITE);
    drawSpinner(4, c % 4);
    display.display();
#endif
  }
  DEBUG_PRINT("");
  DEBUG_PRINT("Could not connect to WiFi");
#ifdef USE_OLED
  display.print("No connection");
#endif
  return false;
}

bool WiFiManager::isCfg(void)
{
  return _bCfg;
}

void WiFiManager::setPass(const char *p){
  strncpy(ee.szSSIDPassword, p, sizeof(ee.szSSIDPassword) );
  eemem.update();
  DEBUG_PRINT("Updated EEPROM.  Restaring.");
  autoConnect(_apName, _pPass);
}

void WiFiManager::seconds(void) {
  static int s = 1; // do first list soon

  if(_timeout == false)
    return;
  if(--s)
    return;
  s = 60;
  int n = WiFi.scanNetworks(); // scan for stored SSID each minute
  if(n == 0 )
    return;

  for (int i = 0; i < n; i++)
  {
#ifdef USE_OLED
    display.print(WiFi.SSID(i));
#endif
    if(WiFi.SSID(i) == ee.szSSID) // found cfg SSID
    {
      DEBUG_PRINT("SSID found.  Restarting.");
      autoConnect(_apName, _pPass);
      s = 5; // set to 5 seconds in case it fails again
    }
  }
}

String WiFiManager::page()
{
  String s = HTTP_HEAD;
  s += HTTP_SCRIPT;
  s += HTTP_STYLE;
  s += HTTP_HEAD_END;

  for (int i = 0;  WiFi.SSID(i).length(); ++i)
  {
    DEBUG_PRINT(WiFi.SSID(i));
    DEBUG_PRINT(WiFi.RSSI(i));
    String item = HTTP_ITEM;
    item.replace("{v}", WiFi.SSID(i) );
    s += item;
  }
  WiFi.scanDelete();
  String form = HTTP_FORM;
  form.replace("$key", _pPass );
  s += form;
  s += HTTP_END;
  
  _timeout = false;
  return s;
}

String WiFiManager::urldecode(const char *src)
{
    String decoded = "";
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a')
                a -= 'a'-'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a'-'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            
            decoded += char(16*a+b);
            src+=3;
        } else if (*src == '+') {
            decoded += ' ';
            *src++;
        } else {
            decoded += *src;
            *src++;
        }
    }
    decoded += '\0';
    
    return decoded;
}

#ifdef USE_OLED
void WiFiManager::drawSpinner(int count, int active) {
  for (int i = 0; i < count; i++) {
    const char *xbm;
    if (active == i) {
       xbm = active_bits;
    } else {
       xbm = inactive_bits;  
    }
    display.drawXbm(64 - (12 * count / 2) + 12 * i,56, 8, 8, xbm);
  }   
}
#endif
