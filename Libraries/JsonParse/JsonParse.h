/*
  JsonParse.h - Arduino library for reading JSON data.
  Copyright 2016 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.
*/
#ifndef JSONPARSE_H
#define JSONPARSE_H

#include <Arduino.h>

class JsonParse
{
public:
  JsonParse(void (*callback)(int16_t iEvent, uint16_t iName, int iValue, char *psValue));
  bool  addList(const char **pList);
  void  process(char *event, char *data);

private:
  char *skipwhite(char *p);
  void  (*m_callback)(int16_t iEvent, uint16_t iName, int iValue, char *psValue);

#define LIST_CNT
  const char **m_jsonList[8];
  uint8_t m_jsonCnt;
};

#endif // JSONPARSE_H
