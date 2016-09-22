#ifndef WEBHANDLER_H
#define WEBHANDLER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer

void startServer(void);
void handleServer(void);
void secondsServer(void);
String ipString(IPAddress ip);
void parseParams(AsyncWebServerRequest *request);
void handleRoot(AsyncWebServerRequest *request);
void handleSettings(AsyncWebServerRequest *request);
String sDec(int t); // just 123 to 12.3 string
String timeFmt(void);
void handleS(AsyncWebServerRequest *request);
void handleJson(AsyncWebServerRequest *request);
void handleRemote(AsyncWebServerRequest *request);
void handleChart(AsyncWebServerRequest *request);
void handleNotFound(AsyncWebServerRequest *request);
String dataJson(void);

#endif // WEBHANDLER_H
