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
  JsonParse(void (*callback)(int16_t iName, int iValue, char *psValue));
  void  setList(const char **pList);
  void  process(char *data);

private:
  char *skipwhite(char *p);
  void  (*m_callback)(int16_t iName, int iValue, char *psValue);

  const char **m_jsonList;
};

#endif // JSONPARSE_H
