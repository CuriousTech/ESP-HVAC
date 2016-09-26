/*
  XMLReader.cpp - Arduino library for simple serialized multi-chunk reading of XML.
  Copyright 2014 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.
*/
#include "XMLReader.h"
 
#define TIMEOUT 30000 // Allow maximum 30s between data packets.

// Initialize with a buffer, it's length, and a callback to iterate values in a list tag (item = tag#, idx = index in list, p = next value string)
XMLReader::XMLReader(void (*xml_callback)(int8_t item, int8_t idx, char *p), XML_tag_t *pTags )
{
  m_xml_callback = xml_callback;
  m_pTags = pTags;

  m_client.onConnect([](void* obj, AsyncClient* c) { (static_cast<XMLReader*>(obj))->_onConnect(c); }, this);
  m_client.onDisconnect([](void* obj, AsyncClient* c) { (static_cast<XMLReader*>(obj))->_onDisconnect(c); }, this);
  m_client.onTimeout([](void* obj, AsyncClient* c, uint32_t time) { (static_cast<XMLReader*>(obj))->_onTimeout(c, time); }, this);
  m_client.onData([](void* obj, AsyncClient* c, void* data, size_t len) { (static_cast<XMLReader*>(obj))->_onData(c, static_cast<char*>(data), len); }, this);

  m_client.setRxTimeout(TIMEOUT);
}

// begin with host and /path
bool XMLReader::begin(const char *pHost, String path)
{
  if(m_client.connected())
  {
    m_client.stop();
  }
 
  if( !m_client.connect(pHost, 80) )
  {
    return false;
  }

  m_pHost = pHost;
  m_path = path;
  m_tagIdx = 0;
  m_tagState = 0;
  m_binValues = false;
  m_pPtr = m_buffer;
  m_pIn = m_pPtr;
  m_pEnd = m_buffer + sizeof(m_buffer) - 2;
  return true;
}

void XMLReader::_onConnect(AsyncClient* client)
{
  (void)client;

  m_client.add("GET ", 4);
  m_client.add(m_path.c_str(), m_path.length());
  m_client.add(" HTTP/1.1", 9);
  m_client.add("\n", 1);

  sendHeader("Host", m_pHost);
  sendHeader("Connection", "close");
  sendHeader("Accept", "*/*");
  m_client.add("\n", 1);
}

void XMLReader::sendHeader(const char *pHeaderName, const char *pHeaderValue) // string
{
  m_client.add(pHeaderName, strlen(pHeaderName));
  m_client.add(": ", 2);
  m_client.add(pHeaderValue, strlen(pHeaderValue) );
  m_client.add("\n", 1);
}

void XMLReader::sendHeader(const char *pHeaderName, int nHeaderValue) // integer
{
  m_client.add(pHeaderName, strlen(pHeaderName) );
  m_client.add(": ", 2);
  String s = String(nHeaderValue);
  m_client.add(s.c_str(), s.length());
  m_client.add("\n", 1);
}

// Note: Chunks are up to about 1460 bytes
void XMLReader::_onData(AsyncClient* client, char* data, size_t len)
{
  (void)client;
  char *dataEnd = data + len;

  bool bDone = false;

  do{
    while(m_pIn < m_pEnd && data < dataEnd)
    {
      *m_pIn++ = *data++;
    }

    *m_pIn = 0; // null terminate to make things easy

    if(!m_pTags[m_tagIdx].pszTag)  // completed
    {
      m_xml_callback(-1, XML_COMPLETED, NULL);
      return;
    }

    if(m_binValues) // if not in values, increment to next tag
    {
      nextValue(m_pTags);
    }
    else if( combTag(m_pTags[m_tagIdx].pszTag, m_pTags[m_tagIdx].pszAttr, m_pTags[m_tagIdx].pszValue))   // scan for next tag
    {
      m_binValues = true;
      m_valIdx = 0;
    }
    emptyBuffer();

    if(data >= dataEnd && tagCnt() < 4 )  // this is just bad
    {
      bDone = true;
    }
  }while(!bDone);
}

void XMLReader::emptyBuffer()
{
  if(m_pPtr >= m_pEnd)	// all bytes are used.  Just reset
  {
    m_pPtr = m_buffer;
    m_pIn = m_buffer;
  }
  else if(m_pPtr > m_buffer)	// remove all used bytes
  {
    memcpy(m_buffer, m_pPtr, m_pEnd - m_pPtr); // shift remaining

    m_pIn -= (m_pPtr - m_buffer);	// shift in-ptr back same as remaining data
    m_pPtr = m_buffer;
  }
}

