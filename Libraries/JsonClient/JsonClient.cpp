/*
  JsonClient.h - Arduino library for reading JSON data streamed or single request.
  Copyright 2014 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.

  1024 byte limit for data received
  8 lists max per instance
*/
#include "JsonClient.h"

// Initialize instance with a callback (event list index, name index from 0, integer value, string value)
JsonClient::JsonClient(void (*callback)(int16_t iEvent, uint16_t iName, int iValue, char *psValue), uint16_t nSize )
{
  m_callback = callback;
  m_Status = JC_IDLE;
  m_jsonCnt = 0;
  m_bufcnt = 0;
  m_event = 0;
  m_bKeepAlive = false;
  m_szPath[0] = 0;
  m_nBufSize = nSize;
  m_ac.onConnect([](void* obj, AsyncClient* c) { (static_cast<JsonClient*>(obj))->_onConnect(c); }, this);
  m_ac.onDisconnect([](void* obj, AsyncClient* c) { (static_cast<JsonClient*>(obj))->_onDisconnect(c); }, this);
  m_ac.onTimeout([](void* obj, AsyncClient* c, uint32_t time) { (static_cast<JsonClient*>(obj))->_onTimeout(c, time); }, this);
  m_ac.onData([](void* obj, AsyncClient* c, void* data, size_t len) { (static_cast<JsonClient*>(obj))->_onData(c, static_cast<char*>(data), len); }, this);
}

// add a json list {"event name", "valname1", "valname2", "valname3", NULL}
// If first string is "" or NULL, the data is expected as JSON without an event name
// If second string is "" or NULL, the event name is expected, but the "data:" string is assumed non-JSON
bool JsonClient::addList(const char **pList)
{
  for(int i = 0; i < m_jsonCnt; i++)
    if(m_jsonList[i] == pList)
      return true;
  if(m_jsonCnt >= LIST_CNT)
    return false;
  m_jsonList[m_jsonCnt++] = pList;
  return true;
}

// begin with host, /path?param=x&param=x, port, streaming
bool JsonClient::begin(const char *pHost, const char *pPath, uint16_t port, bool bKeepAlive, bool bPost, const char **pHeaders, char *pData, uint16_t to)
{
  m_pszHost = (char *)pHost;
  return begin(pPath, port, bKeepAlive, bPost, pHeaders, pData, to);
}

// begin with host, /path?param=x&param=x, port, streaming
bool JsonClient::begin(IPAddress ip, const char *pPath, uint16_t port, bool bKeepAlive, bool bPost, const char **pHeaders, char *pData, uint16_t to)
{
  m_pszHost = (char *)"";
  m_ip = ip;
  return begin(pPath, port, bKeepAlive, bPost, pHeaders, pData, to);
}

bool JsonClient::begin(const char *pPath, uint16_t port, bool bKeepAlive, bool bPost, const char **pHeaders, char *pData, uint16_t to)
{
  if(m_pBuffer == NULL)
    m_pBuffer = new char[m_nBufSize];

  if(m_ac.state())
    return false;
  if(m_Status != JC_IDLE)
    return false;
  if(m_timer && millis() > m_timer + m_to * 1000)
  {
    m_ac.close(true);
    m_timer = 0;
    return false;
  }
  m_jsonCnt = 0;
  m_event = 0;
  m_bufcnt = 0;
  strncpy(m_szPath, pPath, sizeof(m_szPath) );
  m_szData[0] = 0;
  if(pData)
    strncpy(m_szData, pData, sizeof(m_szData) );

  m_nPort = port;
  m_bKeepAlive = bKeepAlive;
  m_timer = millis();
  m_pHeaders = pHeaders;
  m_bPost = bPost;
  m_retryCnt = 0;
  m_to = to;
  m_ac.setRxTimeout(to);
  m_ac.setAckTimeout(to*1000);
  return connect();
}

void JsonClient::process(char *event, char *data)
{
  m_event = 0;

  strcpy(m_pBuffer, "event:");
  strcat(m_pBuffer, event);
  strcat(m_pBuffer, "\r\n");
  m_bufcnt = strlen(m_pBuffer);
  processLine();

  strcpy(m_pBuffer, "data:");
  strcat(m_pBuffer, data);
  strcat(m_pBuffer, "\r\n");
  m_bufcnt = strlen(m_pBuffer);
  processLine();
}

// not used normally
void JsonClient::end()
{
  m_ac.stop();
  m_Status = JC_IDLE;
}

int JsonClient::status()
{
  if(m_Status != JC_IDLE && m_bKeepAlive == false)
  {
    if(millis() - m_timer > m_to * 1000)
      end();
  }
  return m_Status;
}

