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
  Serial.print(String("t") + id + ".txt=\"" + t + "\"");
  FFF();
}

void Nextion::btnText(uint8_t id, String t)
{
  Serial.print(String("b") + id + ".txt=\"" + t + "\"");
  FFF();
}

void Nextion::itemFp(uint8_t id, uint16_t val) // 123 to 12.3
{
  Serial.print(String("f") + id + ".txt=\"" + (val / 10) + "." + (val % 10) + "\"" );
  FFF();
}

void Nextion::itemNum(uint8_t item, int16_t num)
{
  Serial.print(String("n") + item + ".val=" + num);
  FFF();
}

void Nextion::refreshItem(String id)
{
  Serial.print(String("ref ") + id);
  FFF();
}

void Nextion::text(uint16_t x, uint16_t y, uint16_t xCenter, uint16_t color, String sText)
{
  const uint16_t bkColor = m_page; // transparent source
  const uint8_t h = 16; // 8x16 for small font + space
  uint16_t w = sText.length() * 9;

  Serial.print(String("xstr ") + x + "," + y + "," + w + ",16,1," + color + "," + bkColor +
        "," + xCenter + ",1,0,\"" + sText + "\"");
  FFF();
}

void Nextion::fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
  Serial.print(String("fill ") + x + "," + y + "," + w + "," + h + "," + color);
  FFF();
}

void Nextion::line(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2, uint16_t color)
{
  Serial.print(String("line ") + x + "," + y + "," + x2 + "," + y2 + "," + color);
  FFF();
}

void Nextion::visible(String id, uint8_t on)
{
  Serial.print(String("vis ") + id + "," + on);
  FFF();
}

void Nextion::itemPic(uint8_t id, uint8_t idx)
{
  Serial.print(String("p") + id + ".pic=" + idx);
  FFF();
}

void Nextion::brightness(uint8_t level)
{
  m_newBrightness = level;
}

void Nextion::setPage(String sPage)
{
  Serial.print(String("page ") + sPage);
  FFF();
  switch(sPage.charAt(0))
  {
    case 'T': m_page = Page_Thermostat; break;  // Theromosat
    case 'c': m_page = Page_Clock; break;  // clock (analog)
    case 'S': m_page = Page_SSID; break;  // SSID list
    case 'k': m_page = Page_Keyboard; break;  // keyboard
    case 'g': m_page = Page_Graph; break;  // graph
    case 'b': m_page = Page_Blank; break;  // blank (just color)
  }
}

uint8_t Nextion::getPage()
{
  return m_page;
}

void Nextion::gauge(uint8_t id, uint16_t angle)
{
  Serial.print(String("z") + id + ".val=" + angle);
  FFF();
}

void Nextion::backColor(String sPageName, uint16_t color)
{
  Serial.print(sPageName + ".bco=" + color);
  FFF();
}

void Nextion::cls(uint16_t color)
{
  Serial.print(String("cls ") + color);
  FFF();
}

void Nextion::add(uint8_t comp, uint8_t ch, uint16_t val)
{
  Serial.print(String("add ") + comp + "," + ch + "," + val);
  FFF();
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

  Serial.print(String("dim=") + m_brightness);
  FFF();
}
