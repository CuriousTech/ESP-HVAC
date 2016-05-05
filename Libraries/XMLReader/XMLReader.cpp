/*
  XMLReader.cpp - Arduino library for simple serialized multi-chunk reading of XML.
  Copyright 2014 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.
*/
#include "XMLReader.h"
 
#define TIMEOUT 30000 // Allow maximum 30s between data packets.

// Initialize with a buffer, it's length, and a callback to iterate values in a list tag (item = tag#, idx = index in list, p = next value string)
XMLReader::XMLReader(char *pBuffer, int16_t bufSize, void (*xml_callback)(int8_t item, int8_t idx, char *p) )
{
	m_pBuffer = pBuffer;
	m_pEnd = pBuffer + bufSize - 1;
	m_pPtr = pBuffer;
	m_pIn = pBuffer;
	m_xml_callback = xml_callback;
	m_Status = XML_IDLE;
}

void XMLReader::sendHeader(const char *pHeaderName, const char *pHeaderValue) // string
{
	m_client.print(pHeaderName);
	m_client.print(": ");
	m_client.println(pHeaderValue);
}

void XMLReader::sendHeader(const char *pHeaderName, int nHeaderValue) // integer
{
	m_client.print(pHeaderName);
	m_client.print(": ");
	m_client.println(nHeaderValue);
}

// begin with host and /path
bool XMLReader::begin(const char *pHost, char *pPath[])
{
  m_pPtr = m_pBuffer;
  m_pIn  = m_pBuffer;
    
  if(m_client.connected())
  {
    m_client.stop(); 
  }
    
  if( !m_client.connect(pHost, 80) )
  {
    m_Status = XML_NO_CONNECT;
    return false;
  }

	m_client.print("GET ");
  for(int8_t i = 0; pPath[i]; i++)
    m_client.print(pPath[i]);
	m_client.println(" HTTP/1.1");

	sendHeader("Host", pHost);
	sendHeader("Connection", "close");
	sendHeader("Accept", "*/*");

	m_client.println();
	m_client.flush();

	m_tagIdx = 0;
	m_tagState = 0;
	m_binValues = false;
  m_timeOut = millis();
  m_Status = XML_CONNECTED;
	return true;
}

// Serialized reader tags = list of XML_tag_t
bool XMLReader::service(XML_tag_t *tags)
{
  if(!tags[m_tagIdx].pszTag)  // competed, return false
  {
    m_Status = XML_DONE;
    m_client.stop();
    return false;
  }

  do
  {
    if(!fillBuffer())       // fill buffer with new bytes if ready
      return false;

    if(m_pIn == m_pEnd) // buffer full
    {
      if(m_binValues) // if not in values, increment to next tag
      {
        nextValue(tags);
      }
      else if(!tags[m_tagIdx].pszTag) // again, check for end
      {
        m_Status = XML_DONE;
        m_client.stop();
        return false;  // all done
      }
      else if( combTag(tags[m_tagIdx].pszTag, tags[m_tagIdx].pszAttr, tags[m_tagIdx].pszValue))   // scan for next tag
     	{
        m_binValues = true;
        m_valIdx = 0;
      }

      emptyBuffer();  // flush any data scanned
    }
  } while(m_client.available());  // continue while data is available

  if ( (millis() - m_timeOut) > TIMEOUT)
  {
    m_Status = XML_TIMEOUT;
    m_client.stop();
    return false;
	}

  return true;
}

// not used normally
void XMLReader::end()
{
  m_client.stop();
}

int8_t XMLReader::getStatus()
{
  return m_Status;
}

bool XMLReader::fillBuffer()
{
  if(!m_client.connected())       // latest code disconnects at end
  {
    m_pEnd = m_pIn; // truncate
    return (m_pPtr >= m_pEnd) ? false:true;     // return false when no more to read
  }

  char c = 0;
  int16_t cnt = 0;
  while(m_pIn < m_pEnd && m_client.available()) // max avail = 128
  {
    c = m_client.read();
    *m_pIn++ = c;
    m_timeOut = millis();
    cnt++;
  }
  *m_pIn = 0; // null terminate to make things easy
  return true;
}

void XMLReader::emptyBuffer()
{
  if(m_pPtr >= m_pEnd)	// all bytes are used.  Just reset
  {
    m_pPtr = m_pBuffer;
    m_pIn = m_pBuffer;
  }
  else if(m_pPtr > m_pBuffer)	// remove all used bytes
  {
    memcpy(m_pBuffer, m_pPtr, m_pEnd - m_pPtr); // shift remaining

    m_pIn -= (m_pPtr - m_pBuffer);	// shift in-ptr back same as remaining data
    m_pPtr = m_pBuffer;
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
      m_tagState = 1;
			if(*m_pPtr != '<')
				return false;
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
  {
    return true;	                	// Find start of tag
  }
  IncPtr();
  if(m_pPtr >= m_pEnd) return false;

  char *p = m_pPtr;

  while(*p++ != '<')                  	// lookahead
  {
    if(p >= m_pEnd)
    {
      return true;                    // not enough data to continue
    }
  }

  if(*m_pPtr == '/')                      // an end tag
  {
    IncPtr();
    if(tagCompare(m_pPtr, ptags[m_tagIdx].pszTag)) // end of value list
    {
      m_Status = XML_TRUNCATE;
      m_tagIdx++;
      m_binValues = false;
      return true;
    }
  }

  if(!tagEnd()) return true;			    // end of start tag
  IncPtr();
  if(m_pPtr >= m_pEnd) return false;

  char *ptr = m_pPtr;	// data

  if(!tagStart()) return true;
  if(m_pPtr >= m_pEnd) return false;

  *m_pPtr++ = 0;		                    // null term data (unsafe increment)
  if(m_pPtr >= m_pEnd) return false;

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

bool XMLReader::tagEnd()
{
  while(*m_pPtr != '>')                       // find end of tag
  {
    if(++m_pPtr >= m_pEnd)
      return false;
  }
  return true;
}
