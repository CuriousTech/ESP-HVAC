/*
  JsonClient.h - Arduino library for reading JSON data streamed or single request.
  Copyright 2014 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.

  1024 byte limit for data received
*/
#include "JsonClient.h"

// Initialize instance with a callback (event list index, name index from 0, integer value, string value)
JsonClient::JsonClient(void (*callback)(int16_t iName, int iValue, char *psValue, IPAddress ip), uint16_t nSize)
{
  m_callback = callback;
  m_Status = JC_IDLE;
  m_bufcnt = 0;
  m_ip = (uint32_t)0;
  m_bKeepAlive = false;
  m_szPath[0] = 0;
  m_nBufSize = nSize;
  m_ac.onConnect([](void* obj, AsyncClient* c) { (static_cast<JsonClient*>(obj))->_onConnect(c); }, this);
  m_ac.onDisconnect([](void* obj, AsyncClient* c) { (static_cast<JsonClient*>(obj))->_onDisconnect(c); }, this);
  m_ac.onTimeout([](void* obj, AsyncClient* c, uint32_t time) { (static_cast<JsonClient*>(obj))->_onTimeout(c, time); }, this);
  m_ac.onData([](void* obj, AsyncClient* c, void* data, size_t len) { (static_cast<JsonClient*>(obj))->_onData(c, static_cast<char*>(data), len); }, this);
  m_ac.onError([](void* obj, AsyncClient* c, err_t error) { (static_cast<JsonClient*>(obj))->_onError(c, error); }, this);
}

bool JsonClient::addQueue(IPAddress ip, String sUri, uint16_t port, String sJSON)
{
  uint8_t idx;

  for(idx = 0; idx < CQ_CNT; idx++)
  {
    if(queue[idx].port == 0)
      break;
  }
  if(idx == CQ_CNT)
  {
    return false;
  }
  queue[idx].ip = ip;
  queue[idx].pszHost = NULL;
  queue[idx].sUri = sUri;
  queue[idx].sJSON = sJSON;
  queue[idx].port = port;
  queue[idx].retries = 0;
  queue[idx].status = 0;
  return true;
}

bool JsonClient::addQueue(const char *pszHost, String sUri, uint16_t port, String sJSON)
{
  uint8_t idx;

  for(idx = 0; idx < CQ_CNT; idx++)
  {
    if(queue[idx].port == 0)
      break;
  }
  if(idx == CQ_CNT)
  {
    return false;
  }
  queue[idx].pszHost = pszHost;
  queue[idx].ip[0] = 0;
  queue[idx].sUri = sUri;
  queue[idx].sJSON = sJSON;
  queue[idx].port = port;
  queue[idx].retries = 0;
  queue[idx].status = 0;
  return true;
}


void JsonClient::service()
{
  static uint32_t ti = 0L;

  if(ti == 0L)
    ti = millis();

  if(millis() - ti < 500)
    return;

  uint8_t idx, cnt;
  for(idx = m_idx + 1, cnt = 0; idx != m_idx; idx++, cnt++)
  {
    if(idx >= CQ_CNT)
      idx = 0;
    if(cnt >= CQ_CNT)
      return;
    if(queue[idx].port && queue[idx].status != 1)
      break;
  }
  if(queue[idx].port == 0 || m_Status != JC_IDLE) // nothing to do
    return;

  m_idx = idx;

  if(queue[m_idx].retries > RETRIES)
  {
    queue[m_idx].port = 0;
    queue[m_idx].retries = 0;
    return;
  }

  queue[m_idx].status = 0;

  ti = millis();
  if(queue[m_idx].pszHost)
  {
    if ( begin(queue[m_idx].pszHost, queue[m_idx].sUri.c_str(), queue[m_idx].port, m_idx, false, (queue[m_idx].sJSON.length())?true:false, NULL, queue[m_idx].sJSON.c_str() ) )
      queue[m_idx].status = 1;
  }
  else
    if ( begin(queue[m_idx].ip, queue[m_idx].sUri.c_str(), queue[m_idx].port, m_idx, false, (queue[m_idx].sJSON.length())?true:false, NULL, queue[m_idx].sJSON.c_str() ) )
      queue[m_idx].status = 1;
}

// add a json list {"valname1", "valname2", "valname3", ..... NULL}
bool JsonClient::setList(const char **pList)
{
  m_jsonList = pList;
  return true;
}

