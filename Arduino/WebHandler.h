#ifndef WEBHANDLER_H
#define WEBHANDLER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer

void startServer(void);
void handleServer(void);
void secondsServer(void);
String ipString(IPAddress ip);
void parseParams(AsyncWebServerRequest *request);
String sDec(int t); // just 123 to 12.3 string
String timeFmt(void);
String dataJson(void);
void WsSend(String s);
void fc_onConnect(AsyncClient* client);
void fc_onData(AsyncClient* client, char* data, size_t len);
void fc_onDisconnect(AsyncClient* client);
void fc_onTimeout(AsyncClient* client, uint32_t time);
void historyDump(bool bStart);
void appendDump(int startTime);
String forecastJson(void);
#endif // WEBHANDLER_H