// Find a tag
bool XMLReader::combTag(const char *pTagName, const char *pAttr, const char *pValue)
{
  switch(m_tagState)
  {
    case 0:                   // not in tag
      if(!tagStart() )
        return false;         // find start of a tag
      if(*m_pPtr != '<')
        return false;
      m_tagState = 1;
      m_pPtr++;
      return false;
    case 1:		                            // found a tag
      if(tagCompare(m_pPtr, pTagName))
      {
        m_pTagName = pTagName;
        m_pPtr += strlen(pTagName);
        bool bFound = false;
        if(!pAttr)              	    // no attribute required
        {
          bFound = true;
          tagEnd();
          IncPtr();
          char *p = m_pPtr; // start of data in tag
          tagStart(); // end of data
          *m_pPtr++ = 0;
          m_xml_callback(m_tagIdx, m_valIdx, p);
          tagEnd(); // skip end tag
        }
        else while(*m_pPtr != '>' && m_pPtr < m_pEnd && !bFound) // find the correct attribute
        {
          if(tagCompare(m_pPtr++, pAttr))
          {
            m_pPtr += strlen(pAttr) + 1;
            if(pValue)
	        {
              bFound = tagCompare(m_pPtr, pValue);
            }
            else                    // no value required
            {
              bFound = true;
            }
          }
        }
        if(tagEnd())        		    // skip past this tag
          m_pPtr++;
        m_tagState = 0;
        return bFound;
      }
      else m_tagState = 2;
      break;
    case 2:						    	    // find possibly long end of tag
      if(!tagEnd())                       // retry on next pass with more data
        return false;
      m_tagState = 0;
      break;
  }
  return false;
}

// Get next tag data
bool XMLReader::nextValue(XML_tag_t *ptags)
{
  if(!tagStart())
    return true;	                	// Find start of tag
  IncPtr();
  if(m_pPtr >= m_pEnd)
   return false;

  char *p = m_pPtr;

  while(*p++ != '<')                  	// lookahead
  {
    if(p >= m_pEnd)
      return true;                    // not enough data to continue
  }

  if(*m_pPtr == '/')                      // an end tag
  {
    IncPtr();
    if(tagCompare(m_pPtr, ptags[m_tagIdx].pszTag)) // end of value list
    {
      m_tagIdx++;
      m_binValues = false;
      return true;
    }
  }

  if(!tagEnd())
    return true;    // end of start tag
  IncPtr();
  if(m_pPtr >= m_pEnd)
    return false;

  char *ptr = m_pPtr;	// data

  if(!tagStart())
    return true;

  if(m_pPtr >= m_pEnd)
    return false;

  *m_pPtr++ = 0;		                    // null term data (unsafe increment)
  if(m_pPtr >= m_pEnd)
   return false;

  m_xml_callback(m_tagIdx, m_valIdx, ptr);

  if(++m_valIdx >= ptags[m_tagIdx].valueCount)
  {
      m_binValues = false;
      m_tagIdx++;
  }
  tagEnd();		                    	// skip past end of end tag
  IncPtr();

  return true;
}

bool XMLReader::tagCompare(char *p1, const char *p2) // compare at lenngth of p2 with special chars
{
  while(*p2)
  {
    if(*p1 == 0) return false;
    if(*p1++ != *p2++) return false;
  }
  return (*p2 == 0 && (*p1 == ' ' || *p1 == '>' || *p1 == '=' || *p1 == '"') );
}

void XMLReader::IncPtr()
{
  if(++m_pPtr >= m_pEnd)                      // not entirely safe
    m_pPtr--;
}

bool XMLReader::tagStart()
{
  while(*m_pPtr != '<')                       // find start of tag
  {
    if(++m_pPtr >= m_pEnd)
      return false;
  }
  return true;
}

int XMLReader::tagCnt()
{
  char *p = m_pPtr;
  int cnt = 0;

  while(*p)                       // find start of tag
  {
    if(*p++ == '<') cnt++;
    if(p >= m_pEnd)
      return cnt;
  }
  return cnt;
}

bool XMLReader::tagEnd()
{
  while(*m_pPtr != '>')                       // find end of tag
  {
    if(++m_pPtr >= m_pEnd)
      return false;
  }
  return true;
}

void XMLReader::_onDisconnect(AsyncClient* client)
{
  (void)client;
  m_xml_callback(-1, XML_DONE, NULL);
}

void XMLReader::_onTimeout(AsyncClient* client, uint32_t time)
{
  (void)client;
  m_xml_callback(-1, XML_TIMEOUT, NULL);
}
