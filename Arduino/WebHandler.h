#ifndef WEBHANDLER_H
#define WEBHANDLER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer

void startServer(void);
void handleServer(void);
bool secondsServer(void);
void parseParams(AsyncWebServerRequest *request);
String dataJson(void);
void WsSend(String s);
void historyDump(bool bStart);
void appendDump(uint32_t startTime);

#endif // WEBHANDLER_H
