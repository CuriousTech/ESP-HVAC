#ifndef XMLREADER_H
#define XMLREADER_H

#include <Arduino.h>
#include <ESPAsyncTCP.h>

enum XML_Status
{
  XML_IDLE,
  XML_DONE,
  XML_COMPLETED,
  XML_TIMEOUT
};

struct XML_tag_t
{
  const char  *pszTag;
  const char  *pszAttr;
  const char  *pszValue;
  int8_t      valueCount;
};

class XMLReader
{
public:
  XMLReader(void (*xml_callback)(int8_t item, int8_t idx, char *p), XML_tag_t *pTags);
  bool  begin(const char *pHost, String path);

private:
  bool  combTag(const char *pTagName, const char *pAttr, const char *pValue);
  bool  nextValue(XML_tag_t *tags);
  bool  fillBuffer(char* data, size_t len);
  void  emptyBuffer(void);
  void  sendHeader(const char *pHeaderName, const char *pHeaderValue);
  void  sendHeader(const char *pHeaderName, int nHeaderValue);
  bool  tagCompare(char *p1, const char *p2);
  void  IncPtr(void);
  bool  tagStart(void);
  int   tagCnt(void);
  bool  tagEnd(void);

  void  (*m_xml_callback)(int8_t item, int8_t idx, char *p);

  AsyncClient m_client;

  void _onConnect(AsyncClient* client);
  void _onDisconnect(AsyncClient* client);
  static void _onError(AsyncClient* client, int8_t error);
  void _onTimeout(AsyncClient* client, uint32_t time);
  void _onData(AsyncClient* client, char* data, size_t len);

  const char  *m_pHost;
  char m_buffer[257];
  String m_path;
  XML_tag_t *m_pTags;
  char   *m_pPtr;
  char   *m_pEnd;
  char   *m_pIn;
  const char *m_pTagName;
  bool   m_binValues;
  int8_t m_tagIdx;
  int8_t m_tagState;
  int8_t m_valIdx;
};

#endif // XMLREADER_H
