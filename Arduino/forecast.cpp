#include "Forecast.h"
#include "Display.h"
#include "TimeLib.h"
#include "eeMem.h"

// OWM Format: https://openweathermap.org/forecast5

extern void WsSend(String s); // for debug, open chrome console

// forecast retrieval

Forecast::Forecast()
{
  m_ac.onConnect([](void* obj, AsyncClient* c) { (static_cast<Forecast*>(obj))->_onConnect(c); }, this);
  m_ac.onDisconnect([](void* obj, AsyncClient* c) { (static_cast<Forecast*>(obj))->_onDisconnect(c); }, this);
  m_ac.onData([](void* obj, AsyncClient* c, void* data, size_t len) { (static_cast<Forecast*>(obj))->_onData(c, static_cast<char*>(data), len); }, this);

  for(int i = 0; i < FC_CNT; i++)
   m_fc.Data[i].temp = -1000;
  m_fc.Date = 0;
}

void Forecast::init(int16_t tzOff)
{
  m_tzOffset = tzOff;
}

// File retrieval start
void Forecast::start(IPAddress serverIP, uint16_t port, bool bCelcius, int8_t type)
{
  if(m_ac.connected() || m_ac.connecting())
    return;
  m_status = FCS_Busy;
  m_bCelcius = bCelcius;
  m_serverIP = serverIP;
  if(!m_ac.connect(serverIP, port))
    m_status = FCS_ConnectError;
  m_bLocal = true;
  m_type = type;
}

// OpenWeaterMap start
void Forecast::start(char *pCityID, bool bCelcius)
{
  if(m_ac.connected()  || m_ac.connecting())
    return;
  m_bCelcius = bCelcius;
  strcpy(m_cityID, pCityID);
  m_status = FCS_Busy;
  if(!m_ac.connect("api.openweathermap.org", 80))
    m_status = FCS_ConnectError;
  m_bLocal = false;
}

int Forecast::checkStatus()
{
  if(m_status == FCS_Done)
  {
    m_status = FCS_Idle;
    m_fc.loadDate = now();
    m_bUpdateFcstIdle = true;
    m_bFcstUpdated = true;
    return FCS_Done;
  }
  return m_status;
}

void Forecast::_onConnect(AsyncClient* client)
{
  String path = "GET /";

  if(m_bLocal) // file
  {
    switch(m_type)
    {
      case 0:
        path += "Forecast.log";
        break;
      case 1:
        path += "Forecast.json";
        break;
    }
    path += " HTTP/1.1\n"
      "Host: ";
    path += client->remoteIP().toString();
    path += "\n"
      "Connection: close\n"
      "Accept: */*\n\n";
  }
  else // OWM server
  {
    path = "GET /data/2.5/forecast?id=";
    path += m_cityID;
    path += "&appid=";
    path += APPID;   // Account
    path += "&units=";
    if(m_bCelcius)
      path += "celcius";
    else
      path += "imperial";
    path += " HTTP/1.1\n"
      "Host: ";
    path += client->remoteIP().toString();
    path += "\n"
      "Connection: close\n"
      "Accept: */*\n\n";
  }
  m_ac.add(path.c_str(), path.length());
  m_pBuffer = new char[OWBUF_SIZE];
  if(m_pBuffer) m_pBuffer[0] = 0;
  else m_status = FCS_MemoryError;
  m_bufIdx = 0;
}

// build file in chunks
void Forecast::_onData(AsyncClient* client, char* data, size_t len)
{
  if(m_pBuffer == NULL || m_bufIdx + len >= OWBUF_SIZE)
    return;
  memcpy(m_pBuffer + m_bufIdx, data, len);
  m_bufIdx += len;
  m_pBuffer[m_bufIdx] = 0;
}