void JsonClient::sendHeader(const char *pHeaderName, const char *pHeaderValue, AsyncClient* client) // string
{
  client->add(pHeaderName, strlen(pHeaderName));
  client->add(": ", 2);
  client->add(pHeaderValue, strlen(pHeaderValue) );
  client->add("\n", 1);
}

void JsonClient::sendHeader(const char *pHeaderName, int nHeaderValue, AsyncClient* client) // integer
{
  client->add(pHeaderName, strlen(pHeaderName) );
  client->add(": ", 2);
  String s = String(nHeaderValue);
  client->add(s.c_str(), s.length());
  client->add("\n", 1);
}

bool JsonClient::connect()
{
  if(m_szPath[0] == 0)
  {
    m_Status = JC_IDLE;
    return false;
  }
  if( m_retryCnt > RETRIES)
  {
    m_Status = JC_RETRY_FAIL;
    m_callback(-1, m_Status, m_nPort, m_pszHost);
    m_Status = JC_IDLE;
    return false;
  }

//  if(m_Status == JC_CONNECTING || m_Status == JC_CONNECTED)
//	  return false;

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
  m_callback(-1, m_Status, m_nPort, m_pszHost);
  m_Status = JC_IDLE;
  m_retryCnt++;
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
    m_callback(-1, m_Status, m_nPort, m_pszHost);
    m_Status = JC_IDLE;
    return;
  }
  connect();
}

void JsonClient::_onTimeout(AsyncClient* client, uint32_t time)
{
  (void)client;

  m_Status = JC_TIMEOUT;
  m_callback(-1, m_Status, m_nPort, m_pszHost);
  m_Status = JC_IDLE;
}

void JsonClient::_onConnect(AsyncClient* client)
{
  (void)client;

  if(m_bPost)
    client->add("POST ", 5);
  else
    client->add("GET ", 4);

  client->add(m_szPath, strlen(m_szPath));
  client->add(" HTTP/1.1\n", 10);

  if(m_pszHost && m_pszHost[0])
    sendHeader("Host", m_pszHost, client);
  else
    sendHeader("Host", m_ip.toString().c_str(), client);

  sendHeader("User-Agent", "Arduino", client);
  sendHeader("Connection", m_bKeepAlive ? "keep-alive" : "close", client);
  sendHeader("Accept", "*/*", client); // use application/json for strict
  if(m_pHeaders)
  {
    for(int i = 0; m_pHeaders[i] && m_pHeaders[i+1]; i += 2)
    {
      sendHeader(m_pHeaders[i], m_pHeaders[i+1], client);
    }
  }
  if(m_szData[0])
    sendHeader("Content-Length", strlen(m_szData), client);

  client->add("\n", 1);
  if(m_szData[0])
  {
    client->add(m_szData, strlen(m_szData));
    client->add("\n", 1);
  }

  m_brace = 0;
  m_Status = JC_CONNECTED;
  m_callback(-1, m_Status, m_nPort, m_pszHost);
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
      m_pBuffer[m_bufcnt++] = c;
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
  if(m_jsonCnt == 0)
    return;

  char *pPair[2]; // param:data pair

  char *p = m_pBuffer;

  while(*p)
  {
    p = skipwhite(p);
    if(*p == '{'){p++; m_brace++;}
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
    if(*p != ':') return;
    *p++ = 0;
    p = skipwhite(p);
    if(*p == '{'){p++; m_brace++; continue;} // data: {

    bInQ = false;
    if(*p == '"'){p++; bInQ = true;}
    pPair[1] = p;
    if(bInQ)
    {
       while(*p && *p!= '"') p++;
       if(*p == '"') *p++ = 0;
    }else
    {
      while(*p && *p != ',' && *p != '}' && *p != '\r' && *p != '\n') p++;
      if(*p == '}') m_brace--;
      *p++ = 0;
    }
    p = skipwhite(p);
    if(*p == '}'){*p++ = 0; m_brace--;}

    if(pPair[0][0])
    {
      if(!strcmp(pPair[0], "event") && m_jsonList[0][0]) // need event names
      {
        for(int i = 0; i < m_jsonCnt; i++)
          if(!strcmp(pPair[1], m_jsonList[i][0]))
            m_event = i;
      }
      else for(int i = 1; m_jsonList[m_event][i]; i++)
      {
        if(!strcmp(pPair[0], m_jsonList[m_event][i]))
        {
            int n = atol(pPair[1]);
            if(!strcmp(pPair[1], "true")) n = 1; // bool case
            m_callback(m_event, i-1, n, pPair[1]);
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
