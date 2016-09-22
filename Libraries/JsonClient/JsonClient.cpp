/*
  JsonClient.h - Arduino library for reading JSON data streamed or single request.
  Copyright 2014 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.

  1024 byte limit for data received
  8 lists max per instance
*/
#include "JsonClient.h"
 
#define TIMEOUT 30000 // Allow maximum 30s between data packets.

// Initialize instance with a callback (event list index, name index from 0, integer value, string value)
JsonClient::JsonClient(void (*callback)(int16_t iEvent, uint16_t iName, int iValue, char *psValue) )
{
  m_callback = callback;
  m_Status = JC_IDLE;
  m_jsonCnt = 0;
  m_bufcnt = 0;
  m_event = 0;
  m_bKeepAlive = false;
  m_szHost[0] = 0;
  m_szPath[0] = 0;
  m_ac.onConnect([](void* obj, AsyncClient* c) { (static_cast<JsonClient*>(obj))->_onConnect(c); }, this);
  m_ac.onDisconnect([](void* obj, AsyncClient* c) { (static_cast<JsonClient*>(obj))->_onDisconnect(c); }, this);
//  m_ac.onError([](void* obj, AsyncClient* c, int8_t error) { (static_cast<JsonClient*>(obj))->_onError(c, error); }, this);
  m_ac.onTimeout([](void* obj, AsyncClient* c, uint32_t time) { (static_cast<JsonClient*>(obj))->_onTimeout(c, time); }, this);
//  m_ac.onAck([](void* obj, AsyncClient* c, size_t len, uint32_t time) { (static_cast<JsonClient*>(obj))->_onAck(c, len, time); }, this);
  m_ac.onData([](void* obj, AsyncClient* c, void* data, size_t len) { (static_cast<JsonClient*>(obj))->_onData(c, static_cast<char*>(data), len); }, this);
//  m_ac.onPoll([](void* obj, AsyncClient* c) { (static_cast<JsonClient*>(obj))->_onPoll(c); }, this);

  m_ac.setRxTimeout(TIMEOUT);
}

// add a json list {"event name", "valname1", "valname2", "valname3", NULL}
// If first string is "" or NULL, the data is expected as JSON without an event name
// If second string is "" or NULL, the event name is expected, but the "data:" string is assumed non-JSON
bool JsonClient::addList(const char **pList)
{
  if(m_jsonCnt >= 8)
    return false;
  m_jsonList[m_jsonCnt++] = pList;
  return true;
}

// begin with host, /path?param=x&param=x, port, streaming
bool JsonClient::begin(const char *pHost, const char *pPath, uint16_t port, bool bKeepAlive, bool bPost, const char **pHeaders, char *pData)
{
  if(m_ac.connected())
    m_ac.stop();
  m_jsonCnt = 0;
  m_event = 0;
  m_bufcnt = 0;

  strncpy(m_szHost, pHost, sizeof(m_szHost) );
  strncpy(m_szPath, pPath, sizeof(m_szPath) );
  m_szData[0] = 0;
  if(pData)
    strncpy(m_szData, pData, sizeof(m_szData) );

  m_nPort = port;
  m_bKeepAlive = bKeepAlive;
  m_timeOut = millis();
  m_Status = JC_IDLE;
  m_pHeaders = pHeaders;
  m_bPost = bPost;
  m_retryCnt = 0;
  return connect();
}

// Call this from loop() ->remove
bool JsonClient::service()
{
  if(m_Status == JC_DONE || m_Status == JC_RETRY_FAIL)
    return false;
  return true;
}

// not used normally
void JsonClient::end()
{
  m_ac.stop();
  m_szHost[0] = 0;
  m_Status = JC_IDLE;
}

int JsonClient::status()
{
  return m_Status;
}