// Remove most outdated entries to fill in new
int Forecast::makeroom(uint32_t newTm)
{
  if(m_fc.Date == 0) // not filled in yet
    return 0;
  uint32_t tm2 = m_fc.Date;
  int fcIdx;
  for(fcIdx = 0; fcIdx < FC_CNT-4 && m_fc.Data[fcIdx].temp != -1000; fcIdx++)
  {
    if(tm2 >= newTm)
      break;
    tm2 += m_fc.Freq;
  }
  if(fcIdx > (FC_CNT - m_fcCnt - 1)) // not enough room left
  {
    int n = fcIdx - (FC_CNT - 56);
    uint8_t *p = (uint8_t*)m_fc.Data;
    memcpy(p, p + (n*sizeof(forecastItem)), FC_CNT - (n*sizeof(forecastItem)) ); // make room
    m_fc.Date += m_fc.Freq * n;
    fcIdx -= n;
  }
  return fcIdx;
}

void Forecast::_onDisconnect(AsyncClient* client)
{
  (void)client;

  char *p = m_pBuffer;
  m_status = FCS_Done;
  if(p == NULL)
    return;
  if(m_bufIdx == 0)
  {
    delete m_pBuffer;
    return;
  }

  m_fcIdx = 0;
  m_bFirst = false;
  m_lastTm = 0;

  switch(m_type)
  {
    case 0:
      processCDT(); // text file type
      break;
    case 1:
      processOWM(); // json type
      break;
  }

  m_fc.Data[m_fcIdx].temp = -1000; // mark past last as invalid
  delete m_pBuffer;
}

void Forecast::processOWM()
{
  const char *jsonListOw[] = { // root keys
    "cod",     // 0 (200=good)
    "message", // 1 (0)
    "cnt",     // 2 list count (40)
    "list",    // 3 the list
    "city",    // 4 "id", "name", "coord", "country", "population", "timezone", "sunrise", "sunset"
    NULL
  };

  char *p = m_pBuffer;

  if(p[0] != '{') // local copy has no headers
    while(p[4]) // skip all the header lines
    {
      if(p[0] == '\r' && p[1] == '\n' && p[2] == '\r' && p[3] == '\n')
      {
        p += 4;
        break;
      }
      p++;
    }

  processJson(p, 0, jsonListOw);
}

// read data as comma delimited 'time,temp,rh,code' per line
void Forecast::processCDT()
{
  const char *p = m_pBuffer;
  m_status = FCS_Done;

  while(m_fcIdx < FC_CNT-1 && *p)
  {
    uint32_t tm = atoi(p); // this should be local time
    if(tm > 1700516696) // skip the headers
    {
      if(!m_bFirst)
      {
        m_bFirst = true;
        m_fcIdx = makeroom(tm);
        if(m_fc.Date == 0)
          m_fc.Date = tm;
      }
      else
      {
        m_fc.Freq = tm - m_lastTm;
      }
      m_lastTm = tm;
      while(*p && *p != ',') p ++;
      if(*p == ',') p ++;
      else break;
      m_fc.Data[m_fcIdx].temp = (atof(p)*10);
      while(*p && *p != ',') p ++;
      if(*p == ',') p ++;
      else break;
      m_fc.Data[m_fcIdx].humidity = (atof(p)*10);
      while(*p && *p != ',') p ++;
      if(*p == ',') p ++;
      {
        m_fc.Data[m_fcIdx].id = atoi(p);
      }
      m_fcIdx++;
    }
    while(*p && *p != '\r' && *p != '\n') p ++;
    while(*p == '\r' || *p == '\n') p ++;
  }
  m_fc.Data[m_fcIdx].temp = -1000;
  delete m_pBuffer;
}

bool Forecast::getCurrentIndex(int8_t& fcOff, int8_t& fcCnt, uint32_t& tm)
{
  fcOff = 0;
  fcCnt = 0;
  tm = m_fc.Date;

  if(m_fc.Date == 0) // not read yet or time not set
  {
    if(m_bUpdateFcstIdle)
      m_bUpdateFcst = true;
    return false;
  }

  for(fcCnt = 0; fcCnt < FC_CNT && m_fc.Data[fcCnt].temp != -1000; fcCnt++) // get current time in forecast and valid count
  {
    if( tm + m_fc.Freq < now() - m_tzOffset)
    {
      fcOff++;
      tm += m_fc.Freq;
    }
  }

  if(fcCnt >= FC_CNT || m_fc.Data[fcOff].temp == -1000 ) // entire list outdated
  {
    if(m_bUpdateFcstIdle)
      m_bUpdateFcst = true;
    return false;
  }

  return true;
}