// begin with host, /path?param=x&param=x, port, streaming
bool JsonClient::begin(const char *pHost, const char *pPath, uint16_t port, uint8_t idx, bool bKeepAlive, bool bPost, const char **pHeaders, const char *pData)
{
  m_pszHost = (char *)pHost;
  m_ip = (uint32_t)0L;
  m_idx = idx;
  return begin(pPath, port, bKeepAlive, bPost, pHeaders, pData);
}

// begin with host, /path?param=x&param=x, port, streaming
bool JsonClient::begin(IPAddress ip, const char *pPath, uint16_t port, uint8_t idx, bool bKeepAlive, bool bPost, const char **pHeaders, const char *pData)
{
  m_pszHost = (char *)"";
  m_ip = ip;
  m_idx = idx;
  return begin(pPath, port, bKeepAlive, bPost, pHeaders, pData);
}

bool JsonClient::begin(const char *pPath, uint16_t port, bool bKeepAlive, bool bPost, const char **pHeaders, const char *pData)
{
  if(m_pBuffer == NULL)
    m_pBuffer = new char[m_nBufSize];

  status(); // kill if timer > x
  if(m_ac.state() || m_Status != JC_IDLE)
    return false;
  m_bufcnt = 0;
  strncpy(m_szPath, pPath, sizeof(m_szPath) );
  m_szData[0] = 0;
  if(pData)
    if(pData[0])
      strncpy(m_szData, pData, sizeof(m_szData) );

  m_nPort = port;
  m_bKeepAlive = bKeepAlive;
  m_timer = millis();
  m_pHeaders = pHeaders;
  m_bPost = bPost;
  m_ac.setRxTimeout(2);
  m_ac.setAckTimeout(1000);
  m_ac.setNoDelay(true);
  queue[m_idx].status = 0;
  return connect();
}

void JsonClient::process(char *data)
{
  int nSize = strlen(data) + 1;

  if(m_pBuffer && m_nBufSize < nSize)
  {
    delete m_pBuffer;
    m_pBuffer = NULL;
  }
  if(m_nBufSize < nSize)
    m_nBufSize = nSize;
  if(m_pBuffer == NULL)
     m_pBuffer = new char[m_nBufSize];

  m_bufcnt = nSize - 1;
  strcpy(m_pBuffer, data);
  processLine();
  m_callback(-1, m_Status, (char *)"", m_ip);
}

// not used normally
void JsonClient::end()
{
  m_ac.stop();
  m_Status = JC_IDLE;
}

int JsonClient::status()
{
  return m_Status;
}

bool JsonClient::connect()
{
  m_Status = JC_CONNECTING;
  if(m_pszHost && m_pszHost[0])
  {
  	if( m_ac.connect(m_pszHost, m_nPort) )
      return true;
  }
  else
  {
	  if( m_ac.connect(m_ip, m_nPort) )
      return true;
  }

  m_Status = JC_NO_CONNECT;
  queue[m_idx].status = 2;
  queue[m_idx].retries++;
  m_callback(-1, m_Status, m_pszHost, m_ip);
  m_Status = JC_IDLE;
  return false;
}

void JsonClient::_onDisconnect(AsyncClient* client)
{
  (void)client;

  if(m_bKeepAlive == false)
  {
    m_Status = JC_DONE;
    if(m_bufcnt) // no LF at end?
    {
        m_pBuffer[m_bufcnt] = '\0';
        processLine();
    }
    m_Status = JC_IDLE;
    m_callback(-1, m_Status, m_pszHost, m_ip);
    if(queue[m_idx].status <= 1)
      queue[m_idx].port = 0;
    return;
  }
}

void JsonClient::_onTimeout(AsyncClient* client, uint32_t time)
{
  (void)client;
  m_callback(-1, JC_TIMEOUT, m_pszHost, m_ip);
  queue[m_idx].status = 3;
  queue[m_idx].retries++;
  m_Status = JC_IDLE;
}

String JsonClient::mkHeader(const char *pHeaderName, const char *pHeaderValue) // string
{
  String s = pHeaderName;
  s += ": ";
  s += pHeaderValue;
  s += "\r\n";
  return s;
}

String JsonClient::mkHeader(const char *pHeaderName, int nHeaderValue) // integer
{
  String s = pHeaderName;
  s += ": ";
  s += nHeaderValue;
  s += "\r\n";
  return s;
}

