#ifndef FORECAST_H
#define FORECAST_H

#include <Arduino.h>
#ifdef ESP32
#include <AsyncTCP.h>
#else
#include <ESPAsyncTCP.h>
#endif

#define FC_CNT 74

struct forecastData
{
  uint32_t Date;
  uint16_t Freq;
  int8_t Data[FC_CNT];
};

class Forecast
{
public:
  Forecast(void);
  void start(IPAddress serverIP, uint16_t port, forecastData *pfd);
  bool checkStatus();
private:
  void _onConnect(AsyncClient* client);
  void _onDisconnect(AsyncClient* client);
  void _onData(AsyncClient* client, char* data, size_t len);
  int makeroom(uint32_t newTm);

  IPAddress m_serverIP;
  AsyncClient m_ac;
  forecastData *m_pfd;
#define FCBUF_SIZE 1200
  char *m_pBuffer = NULL;
  int m_bufIdx;
  bool m_bDone = false;
};
#endif // FORECAST_H
