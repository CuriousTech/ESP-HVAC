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
JsonClient::JsonClient(void (*callback)(uint16_t iEvent, uint16_t iName, uint16_t iValue, char *psValue) )
{
  m_callback = callback;
  m_Status = JC_IDLE;
  m_jsonCnt = 0;
  m_bufcnt = 0;
  m_event = 0;
  m_bKeepAlive = false;
  m_szHost[0] = 0;
  m_szPath[0] = 0;
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
bool JsonClient::begin(const char *pHost, const char *pPath, uint16_t port, bool bKeepAlive)
{
  m_jsonCnt = 0;
  m_event = 0;
  m_bufcnt = 0;
  strncpy(m_szHost, pHost, sizeof(m_szHost) );
  strncpy(m_szPath, pPath, sizeof(m_szPath) );
  m_nPort = port;
  m_bKeepAlive = bKeepAlive;
  m_timeOut = millis() - 1000;
  m_Status = JC_IDLE;
  return connect();
}

// Call this from loop()
bool JsonClient::service()
{
  char c;
  if(m_Status == JC_DONE)
    return false;
  if(!m_client.connected())
  {
    if(m_bKeepAlive == false)
    {
      m_Status = JC_DONE;
      if(m_bufcnt) // no LF at end?
      {
          m_buffer[m_bufcnt] = '\0';
          processLine();
      }
      return false;
    }
    return connect();
  }

  while(m_bufcnt < JC_BUF_SIZE && m_client.available())
  {
    char c = m_client.read();
    if(c != '\r')
      m_buffer[m_bufcnt++] = c;
    m_timeOut = millis();
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

  if ( (millis() - m_timeOut) > TIMEOUT)
  {
    m_Status = JC_TIMEOUT;
    m_client.stop();
    return false;
	}

  return true;
}

// not used normally
void JsonClient::end()
{
  m_client.stop();
  m_szHost[0] = 0;
  m_Status = JC_IDLE;
}

void JsonClient::sendHeader(const char *pHeaderName, const char *pHeaderValue) // string
{
  m_client.print(pHeaderName);
  m_client.print(": ");
  m_client.println(pHeaderValue);
}

void JsonClient::sendHeader(const char *pHeaderName, int nHeaderValue) // integer
{
  m_client.print(pHeaderName);
  m_client.print(": ");
  m_client.println(nHeaderValue);
}

bool JsonClient::connect()
{
  if(m_szHost[0] == 0 || m_szPath[0] == 0)
    return false;

  if(m_client.connected())
    return false;

  if((millis() - m_timeOut) < 1000) // 1 second between retries
    return false;

  m_timeOut = millis();

  if( !m_client.connect(m_szHost, m_nPort) )
  {
    m_Status = JC_NO_CONNECT;
    Serial.println("Connection failed");
    Serial.print(m_szHost);
    Serial.print(" ");
    Serial.print(m_szPath);
    Serial.print(" ");
    Serial.println(m_nPort);
    return false;
  }

  m_client.print("GET ");
  m_client.print(m_szPath);
  m_client.println(" HTTP/1.1");

  sendHeader("Host", m_szHost);
  sendHeader("Connection", m_bKeepAlive ? "keep-alive" : "close");
  sendHeader("Accept", "*/*"); // use text/json for strict

  m_client.println();
  m_client.flush();

  m_Status = JC_CONNECTED;
  return true;
}

void JsonClient::processLine()
{
  if(!strcmp(m_buffer, ":ok")) // started
    return;

  if(m_jsonCnt == 0)
    return;

  char *p = strchr(m_buffer, ':');
  char *p2 = strchr(m_buffer, '{');
  if(p == NULL || p2 == NULL)
    return;

  if(p > p2) // no event name
  {
    p = m_buffer;
  }
  else
  {
    *p = 0; // term the :
    p++;
  }

  p = skipwhite(p);

  if(!strcmp(m_buffer, "event") && m_jsonList[0][0]) // need event names
  {
    for(int i = 0; i < m_jsonCnt; i++)
      if(!strcmp(p, m_jsonList[i][0]))
        m_event = i;
  }
  else if(!strcmp(m_buffer, "data") || m_jsonList[0][0] == NULL || m_jsonList[0][0][0] == 0)
  {
    p = skipwhite(p); // skip leading spaces
    if(*p != '{')
    {
      m_callback(m_event, 0, atoi(p), p);
      return;
    }
    p = strtok(p+1, ",}");
    if(p == NULL)
    {
      Serial.println("No } found");
      return;
    }
    do // for each pair
    {
      bool bInQ = false;
      p = skipwhite(p); // skip leading spaces
      if(*p == '"') // name in quotes?
      {
        p++;
        bInQ = true;
      }
      char *pName = p;
      p++;
      if(bInQ) while(*p && *p != '"' && *p != ':') p++; // find end quote (if missing, it'll find :)
      else while(*p && *p != ':') p++; // or : if no quotes
      *p++ = 0; // null term at end of name
      if(*p == ':') p++;
      p = skipwhite(p); // skip any spaces
      bInQ = false;
      if(*p == '"'){ // value in quotes?
        p++;
        bInQ = true;
      }
      int16_t value = atoi(p); // possibly intenger
      char *pValue = p;       // string, float etc.
      p += strlen(p)-1;

      if(*p == '"')
        *p = 0;

      for(int i = 1; m_jsonList[m_event][i]; i++)
      {
        if(!strcmp(pName, m_jsonList[m_event][i]))
        {
            m_callback(m_event, i-1, value, pValue);
        }
      }
    }while( p = strtok(NULL, ",}") );
  }
  else
  {
//    Serial.print("JsonClient: invalid label ");
//    Serial.println(m_buffer);
  }
}

char * JsonClient::skipwhite(char *p)
{
  while(*p == ' ' || *p == '\t' || *p =='\r' || *p == '\n')
    p++;
  return p;
}
