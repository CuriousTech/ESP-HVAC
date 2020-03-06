/*
  JsonClient.h - Arduino library for reading JSON data streamed or single request.
  Copyright 2014 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.
*/
#ifndef JSONCLIENT_H
#define JSONCLIENT_H

#include <Arduino.h>
#include <ESPAsyncTCP.h>

enum JC_Status
{
    JC_IDLE,
	JC_BUSY,
    JC_CONNECTED,
    JC_DONE,
    JC_TIMEOUT,
    JC_NO_CONNECT,
    JC_RETRY_FAIL,
    JC_ERROR,
};

#define RETRIES 6

class JsonClient
{
public:
  JsonClient(void (*callback)(int16_t iEvent, uint16_t iName, int iValue, char *psValue), uint16_t nSize = 1024);
  bool  addList(const char **pList);
  bool  begin(const char *pHost, const char *pPath, uint16_t port, bool bKeepAlive, bool bPost = false, const char **pHeaders = NULL, char *pData = NULL, uint16_t to = 30000);
  void  end(void);
  void  process(char *event, char *data);
  int   status(void);

private:
  bool  connect(void);
  void  processLine(void);
  void  sendHeader(const char *pHeaderName, const char *pHeaderValue);
  void  sendHeader(const char *pHeaderName, int nHeaderValue);
  void  (*m_callback)(int16_t iEvent, uint16_t iName, int iValue, char *psValue);
  char *skipwhite(char *p);

  AsyncClient m_ac;
  void _onConnect(AsyncClient* client);
  void _onDisconnect(AsyncClient* client);
  static void _onError(AsyncClient* client, int8_t error);
  void _onTimeout(AsyncClient* client, uint32_t time);
  void _onData(AsyncClient* client, char* data, size_t len);
  char m_szHost[64];
  char m_szPath[128];
  char m_szData[256];
#define LIST_CNT 8
  const char **m_jsonList[LIST_CNT];
  const char **m_pHeaders;
  uint16_t m_bufcnt;
  uint16_t m_event;
  uint16_t m_nPort;
  uint16_t m_nBufSize;
  char     *m_pBuffer;
  unsigned long m_timeOut;
  int8_t m_brace;
  int8_t m_bracket;
  int8_t m_inBrace;
  int8_t m_inBracket;
  int8_t m_retryCnt;
  uint8_t m_jsonCnt;
  int8_t  m_Status;
  bool    m_bKeepAlive;
  bool    m_bPost;
};

#endif // JSONCLIENT_H
