#include "Forecast.h"

// local server forecast retrieval

Forecast::Forecast()
{
  m_ac.onConnect([](void* obj, AsyncClient* c) { (static_cast<Forecast*>(obj))->_onConnect(c); }, this);
  m_ac.onDisconnect([](void* obj, AsyncClient* c) { (static_cast<Forecast*>(obj))->_onDisconnect(c); }, this);
  m_ac.onData([](void* obj, AsyncClient* c, void* data, size_t len) { (static_cast<Forecast*>(obj))->_onData(c, static_cast<char*>(data), len); }, this);
}

void Forecast::start(IPAddress serverIP, uint16_t port, forecastData *pfd, bool bCelcius)
{
    if(m_ac.connected())
      return;
    m_pfd = pfd;
    m_status = FCS_Busy;
    m_bCelcius = bCelcius;
    m_serverIP = serverIP;
    if(!m_ac.connect(serverIP, port))
      m_status = FCS_ConnectError;
}

int Forecast::checkStatus()
{
  if(m_status == FCS_Done)
  {
    m_status = FCS_Idle;
    return FCS_Done;
  }
  return m_status;
}

void Forecast::_onConnect(AsyncClient* client)
{
  (void)client;

  String s = "GET /Forecast.log HTTP/1.1\n"
    "Host: ";
  s += m_serverIP.toString();
  s += "\n"
    "Connection: close\n"
    "Accept: */*\n\n";

  m_ac.add(s.c_str(), s.length());
  m_pBuffer = new char[FCBUF_SIZE];
  if(m_pBuffer) m_pBuffer[0] = 0;
  else m_status = FCS_MemoryError;
  m_bufIdx = 0;
}

// build file in chunks
void Forecast::_onData(AsyncClient* client, char* data, size_t len)
{
  if(m_pBuffer == NULL || m_bufIdx + len >= FCBUF_SIZE)
    return;
  memcpy(m_pBuffer + m_bufIdx, data, len);
  m_bufIdx += len;
  m_pBuffer[m_bufIdx] = 0;
}

int Forecast::makeroom(uint32_t newTm)
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
  if(fcIdx > (FC_CNT - 56)) // not enough room left
  {
    int n = fcIdx - (FC_CNT - 56);
    uint8_t *p = (uint8_t*)m_pfd->Data;
    memcpy(p, p + n, FC_CNT - n); // make room
    m_pfd->Date += m_pfd->Freq * n;
    fcIdx -= n;
  }
  return fcIdx;
}

// read data as comma delimited 'time,temp,rh' per line
void Forecast::_onDisconnect(AsyncClient* client)
{
  (void)client;

  const char *p = m_pBuffer;
  m_status = FCS_Done;
  if(p == NULL)
    return;
  if(m_bufIdx == 0)
  {
    delete m_pBuffer;
    return;
  }

  int fcIdx = 0;
  bool bFirst = false;
  uint32_t lastTm = 0;

  while(fcIdx < FC_CNT-1 && *p)
  {
    uint32_t tm = atoi(p);
    if(tm > 15336576) // skip the headers
    {
      if(!bFirst)
      {
        bFirst = true;
        fcIdx = makeroom(tm);
        if(m_pfd->Date == 0)
          m_pfd->Date = tm;
      }
      else
      {
        m_pfd->Freq = tm - lastTm;
      }
      lastTm = tm;
      while(*p && *p != ',') p ++;
      if(*p == ',') p ++;
      else break;
      m_pfd->Data[fcIdx] = atoi(p);
      fcIdx++;
    }
    while(*p && *p != '\r' && *p != '\n') p ++;
    while(*p == '\r' || *p == '\n') p ++;
  }
  m_pfd->Data[fcIdx] = -127;
  delete m_pBuffer;
}