void JsonClient::sendHeader(const char *pHeaderName, const char *pHeaderValue) // string
{
  m_ac.add(pHeaderName, strlen(pHeaderName));
  m_ac.add(": ", 2);
  m_ac.add(pHeaderValue, strlen(pHeaderValue) );
  m_ac.add("\n", 1);
}

void JsonClient::sendHeader(const char *pHeaderName, int nHeaderValue) // integer
{
  m_ac.add(pHeaderName, strlen(pHeaderName) );
  m_ac.add(": ", 2);
  String s = String(nHeaderValue);
  m_ac.add(s.c_str(), s.length());
  m_ac.add("\n", 1);
}

bool JsonClient::connect()
{
  if(m_szHost[0] == 0 || m_szPath[0] == 0)
    return false;

  if( m_retryCnt > RETRIES)
  {
    m_Status = JC_RETRY_FAIL;
    m_szHost[0] = 0;
    m_callback(-1, m_Status, m_nPort, m_szHost);
    return false;
  }

  if(m_ac.connected())
    return true;

  if( !m_ac.connect(m_szHost, m_nPort) )
  {
    m_Status = JC_NO_CONNECT;
    m_callback(-1, m_Status, m_nPort, m_szHost);
    m_retryCnt++;
    return false;
  }
  return true;
}

void JsonClient::_onDisconnect(AsyncClient* client)
{
  (void)client;

  if(m_bKeepAlive == false)
  {
    m_Status = JC_DONE;
    if(m_bufcnt) // no LF at end?
    {
        m_buffer[m_bufcnt] = '\0';
        processLine();
    }
    return;
  }
  connect();
}

void JsonClient::_onTimeout(AsyncClient* client, uint32_t time)
{
  (void)client;

  m_Status = JC_TIMEOUT;
  m_ac.stop();
  m_callback(-1, m_Status, m_nPort, m_szHost);
}

void JsonClient::_onConnect(AsyncClient* client)
{
  (void)client;

  if(m_bPost)
    m_ac.add("POST ", 5);
  else
    m_ac.add("GET ", 4);

  m_ac.add(m_szPath, strlen(m_szPath));
  m_ac.add(" HTTP/1.1\n", 10);

  sendHeader("Host", m_szHost);
  sendHeader("User-Agent", "Arduino");
  sendHeader("Connection", m_bKeepAlive ? "keep-alive" : "close");
  sendHeader("Accept", "*/*"); // use application/json for strict
  if(m_pHeaders)
  {
    for(int i = 0; m_pHeaders[i] && m_pHeaders[i+1]; i += 2)
    {
      sendHeader(m_pHeaders[i], m_pHeaders[i+1]);
    }
  }
  if(m_szData[0])
    sendHeader("Content-Length", strlen(m_szData));

  m_ac.add("\n", 1);
  if(m_szData[0])
  {
    m_ac.add(m_szData, strlen(m_szData));
	m_ac.add("\n", 1);
  }

  m_Status = JC_CONNECTED;
  m_brace = 0;
  m_callback(-1, m_Status, m_nPort, m_szHost);
}

void JsonClient::_onData(AsyncClient* client, char* data, size_t len)
{
  (void)client;

  for(int i = 0; i < len; i++)
  {
    char c = data[i];
    if(c != '\r')
      m_buffer[m_bufcnt++] = c;
    if(c == '\n')
    {
      if(m_bufcnt > 1) // ignore keepalive
      {
        m_buffer[m_bufcnt-1] = '\0';
        processLine();
      }
      m_bufcnt = 0;
    }
  }
  m_timeOut = millis();
}

void JsonClient::processLine()
{
  if(m_jsonCnt == 0)
    return;

  char *pPair[2]; // param:data pair

  char *p = m_buffer;

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
      while(*p && *p != ',') p++;
      *p++ = 0;
    }
    p = skipwhite(p);
    if(*p == '}'){p++; m_brace--;}

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
            m_callback(m_event, i-1, atoi(pPair[1]), pPair[1]);
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
