#ifndef XMLREADER_H
#define XMLREADER_H

#include "HVAC.h"
#include "WiFiClient.h"

enum XML_Status
{
    XML_IDLE,
    XML_CONNECTED,
    XML_DONE,
    XML_TIMEOUT,
    XML_NO_CONNECT,
    XML_TRUNCATE
};

struct XML_tag_t
{
	const char  *pszTag;
	const char  *pszAttr;
	const char  *pszValue;
	int8_t		valueCount;
};

class XMLReader
{
public:
	XMLReader(char *pBuffer, int16_t bufSize, void (*xml_callback)(int8_t item, int8_t idx, char *p));
  bool   begin(const char *pHost, char *pPath[]);
  bool   service(XML_tag_t *tags);
	void	 end(void);
	bool	 combTag(const char *pTagName, const char *pAttr, const char *pValue);
  int8_t getStatus(void);
    
private:
	bool	nextValue(XML_tag_t *tags);
	bool	fillBuffer(void);
  void  emptyBuffer(void);
  void  sendHeader(const char *pHeaderName, const char *pHeaderValue);
  void  sendHeader(const char *pHeaderName, int nHeaderValue);
	bool  tagCompare(char *p1, const char *p2);
	void  IncPtr(void);
	bool  tagStart(void);
	bool  tagEnd(void);

  void  (*m_xml_callback)(int8_t item, int8_t idx, char *p);

  WiFiClient m_client;

	char	   *m_pBuffer;
	char	   *m_pPtr;
	char	   *m_pEnd;
	char      *m_pIn;
	const char *m_pTagName;
	bool	    m_binValues;
	int8_t	  m_tagIdx;
	int8_t	  m_tagState;
	int8_t  	m_valIdx;
	int8_t    m_Status;
  unsigned long m_timeOut;
};

#endif // XMLREADER_H
