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

#ifndef WiFiManager_h
#define WiFiManager_h

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#define DEBUG //until arduino ide can include defines at compile time from main sketch

#ifdef DEBUG
#define DEBUG_PRINT(x)  Serial.println(x)
#else
#define DEBUG_PRINT(x)
#endif


class WiFiManager
{
public:
    WiFiManager();
    void autoConnect(char const *apName, const char *pPass);
    String page(void);
    void seconds(void);
    void setPass(const char *p);
    bool isCfg(void);

    boolean hasConnected();

    //for convenience
    String urldecode(const char*);
private:
    const int WM_DONE = 0;
    const int WM_WAIT = 10;
    bool _timeout;
    bool _bCfg;

    const String HTTP_HEAD = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/><title>Config ESP</title>";
    const String HTTP_STYLE = "<style>div,input {margin-bottom: 5px;}body{width:200px;display:block;margin-left:auto;margin-right:auto;}</style>";
    const String HTTP_SCRIPT = "<script>function c(l){document.getElementById('s').value=l.innerText||l.textContent;document.getElementById('p').focus();}</script>";
    const String HTTP_HEAD_END = "</head><body>";
    const String HTTP_ITEM = "<div><a href='#' onclick='c(this)'>{v}</a></div>";
    const String HTTP_FORM = "<form method='get' action='s'><input id='s' name='ssid' length=32 placeholder='SSID'><input id='p' name='pass' length=64 placeholder='password'><input type='hidden' name='key' value='$key' ><br/><input type='submit'></form>";
    const String HTTP_END = "</body></html>";
    
    const char* _apName = "no-net";
    const char *_pPass = "";
    void drawSpinner(int count, int active);
};

#endif
