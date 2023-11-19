#include "Openweathermap.h"
// Openwathermap forecast reader

OpenWeather::OpenWeather()
{
  m_ac.onConnect([](void* obj, AsyncClient* c) { (static_cast<OpenWeather*>(obj))->_onConnect(c); }, this);
  m_ac.onDisconnect([](void* obj, AsyncClient* c) { (static_cast<OpenWeather*>(obj))->_onDisconnect(c); }, this);
  m_ac.onData([](void* obj, AsyncClient* c, void* data, size_t len) { (static_cast<OpenWeather*>(obj))->_onData(c, static_cast<char*>(data), len); }, this);
}

void OpenWeather::start(forecastData *pfd, bool bCelcius, char *pCityID)
{
  if(m_ac.connected())
    return;
  m_pfd = pfd;
  m_bCelcius = bCelcius;
  strcpy(m_cityID, pCityID);
  m_status = FCS_Busy;
  if(!m_ac.connect("api.openweathermap.org", 80))
    m_status = FCS_ConnectError;
}

int OpenWeather::checkStatus()
{
  if(m_status == FCS_Done)
  {
    m_status = FCS_Idle;
    return FCS_Done;
  }
  return m_status;
}

void OpenWeather::_onConnect(AsyncClient* client)
{
  String path = "GET /data/2.5/forecast?id=";
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

  m_ac.add(path.c_str(), path.length());
  m_pBuffer = new char[OWBUF_SIZE];
  if(m_pBuffer) m_pBuffer[0] = 0;
  else m_status = FCS_MemoryError;
  m_bufIdx = 0;
}

// build file in chunks
void OpenWeather::_onData(AsyncClient* client, char* data, size_t len)
{
  if(m_pBuffer == NULL || m_bufIdx + len >= OWBUF_SIZE)
  {
    return;
  }
  memcpy(m_pBuffer + m_bufIdx, data, len);
  m_bufIdx += len;
  m_pBuffer[m_bufIdx] = 0;
}

int OpenWeather::makeroom(uint32_t newTm)
{
  if(m_pfd->Date == 0) // not filled in yet
    return 0;
  uint32_t tm2 = m_pfd->Date;
  int fcIdx;
  for(fcIdx = 0; fcIdx < FC_CNT-4 && m_pfd->Data[fcIdx] != -127; fcIdx++)
  {
    if(tm2 >= newTm)
      break;
    tm2 += m_pfd->Freq;
  }
  if(fcIdx > (FC_CNT - m_fcCnt - 1)) // not enough room left
  {
    int n = fcIdx - (FC_CNT - m_fcCnt - 1);
    uint8_t *p = (uint8_t*)m_pfd->Data;
    memcpy(p, p + n, FC_CNT - n); // make room
    m_pfd->Date += m_pfd->Freq * n;
    fcIdx -= n;
  }
  return fcIdx;
}

void OpenWeather::_onDisconnect(AsyncClient* client)
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

  const char *jsonListOw[] = { // root values
    "cod",     // 0 (200=good)
    "message", // 1 (0)
    "cnt",     // 2 list count (40)
    "list",    // 3 the list
    "city",    // 4 "id", "name", "coord", "country", "population", "timezone", "sunrise", "sunset"
    NULL
  };

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
  m_pfd->Data[m_fcIdx] = -127;
  delete m_pBuffer;
}

void OpenWeather::callback(int8_t iEvent, uint8_t iName, int32_t iValue, char *psValue)
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
            const char *jsonList[] = {
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
            if(m_pfd->Date == 0)
              m_pfd->Date = iValue;
          }
          else
          {
            m_pfd->Freq = iValue - m_lastTm;
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
            const char *jsonList[] = {
              "id", // 802
              "main", // Cluods
              "description", // scattered clouds
              "icon", // 03d
              NULL
            };
            processJson(psValue, 3, jsonList);
        }
        break;
      }
      break;
    case 2: // main
      switch(iName)
      {
        case 0: // temp
          m_pfd->Data[m_fcIdx] = iValue;
          m_fcIdx++;
          break;
        case 5: // humidity
          break;
      }
      break;
    case 3: // weather
      switch(iName)
      {
        case 0: // id
          break;
        case 1: // main
          break;
        case 2: // description
          break;
        case 3: // icon
          break;
      }
      break;
  }
}

void OpenWeather::processJson(char *p, int8_t event, const char **jsonList)
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

char * OpenWeather::skipwhite(char *p)
{
  while(*p == ' ' || *p == '\t' || *p =='\r' || *p == '\n')
    p++;
  return p;
}
