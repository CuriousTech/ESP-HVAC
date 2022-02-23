#ifndef OPENWEATHER_H
#define OPENWEATHER_H

#include <Arduino.h>
#ifdef ESP32
#include <AsyncTCP.h>
#else
#include <ESPAsyncTCP.h>
#endif

#include "forecast.h"

class OpenWeather
{
public:
  OpenWeather(void);
  void start(forecastData *pfd, bool bCelcius, char *pCityID);
  int checkStatus();
private:
  void _onConnect(AsyncClient* client);
  void _onDisconnect(AsyncClient* client);
  void _onData(AsyncClient* client, char* data, size_t len);
  int makeroom(uint32_t newTm);
  void processJson(char *p, int8_t event, const char **jsonList);
  char *skipwhite(char *p);
  void callback(int8_t iEvent, uint8_t iName, int32_t iValue, char *psValue);

  AsyncClient m_ac;
  forecastData *m_pfd;
  char m_cityID[8];

  char *m_pBuffer = NULL;
  int m_bufIdx;
  int m_fcIdx;
  int m_fcCnt;
  uint32_t m_lastTm;
  bool m_bFirst;
  bool m_bDone = false;
  bool m_bCelcius;
  int m_status;
};

#define OWBUF_SIZE 17500

#endif // OPENWEATHER_H
