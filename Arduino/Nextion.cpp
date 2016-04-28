#include "Nextion.h"

// get changes
int Nextion::service(char *pBuf)
{
  dimmer();
  if(!Serial.available())
    return 0;
  int len = Serial.readBytesUntil(0xFF, pBuf, 63);

  if(len < 3) // could be the other 2 FFs
    return 0;
  pBuf[len] = 0; // change FF to NULL
  return len;
}

void Nextion::itemText(uint8_t id, String t)
{
  String s = "t";
  s += id;
  s += ".txt=\"";
  s += t;
  s += "\"";
  Serial.print(s);
  FFF();
}

void Nextion::btnText(uint8_t id, String t)
{
  String s = "b";
  s += id;
  s += ".txt=\"";
  s += t;
  s += "\"";
  Serial.print(s);
  FFF();
}

void Nextion::itemFp(uint8_t id, uint16_t val)
{
  String s = "f";
  s += id;
  s += ".txt=\"";
  s += sDec(val);
  s += "\"";
  Serial.print(s);
  FFF();
}

void Nextion::itemNum(uint8_t item, int16_t num)
{
  String s = "n";
  s += item;
  s += ".val=";
  s += num;
  Serial.print(s);
  FFF();
}

void Nextion::refreshItem(String id)
{
  Serial.print("ref ");
  Serial.print(id);
  FFF();
}

void Nextion::text(uint16_t x, uint16_t y, uint16_t w,  uint16_t xCenter, String s)
{
  const uint8_t fontId = 1;
  const uint16_t fontColor = 0xFFE0; // yellow
  const uint16_t bkColor = 0; // black
  const uint8_t yCenter = 1; // xCenter 0=left, 1=center, 2=right > yCenter top, center, bottom
  const uint8_t sta = 0; // 0=crop, 1=bkColor
  const uint8_t h = 16; // 16 for small font
  
  Serial.print("xstr ");
  Serial.print(x);
  Serial.print(",");
  Serial.print(y);
  Serial.print(",");
  Serial.print(w);
  Serial.print(",");
  Serial.print(h);
  Serial.print(",");
  Serial.print(fontId);
  Serial.print(",");
  Serial.print(fontColor);
  Serial.print(",");
  Serial.print(bkColor);
  Serial.print(",");
  Serial.print(xCenter);
  Serial.print(",");
  Serial.print(yCenter);
  Serial.print(",");
  Serial.print(sta);
  Serial.print(",\"");
  Serial.print(s);
  Serial.print("\"");
  FFF();
}

void Nextion::fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  Serial.print("fill ");
  Serial.print(x);
  Serial.print(",");
  Serial.print(y);
  Serial.print(",");
  Serial.print(w);
  Serial.print(",");
  Serial.print(h);
  Serial.print(",");
  Serial.print(color);
  FFF();
}

void Nextion::line(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2, uint16_t color)
{
  Serial.print("line ");
  Serial.print(x);
  Serial.print(",");
  Serial.print(y);
  Serial.print(",");
  Serial.print(x2);
  Serial.print(",");
  Serial.print(y2);
  Serial.print(",");
  Serial.print(color);
  FFF();
}

void Nextion::visible(String id, uint8_t on)
{
  Serial.print("vis ");
  Serial.print(id);
  Serial.print(",");
  Serial.print(on);
  FFF();
}

void Nextion::itemPic(uint8_t id, uint8_t idx)
{
  String s = "p";
  s += id;
  s += ".pic=";
  s += idx;
  Serial.print(s);
  FFF();
}

void Nextion::brightness(uint8_t level)
{
  m_newBrightness = level;
}

void Nextion::setPage(String sPage)
{
  String s = "page ";
  s += sPage;
  Serial.print(s);
  FFF();
  switch(sPage.charAt(0))
  {
    case 'T': m_page = 0; break;  // Theromosat
    case 'c': m_page = 1; break;  // clock (analog)
    case 'S': m_page = 2; break;  // SSID list
    case 'k': m_page = 3; break;  // keyboard
    case 'g': m_page = 4; break;  // graph
    case 'b': m_page = 5; break;  // blank (just color)
  }
}

uint8_t Nextion::getPage()
{
  return m_page;
}

void Nextion::gauge(uint8_t id, uint16_t angle)
{
  String s = "z";
  s += id;
  s += ".val=";
  s += angle;
  Serial.print(s);
  FFF();
}

void Nextion::backColor(String sPageName, uint16_t color)
{
  String s = sPageName;
  s += ".bco=";
  s += color;
  Serial.print(s);
  FFF();
}

void Nextion::cls(uint16_t color)
{
  String s = "cls ";
  s += color;
  Serial.print(s);
  FFF();
}

void Nextion::add(uint8_t comp, uint8_t ch, uint16_t val)
{
  String s = "add ";
  s += comp;
  s += ",";
  s += ch;
  s += ",";
  s += val;
  Serial.print(s);
  FFF();
}

String Nextion::sDec(int t) // just 123 to 12.3 string
{
  String s = String( t / 10 ) + ".";
  s += t % 10;
  return s;
}

void Nextion::FFF()
{
  Serial.write(0xFF);
  Serial.write(0xFF);
  Serial.write(0xFF);
}

void Nextion::dimmer()
{
  if(m_newBrightness == m_brightness)
    return;
  if(m_newBrightness > m_brightness + 1)
    m_brightness += 2;
  else if(m_newBrightness < m_brightness - 1)
    m_brightness -= 2;
  else
    m_brightness = m_newBrightness;

  Serial.print("dim=");
  Serial.print(m_brightness);
  FFF();
}
