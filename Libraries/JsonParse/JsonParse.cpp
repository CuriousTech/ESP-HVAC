/*
  JsonParse.h - Arduino library for parsing JSON data.  Note: Data string is written to for NULL termination.
  Copyright 2016 Greg Cunningham, CuriousTech.net
  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.
  8 lists max per instance
*/
#include "JsonParse.h"
 
// Initialize instance with a callback (event list index, name index from 0, integer value, string value)
JsonParse::JsonParse( void (*callback)(int16_t iName, int iValue, char *psValue) )
{
  m_callback = callback;
}

// add a json list { "valname1", "valname2", "valname3", NULL}
void JsonParse::setList( const char **pList )
{
  m_jsonList = pList;
}

void JsonParse::process( char *data )
{
  if(m_jsonList == NULL)
    return;

  char *pPair[2]; // param:data pair
  int8_t brace = 0;
  int8_t bracket = 0;
  int8_t inBracket = 0;
  int8_t inBrace = 0;

  char *p = data;
  while(*p && *p != '{') // skip old label
    p++;

  while(*p)
  {
    p = skipwhite(p);
    if(*p == '{'){p++; brace++;}
    if(*p == '['){p++; bracket++;}
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
    if(*p != ':')
      return;

    *p++ = 0;
    p = skipwhite(p);
    bInQ = false;
    if(*p == '{') inBrace = brace+1; // data: {
    else if(*p == '['){p++; inBracket = bracket+1;} // data: [
    else if(*p == '"'){p++; bInQ = true;}
    pPair[1] = p;
    if(bInQ)
    {
       while(*p && *p!= '"') p++;
       if(*p == '"') *p++ = 0;
    }else if(inBrace)
    {
      while(*p && inBrace != brace){
        p++;
        if(*p == '{') inBrace++;
        if(*p == '}') inBrace--;
      }
      if(*p=='}') p++;
    }else if(inBracket)
    {
      while(*p && inBracket != bracket){
        p++;
        if(*p == '[') inBracket++;
        if(*p == ']') inBracket--;
      }
      if(*p == ']') *p++ = 0;
    }else while(*p && *p != ',' && *p != '\r' && *p != '\n' && *p != '}') p++;
    if(*p) *p++ = 0;
    p = skipwhite(p);
    if(*p == ',') *p++ = 0;

    inBracket = 0;
    inBrace = 0;
    p = skipwhite(p);

    if(pPair[0][0])
    {
      for(int i = 0; m_jsonList[i]; i++)
      {
        if( !strcmp(pPair[0], m_jsonList[i]) )
        {
            int32_t n = atoi(pPair[1]);
            if(!strcmp(pPair[1], "true")) n = 1; // bool case
            m_callback( i, n, pPair[1]);
            break;
        }
      }
    }
  }
  m_callback( -1, 0, (char*)""); // end
}

char * JsonParse::skipwhite(char *p)
{
  while(*p == ' ' || *p == '\t' || *p =='\r' || *p == '\n')
    p++;
  return p;
}