void JsonClient::_onConnect(AsyncClient* client)
{
  (void)client;

  String s;
  s = (m_bPost) ? "POST " : "GET ";
  s += m_szPath;
  s += " HTTP/1.1\r\n";

  String sHost = client->remoteIP().toString();
  if(m_pszHost && m_pszHost[0])
    sHost = m_pszHost;
  if(m_nPort != 80)
  {
    sHost += ":";
    sHost += m_nPort;
  }

  s += mkHeader("Host", sHost.c_str());
  s += mkHeader("User-Agent", "Arduino");
  if(m_szData[0])
    s += mkHeader("Content-Type", "application/json"); //"text/plain; charset=UTF-8");
  s += mkHeader("Connection", m_bKeepAlive ? "keep-alive" : "close");
  s += mkHeader("Accept", "*/*"); // use application/json for strict
  if(m_pHeaders)
  {
    for(int i = 0; m_pHeaders[i] && m_pHeaders[i+1]; i += 2)
    {
      s += mkHeader(m_pHeaders[i], m_pHeaders[i+1]);
    }
  }
  if(m_szData[0])
    s += mkHeader("Content-Length", strlen(m_szData) + 1);

  s += "\r\n";
  if(m_szData[0])
  {
    s += m_szData;
    s += "\r\n";
  }

  client->add(s.c_str(), s.length());
  m_brace = 0;
  m_Status = JC_CONNECTED;

  m_callback(-1, m_Status, m_pszHost, m_ip);
}

void JsonClient::_onData(AsyncClient* client, char* data, size_t len)
{
  (void)client;
  if(m_pBuffer == NULL)
    return;

  for(int i = 0; i < len; i++)
  {
    char c = data[i];
    if(c != '\r' && c != '\n' && m_bufcnt < m_nBufSize)
    {
      m_pBuffer[m_bufcnt++] = c;
      m_pBuffer[m_bufcnt] = 0;
    }
    if(c == '\r' || c == '\n')
    {
      if(m_bufcnt > 1) // ignore keepalive
      {
        m_pBuffer[m_bufcnt-1] = '\0';
        processLine();
      }
      m_bufcnt = 0;
    }
  }
  m_timer = millis();
}

void JsonClient::processLine()
{
  if(m_jsonList == NULL)
    return;

  char *pPair[2]; // param:data pair

  char *p = m_pBuffer;

  while(*p)
  {
    p = skipwhite(p);
    if(*p == '{'){p++; m_brace++;}
    if(*p == '['){p++; m_bracket++;}
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
    if(*p == '{') m_inBrace = m_brace+1; // data: {
    else if(*p == '['){p++; m_inBracket = m_bracket+1;} // data: [
    else if(*p == '"'){p++; bInQ = true;}
    pPair[1] = p;
    if(bInQ)
    {
       while(*p && *p!= '"') p++;
       if(*p == '"') *p++ = 0;
    }else if(m_inBrace)
    {
      while(*p && m_inBrace != m_brace){
        p++;
        if(*p == '{') m_inBrace++;
        if(*p == '}') m_inBrace--;
      }
      if(*p=='}') p++;
    }else if(m_inBracket)
    {
      while(*p && m_inBracket != m_bracket){
        p++;
        if(*p == '[') m_inBracket++;
        if(*p == ']') m_inBracket--;
      }
      if(*p == ']') *p++ = 0;
    }else while(*p && *p != ',' && *p != '\r' && *p != '\n' && *p != '}') p++;
    if(*p) *p++ = 0;
    p = skipwhite(p);
    if(*p == ',') *p++ = 0;

    m_inBracket = 0;
    m_inBrace = 0;
    p = skipwhite(p);

    if(pPair[0][0])
    {
      for(int i = 0; m_jsonList[i]; i++)
      {
        if( !strcmp(pPair[0], m_jsonList[i]) )
        {
            int32_t n = atoi(pPair[1]);
            if(!strcmp(pPair[1], "true")) n = 1; // bool case
            m_callback(i, n, pPair[1], m_ip);
            break;
        }
      }
    }
  }
}

char * JsonClient::skipwhite(char *p)
{
  while(*p == ' ' || *p == '\t' || *p =='\r' || *p == '\n')
    p++;
  return p;
}

void JsonClient::_onError(AsyncClient* client, int8_t error)
{
  m_Status = JC_ERROR;
  queue[m_idx].status = 4;
  queue[m_idx].retries++;
  m_callback(-1, error, m_pszHost, m_ip);
}
