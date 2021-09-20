/*
  JsonParse.h - Arduino library for parsing JSON data.  Note: Data string is written to for NULL termination.
  Copyright 2016 Greg Cunningham, CuriousTech.net
  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.
  8 lists max per instance
*/
#include "JsonParse.h"
 
// Initialize instance with a callback (event list index, name index from 0, integer value, string value)
JsonParse::JsonParse(void (*callback)(int16_t iEvent, uint16_t iName, int iValue, char *psValue) )
{
  m_callback = callback;
  m_jsonCnt = 0;
}

// add a json list {"event name", "valname1", "valname2", "valname3", NULL}
bool JsonParse::addList(const char **pList)
{
  if(m_jsonCnt >= LIST_CNT)
    return false;
  m_jsonList[m_jsonCnt++] = pList;
  return true;
}

void JsonParse::process(char *event, char *data)
{
  if(m_jsonCnt == 0)
    return;

  uint16_t _event = 0;
  for(int i = 0; i < m_jsonCnt; i++)
    if(!strcmp(event, m_jsonList[i][0]))
      _event = i;

  char *pPair[2]; // param:data pair

  char *p = data;
  int16_t brace = 0;

  while(*p)
  {
    p = skipwhite(p);
    if(*p == '{'){p++; brace++;}
    if(*p == ',') p++;
    p = skipwhite(p);

    bool bInQ = false;
    if(*p == '"'){p++; bInQ = true;}
    pPair[0] = p;
    if(bInQ)
    {
       while(*p && *p!= '"') p++;
       if(*p == '"') *p++ = 0;
    }
    while(*p && *p != ':') p++;
    if(*p != ':') return;
    *p++ = 0;
    p = skipwhite(p);
    if(*p == '{'){p++; brace++; continue;} // data: {

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
      if(*p == '}') brace--;
      *p++ = 0;
    }
    p = skipwhite(p);
    if(*p == '}'){*p++ = 0; brace--;}

    if(pPair[0][0])
    {
      for(int i = 1; m_jsonList[_event][i]; i++)
      {
        if(!strcmp(pPair[0], m_jsonList[_event][i]))
        {
            int n = atoi(pPair[1]);
            if(!strcmp(pPair[1], "true")) n = 1; // bool case
            m_callback(_event, i-1, n, pPair[1]);
            break;
        }
      }
    }
  }
}

char * JsonParse::skipwhite(char *p)
{
  while(*p == ' ' || *p == '\t' || *p =='\r' || *p == '\n')
    p++;
  return p;
}
