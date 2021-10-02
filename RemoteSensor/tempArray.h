#ifndef TEMPARRAY_H
#define TEMPARRAY_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "defs.h"

typedef union mbits
{
  uint32_t u[2];
  struct
  {
    uint32_t tmdiff:12;
    int32_t rh:7;
    int32_t temp:7;
    int32_t co2:6;
    int32_t voc:4;
    int32_t ch2o:4;
  };
};

struct tempArr{
  mbits m;
};

#define LOG_CNT 1440 // 1 day at 1 minute

class TempArray
{
public:
  TempArray(){};
  void update(uint16_t *pValues);
  void add(uint16_t flags, uint32_t date, AsyncWebSocket &ws, int WsClientID);
  void historyDump(bool bStart, AsyncWebSocket &ws, int WsClientID);
protected:
  bool get(tempArr *pa, int n);
  void sendNew(uint16_t *pValues, uint16_t flags, uint32_t date, AsyncWebSocket &ws, int WsClientID);

  tempArr m_log[LOG_CNT];
  int16_t m_idx;
  uint32_t m_lastDate;
  uint16_t m_peakVal[6];
  uint16_t m_lastVal[6];
  uint16_t m_flags;
};

#endif
