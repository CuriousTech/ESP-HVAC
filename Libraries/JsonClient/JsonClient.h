/*
  JsonClient.h - Arduino library for reading JSON data streamed or single request.
  Copyright 2014 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.
*/
#ifndef JSONCLIENT_H
#define JSONCLIENT_H

#include <Arduino.h>
#include "WiFiClient.h"

enum JC_Status
{
    JC_IDLE,
    JC_CONNECTED,
    JC_DONE,
    JC_TIMEOUT,
    JC_NO_CONNECT,
};

#define JC_BUF_SIZE 1024

class JsonClient
{
public:
  JsonClient(void (*callback)(uint16_t iEvent, uint16_t iName, uint16_t iValue, char *psValue));
  bool  addList(const char **pList);
  bool  begin(const char *pHost, const char *pPath, uint16_t port, bool bKeepAlive, bool bPost = false, const char **pHeaders = NULL, char *pData = NULL);
  bool  service(void);
  void  end(void);

private:
  bool  connect(void);
  void  processLine(void);
  void  sendHeader(const char *pHeaderName, const char *pHeaderValue);
  void  sendHeader(const char *pHeaderName, int nHeaderValue);
  void  (*m_callback)(uint16_t iEvent, uint16_t iName, uint16_t iValue, char *psValue);
  char *skipwhite(char *p);

  WiFiClient m_client;
  char m_szHost[64];
  char m_szPath[64];
  char m_szData[256];
  const char **m_jsonList[8];
  const char **m_pHeaders;
  uint16_t m_bufcnt;
  uint16_t m_event;
  uint16_t m_nPort;
  char     m_buffer[JC_BUF_SIZE];
  unsigned long m_timeOut;
  uint8_t m_jsonCnt;
  int16_t m_brace;
  int8_t  m_Status;
  bool    m_bKeepAlive;
  bool    m_bPost;
};

#endif // JSONCLIENT_H

