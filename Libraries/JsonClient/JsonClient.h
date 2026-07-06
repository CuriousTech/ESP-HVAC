/*
  JsonClient.h - Arduino library for reading JSON data streamed or single request.
  Copyright 2014 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.
*/
#ifndef JSONCLIENT_H
#define JSONCLIENT_H

#include <Arduino.h>
#ifdef ESP32
#include <AsyncTCP.h>
#else
#include <ESPAsyncTCP.h>
#endif

enum JC_Status
{
    JC_IDLE,
    JC_CONNECTING,
    JC_CONNECTED,
    JC_DONE,
    JC_TIMEOUT,
    JC_NO_CONNECT,
    JC_ERROR,
};

#define RETRIES 6
#define TIMEOUT 30 // Allow maximum 30s between data packets.
#define CQ_CNT 32 

struct cQ
{
  IPAddress ip;
  const char *pszHost;
  String sUri;
  String sJSON;
  uint16_t port;
  uint8_t status;
  uint8_t retries;
};

class JsonClient
{
public:
  JsonClient(void (*callback)(int16_t iName, int iValue, char *psValue, IPAddress ip), uint16_t nSize = 1024);
  bool addQueue(IPAddress ip, String sUri, uint16_t port, String sJSON);
  bool addQueue(const char *pszHost, String sUri, uint16_t port, String sJSON);
  void  service(void);
  bool  setList(const char **pList);
  bool  begin(const char *pHost, const char *pPath, uint16_t port, uint8_t idx, bool bKeepAlive = false, bool bPost = false, const char **pHeaders = NULL, const char *pData = NULL);
  bool  begin(IPAddress ip, const char *pPath, uint16_t port, uint8_t idx, bool bKeepAlive = false, bool bPost = false, const char **pHeaders = NULL, const char *pData = NULL);
  void  end(void);
  void  process(char *data);
  int   status(void);

private:
  bool  begin(const char *pPath, uint16_t port, bool bKeepAlive = false, bool bPost = false, const char **pHeaders = NULL, const char *pData = NULL);
  bool  connect(void);
  void  processLine(void);
  String mkHeader(const char *pHeaderName, const char *pHeaderValue);
  String mkHeader(const char *pHeaderName, int nHeaderValue);
  void  (*m_callback)(int16_t iName, int iValue, char *psValue, IPAddress ip);
  char *skipwhite(char *p);

  AsyncClient m_ac;
  void _onConnect(AsyncClient* client);
  void _onDisconnect(AsyncClient* client);
  void _onError(AsyncClient* client, int8_t error);
  void _onTimeout(AsyncClient* client, uint32_t time);
  void _onData(AsyncClient* client, char* data, size_t len);

  cQ queue[CQ_CNT];
  uint32_t m_timer;
  uint8_t m_idx;
  IPAddress m_ip;
  char *m_pszHost;
  char m_szPath[128];
  char m_szData[256];
#define LIST_CNT 8
  const char **m_jsonList;
  const char **m_pHeaders;
  uint16_t m_bufcnt;
  uint16_t m_nPort;
  char     *m_pBuffer;
  uint16_t m_nBufSize;
  uint8_t m_acIdx;
  int8_t m_brace;
  int8_t m_bracket;
  int8_t m_inBrace;
  int8_t m_inBracket;
  int8_t  m_Status;
  bool    m_bKeepAlive;
  bool    m_bPost;
};

#endif // JSONCLIENT_H
