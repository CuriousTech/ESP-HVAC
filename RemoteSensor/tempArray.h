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
//--
    int32_t voc:4;
    int32_t ch2o:4;
    int32_t error:6;
    int32_t res:18;
  };
};

struct tempArr
{
  mbits m;
};

struct LHLog
{
  uint16_t temp[2];
  uint16_t rh[2];
  uint16_t co2[2];
  uint8_t voc[2];
  uint8_t ch2o[2];
};

#define LOG_CNT 1440 // 1 day at 1 minute

class TempArray
{
public:
  TempArray(){};
  void init(uint16_t flags);
  void saveData(void);
  void update(uint16_t Values[]);
  void add(uint32_t date, AsyncWebSocket &ws, int WsClientID);
  void historyDump(bool bStart, AsyncWebSocket &ws, int WsClientID);

  bool m_bSilence;
  bool m_bValidDate;

protected:
  bool get(int &pidx, int n);
  void sendNew(uint16_t Values[], uint32_t date, AsyncWebSocket &ws, int WsClientID);
  void logLH(uint16_t Values[], LHLog log[], int idx);
  void resetLogEntry(LHLog log[], int idx);
  void checkAlert(String sName, bool bUD, uint16_t nNow, uint16_t nAlert);
  void rangeAlert(char *name, int16_t val);
  bool logDump(bool bStart, AsyncWebSocket &ws, int WsClientID, int logType);

  tempArr m_log[LOG_CNT];
  LHLog m_daily[7];
  LHLog m_weekly[53];
  int16_t m_idx;
  uint16_t m_dataFlags;
  uint32_t m_lastDate;
  uint16_t m_peakVal[DE_COUNT];
  uint16_t m_lastVal[DE_COUNT];
  uint16_t m_sampleCount;
  int8_t m_nWeek = -1;
  int8_t m_nWeekDay = -1;
  uint8_t m_nSending;
};

#endif