void Forecast::callback(int8_t iEvent, uint8_t iName, int32_t iValue, char *psValue)
{
  switch(iEvent)
  {
    case 0: // root
      switch(iName)
      {
        case 0: // cod
          if(iValue != 200)
            m_status = FCS_Fail;
          break;
        case 1: // message
          break;
        case 2: // cnt
          m_fcCnt = iValue;
          break;
        case 3: // list
          {
            static const char *jsonList[] = {
              "dt",      // 0
              "main",    // 1
              "weather", // 2
//              "clouds",  // 3
//              "wind",
//              "visibility",
//              "pop",
//              "sys",
//              "dt_txt",
              NULL
            };

            processJson(psValue, 1, jsonList);
          }
          break;
        case 4: // city
          {
            static const char *jsonCity[] = {
              "id", // 0  3163858,
              "timezone", // 1 7200,
              "sunrise", // 2 1661834187,
              "sunset", // 3 1661882248
//              "name", //  "Zocca",
//              "coord", //  {"lat": 44.34, "lon": 10.99}
//              "country", // "IT",
//              "population", // 4593,
              NULL
            };
            processJson(psValue, 4, jsonCity);
          }
          break;
      }
      break;
    case 1: // list
      switch(iName)
      {
        case 0: // dt
          if(!m_bFirst)
          {
            m_bFirst = true;
            m_fcIdx = makeroom(iValue);
            if(m_fc.Date == 0) // first time uses external date, subsequent will shift
              m_fc.Date = iValue;
          }
          else
          {
            m_fc.Freq = iValue - m_lastTm; // figure out frequency of periods
          }
          m_lastTm = iValue;
          break;
        case 1: // main
          {
            const char *jsonList[] = {
              "temp",      // 0
              "feels_like", // 1
              "temp_min",   // 2
              "temp_max",  // 3
              "pressure",
              "humidity", // 5
              "temp_kf",
              NULL
            };
            processJson(psValue, 2, jsonList);
          }
          break;
        case 2: // weather
          {
            static const char *jsonList[] = {
              "id", // 802
              "main", // Clouds
              "description", // scattered clouds
              "icon", // 03d
              NULL
            };
            processJson(psValue, 3, jsonList);
          }
          if(m_fcIdx < FC_CNT - 1)
            m_fcIdx++;
          break;
      }
      break;
    case 2: // main
      switch(iName)
      {
        case 0: // temp
          m_fc.Data[m_fcIdx].temp = (atof(psValue)*10);
          break;
        case 5: // humidity
          m_fc.Data[m_fcIdx].humidity = (atoi(psValue)*10);
          break;
      }
      break;
    case 3: // weather
      switch(iName)
      {
        case 0: // id 804
          m_fc.Data[m_fcIdx].id = atoi(psValue);
          break;
        case 1: // main "Clouds"
          break;
        case 2: // description "Overcast clouds"
          break;
        case 3: // icon 04d (id has more values)
          break;
      }
      break;
    case 4: // city
      switch(iName)
      {
        case 0: // id
          break;
        case 1: // timezone
          break;
        case 2: // sunrise
          break;
        case 3: // sunset
          break;
      }
      break;
  }
}

