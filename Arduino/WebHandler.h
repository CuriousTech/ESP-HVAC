#ifndef WEBHANDLER_H
#define WEBHANDLER_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

void startServer(void);
void handleServer(void);
void secondsServer(void);
String ipString(IPAddress ip);
void parseParams(void);
void handleRoot(void);
String sDec(int t); // just 123 to 12.3 string
String timeFmt(void);
void handleS(void);
void handleJson(void);
void handleEvents(void);
void handleRemote(void);
void handleNotFound(void);
String dataJson(void);
void pushBullet(const char *pTitle, const char *pBody);

#endif // WEBHANDLER_H
