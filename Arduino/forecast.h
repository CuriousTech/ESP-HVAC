#ifndef FORECAST_H
#define FORECAST_H

#include <Arduino.h>
#ifdef ESP32
#include <AsyncTCP.h>
#else
#include <ESPAsyncTCP.h>
#endif

#define APPID "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" // OpenWeathermap APP ID

#define FC_CNT 74 // Max forecast items

struct forecastItem
{
  int16_t temp;
  int16_t humidity;
  int16_t id;
};

struct forecastData
{
  uint32_t Date;
  uint32_t loadDate;
  uint16_t Freq;
  forecastItem Data[FC_CNT];
};

enum FCS_Status
{
  FCS_Idle,
  FCS_Busy,
  FCS_Done,
  FCS_ConnectError,
  FCS_Fail,
  FCS_MemoryError,
};

class Forecast
{
public:
  Forecast(void);
  void init(int16_t tzOff);

  void start(IPAddress serverIP, uint16_t port, bool bCelcius, int8_t type);
  void start(char *pCityID, bool bCelcius); // Openweathermmap start
  int checkStatus();
  bool getCurrentIndex(int8_t& fcOff, int8_t& fcCnt, uint32_t& tm); // index into stored data at current timeframme
  void getMinMax(int16_t& tmin, int16_t& tmax, int8_t offset, int8_t range);
  int16_t getCurrentTemp(int& shiftedTemp, uint8_t shiftMins);
  int tween(int16_t t1, int16_t t2, int m, int r);

private:
  void _onConnect(AsyncClient* client);
  void _onDisconnect(AsyncClient* client);
  void _onData(AsyncClient* client, char* data, size_t len);
  void processCDT(void);
  void processOWM(void);
  void processJson(char *p, int8_t event, const char **jsonList);
  int makeroom(uint32_t newTm);
  char *skipwhite(char *p);
  void callback(int8_t iEvent, uint8_t iName, int32_t iValue, char *psValue);

  IPAddress m_serverIP;
  AsyncClient m_ac;
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
  bool m_bLocal;
  int8_t m_type;
  int16_t m_tzOffset;
public:
  forecastData m_fc;
  bool    m_bUpdateFcst = true;
  bool    m_bUpdateFcstIdle = true;
  bool    m_bFcstUpdated = false;
};

#define OWBUF_SIZE 17500

#endif // FORECAST_H