void Forecast::processJson(char *p, int8_t event, const char **jsonList)
{
  char *pPair[2]; // param:data pair
  int8_t brace = 0;
  int8_t bracket = 0;
  int8_t inBracket = 0;
  int8_t inBrace = 0;

  while(*p)
  {
    p = skipwhite(p);
    if(*p == '{'){p++; brace++;}
    if(*p == '['){p++; bracket++;}
    if(*p == ',') p++;
    p = skipwhite(p);

    bool bInQ = false;
    if(*p == '"'){p++; bInQ = true;}
    pPair[0] = p;
    if(bInQ)
    {
       while(*p && *p!= '"') p++;
       if(*p == '"') *p++ = 0;
    }else
    {
      while(*p && *p != ':') p++;
    }
    if(*p != ':')
      return;

    *p++ = 0;
    p = skipwhite(p);
    bInQ = false;
    if(*p == '{') inBrace = brace+1; // data: {
    else if(*p == '['){p++; inBracket = bracket+1;} // data: [
    else if(*p == '"'){p++; bInQ = true;}
    pPair[1] = p;
    if(bInQ)
    {
       while(*p && *p!= '"') p++;
       if(*p == '"') *p++ = 0;
    }else if(inBrace)
    {
      while(*p && inBrace != brace){
        p++;
        if(*p == '{') inBrace++;
        if(*p == '}') inBrace--;
      }
      if(*p=='}') p++;
    }else if(inBracket)
    {
      while(*p && inBracket != bracket){
        p++;
        if(*p == '[') inBracket++;
        if(*p == ']') inBracket--;
      }
      if(*p == ']') *p++ = 0;
    }else while(*p && *p != ',' && *p != '\r' && *p != '\n') p++;
    if(*p) *p++ = 0;
    p = skipwhite(p);
    if(*p == ',') *p++ = 0;

    inBracket = 0;
    inBrace = 0;
    p = skipwhite(p);

    if(pPair[0][0])
    {
      for(int i = 0; jsonList[i]; i++)
      {
        if(!strcmp(pPair[0], jsonList[i]))
        {
          int32_t n = atol(pPair[1]);
          if(!strcmp(pPair[1], "true")) n = 1; // bool case
          callback(event, i, n, pPair[1]);
          break;
        }
      }
    }

  }
}

char *Forecast::skipwhite(char *p)
{
  while(*p == ' ' || *p == '\t' || *p =='\r' || *p == '\n')
    p++;
  return p;
}

void Forecast::getMinMax(int16_t& tmin, int16_t& tmax, int8_t offset, int8_t range)
{
  // Update min/max
  tmax = tmin = m_fc.Data[offset].temp;

  // Get min/max of current forecast
  for(int8_t i = offset + 1; i <  offset + range && m_fc.Data[i].temp != -1000 && i < FC_CNT; i++)
  {
    int16_t t = m_fc.Data[i].temp;
    if(tmin > t) tmin = t;
    if(tmax < t) tmax = t;
  }

  if(tmin == tmax) tmax++;   // div by 0 check
}

int16_t Forecast::getCurrentTemp(int& shiftedTemp, uint8_t shiftMins)
{
  int8_t fcOff;
  int8_t fcCnt;
  uint32_t tm;
  if(!getCurrentIndex(fcOff, fcCnt, tm))
    return 0;

  int16_t m = minute();
  uint32_t tmNow = now() - m_tzOffset;
  int16_t r = m_fc.Freq / 60; // usually 3 hour range (180 m)

  if( tmNow >= tm )
    m = (tmNow - tm) / 60;  // offset = minutes past forecast up to 179

  int16_t temp = tween(m_fc.Data[fcOff].temp, m_fc.Data[fcOff+1].temp, m, r);

  m += shiftMins; // get the adjust shift
  while(m >= r && fcOff < fcCnt - 2 && m_fc.Data[fcOff + 1].temp != -1000) // skip a window if 3h+ over range
  {
    fcOff++;
    m -= r;
  }

  while(m < 0 && fcOff) // skip a window if 3h+ prior to range
  {
    fcOff--;
    m += r;
  }
  if(m < 0) m = 0; // if just started up

  shiftedTemp = tween(m_fc.Data[fcOff].temp, m_fc.Data[fcOff+1].temp, m, r);

  return temp;
}

// get value at current minute between hours
int Forecast::tween(int16_t t1, int16_t t2, int m, int r)
{
  if(r == 0) r = 1; // div by zero check
  float t = (float)(t2 - t1) * (m * 100 / r) / 100;
  return (int)(t + (float)t1);
}
