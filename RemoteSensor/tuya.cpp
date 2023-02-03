// Class for Tuya temp and humidity

#include "tuya.h"
#include <TimeLib.h>
#include <UdpTime.h>
#include "defs.h"

//#define BWAR01 // BlitzWolf BW-AR01

//#define DEBUG
#define BAUD 9600

#ifdef BWAR01 // BlitzWolf BW-AR01
#define WIFI_LED 13  // high = on
#define BTN      14   // button (reset button)
#else // Tuya WIFI Temperature Humidity Smart Sensor Clock Digital Display - Type A (must replace with ESP8266 and add resistors to IO15 and REST, cut trace to REST)
#define ESP_LED   2  // low = on
#define WIFI_LED  4  // high = on
#define BTN      13  // C/F button (connected to MCU as well)
#endif

extern UdpTime utime;

extern void WsSend(String s);

void TuyaInterface::init(bool bCF)
{
#ifdef BWAR01
  m_dataFlags = 0x1F; // enable all sensors
#else
  m_dataFlags = (DF_TEMP | DF_RH);
#endif
  pinMode(WIFI_LED, OUTPUT);
  digitalWrite(WIFI_LED, LOW);
#ifdef ESP_LED
  pinMode(ESP_LED, OUTPUT);
  digitalWrite(ESP_LED, HIGH);
#endif
  pinMode(BTN, INPUT_PULLUP);
  Serial.begin(BAUD);
  m_bCF = bCF;
}

int TuyaInterface::service(int8_t tcal, int8_t rhcal)
{
  static uint8_t inBuffer[52];
  static uint8_t idx;
  static uint8_t v;
  static uint8_t state;
  static uint8_t cmd;
  static bool bInit = false;
  static uint16_t len;
  uint8_t n;
  uint8_t buf[2];
  uint32_t val;

  while(Serial.available())
  {
    uint8_t c = Serial.read();
    switch(state)
    {
      case 0:     // data packet: 55 AA vers cmd 00 len d0 d1 d2.... chk
        if(c == 0x55)
          state = 1;
        else if(c == 0xAA)
          state = 2;
        break;
      case 1:
        if(c == 0xAA)
          state = 2;
        break;
      case 2:
        // version 3
        v = c;
        state = 3;
        break;
      case 3:
        cmd = c;
        state = 4;
        break;
      case 4:
        len = (uint16_t)c<<8;
        state = 5;
        break;
      case 5:
        len |= (uint16_t)c;
        state = 6;
        idx = 0;
        break;
      case 6:
        inBuffer[idx++] = c; // get length + checksum
        if(idx > len || idx >= sizeof(inBuffer) )
        {
          uint8_t chk = 0xFF + len + v + cmd;
          for(int a = 0; a < len; a++)
            chk += inBuffer[a];
#ifdef DEBUG
          String s = "{\"cmd\":\"print\",\"text\":\"RX ";
          s += len;
          s += " ";
          for(int a = 0; a < len; a++)
          {
            s += " ";
            s += String(inBuffer[a], 16);
          }
          s += "\"}";
          WsSend(s);
#endif
          if( inBuffer[len] == chk) // good checksum
          {
            switch(cmd)
            {
              case TC_HEARTBEAT: // heartbeat 01 = MCU reset
                writeSerial(TC_MCU_CONF);
                break;
              case TC_QUERY_PRODUCT: // product ID (42 bytes)
                break;
              case TC_MCU_CONF: // ack for MCU conf 09 06 or 0D 0E
                m_mcuConf[0] = inBuffer[0];
                m_mcuConf[1] = inBuffer[1];
#ifdef BWAR01 // Causes AR-01 to start/reset (first temp/RH values are 0)
                if(bInit == false)
                {
                  bInit = true;
                  writeSerial(TC_QUERY_STATE);
                }
#else // Causes Tuya clock to continue running
                writeSerial(TC_QUERY_STATE);
#endif
                break;
              case TC_STATE: // data
                val = (inBuffer[4] << 24) | (inBuffer[5] << 16) | (inBuffer[6] << 8) | inBuffer[7]; // big endien long
                switch(inBuffer[0])
                {
#ifdef BWAR01
                  case 2: // 02 02 00 04 00 00 00 01   // CH2O
                    if(val != m_values[DE_CH2O])
                      m_bUpdated = true;
                    m_values[DE_CH2O] = val;
                    break;

                  case 0x12: // 12 02 00 04 00 00 00 E5  // temp C
                    if(val == 0) break; // reset
                    if(m_bCF)
                      val = val * 90 / 50 + 320;
                    val += tcal;
                    if(val != m_values[DE_TEMP] )
                      m_bUpdated = true;
                    m_values[DE_TEMP] = val;
                    break;
                  case 0x13: // 13 02 00 04 00 00 02 1B  // Rh
                    if(val == 0) break; // reset
                    if(val != m_values[DE_RH] )
                      m_bUpdated = true;
                    m_values[DE_RH] = val + rhcal;
                    break;
                  case 0x15: // 15 02 00 04 00 00 00 01  // VOC
                    if(val != m_values[DE_VOC] )
                      m_bUpdated = true;
                    m_values[DE_VOC] = val;
                    break;
                  case 0x16: // 16 02 00 04 00 00 01 66  // CO2
                    if(val != m_values[DE_CO2] )
                      m_bUpdated = true;
                    m_values[DE_CO2] = val;
                    break;

#else // Tuya T & H clock Type A (banggood)
                  case 1: // 01 02 00 04 00 00 00 FC  // temp 00FC = 25.2C 77.36F
                    if(m_bCF)
                      val = val * 90 / 50 + 320;
                    val += tcal;
                    if(val != m_values[DE_TEMP] )
                      m_bUpdated = true;
                    m_values[DE_TEMP] = val;
                    break;
                  case 2: // 02 02 00 04 00 00 00 3B   // rh 003B = 55%
                    val *= 10;
                    val += rhcal;
                    if(val != m_values[DE_RH])
                      m_bUpdated = true;
                    m_values[DE_RH] = val;
                    break;
#endif
                  case 9: // len = 5
                  // 09 04 00 01 01 // C/F button setting
//                  m_bCF = inBuffer[4];
                    break;
                  default:
                   {
                      String s = "{\"cmd\":\"print\",\"text\":\"unknown register ";
                      s += inBuffer[0];
                      s += "\"}";
                      WsSend(s);
                    }
                    break;
                }
                break;
              case TC_SET_DATE: // requesting date
                sendDate();
                break;
              case TC_SIGNAL:
                buf[0] = m_signal;
                writeSerial(TC_SIGNAL, buf, 1);
                break;
              case TC_UNK1: // not sure yet
                buf[0] = 4;
                writeSerial(TC_UNK1, buf, 1);
                break;
              default:
                {
                  String s = "{\"cmd\":\"print\",\"text\":\"Tuya cmd unknown ";
                  s += cmd;
                  s += "\"}";
                  WsSend(s);
                }
                break;
            }
          }
          state = 0;
          idx = 0;
          len = 0;
        }
        break;
    }
    return 0;
  }

  static uint8_t sec;
  if(sec != second())
  {
    sec = second();
    if(--m_cs == 0) // start the sequence
    {
      writeSerial(TC_HEARTBEAT);
      m_cs = 15;
    }
  }

  static bool bBtn = true;
  if( digitalRead(BTN) != bBtn )
  {
    bBtn = digitalRead(BTN);
//    if( bBtn ) // release
//      setCF( !m_bCF );
  }
  return 0;
}

int TuyaInterface::status()
{
  return m_status;
}

void TuyaInterface::setSignal(int db)
{
  m_signal = 255 + (db * 2);
}

void TuyaInterface::setCF(bool f)
{
  m_bCF = f;

/*  uint8_t data[5];

  data[0] = 0x09;
  data[1] = 0x04;
  data[2] = 0x00;
  data[3] = 0x01; // byte value
  data[4] = f ? 1:0; // C/F
  writeSerial(TC_STATE, data, 5);
  */
}

//  writeSerial(TC_SET_DP);

void TuyaInterface::sendDate()
{
  tmElements_t tm;
  breakTime(now(), tm);

  uint8_t data[8];
  data[0] = 1; // must be 1
  data[1] = tm.Year - 30; // offset from 1971
  data[2] = tm.Month;
  data[3] = tm.Day;
  data[4] = tm.Hour;
  data[5] = tm.Minute;
  data[6] = tm.Second;
  data[7] = weekday() - 1;
  writeSerial(TC_SET_DATE, data, 8);
}

bool TuyaInterface::writeSerial(uint8_t cmd, uint8_t *p, uint16_t len)
{
  uint8_t buf[16];

  buf[0] = 0x55;
  buf[1] = 0xAA;
  buf[2] = 0; // version
  buf[3] = cmd;
  buf[4] = len >> 8; // 16 bit len big endien
  buf[5] = len & 0xFF;

  int i;
  if(p) for(i = 0; i < len; i++)
    buf[6 + i] = p[i];

  uint16_t chk = 0;
  for(i = 0; i < len + 6; i++)
    chk += buf[i];
  buf[6 + len] = (uint8_t)chk;

#ifdef DEBUG
  String s = "{\"cmd\":\"print\",\"text\":\"TX ";
  s += len;
  s += " ";
  for(int a = 0; a < len+7; a++)
  {
    s += " ";
    s += String(buf[a], 16);
  }
  s += "\"}";
  WsSend(s);
#endif
  return Serial.write(buf, 7 + len);
}

void TuyaInterface::setLED(uint8_t no, bool bOn)
{
#ifdef ESP_LED
  m_bLED[no] = bOn;
  no = constrain(no, 0, 1);
  if(no == 0)
    digitalWrite(WIFI_LED, bOn);
  else
    digitalWrite(ESP_LED, !bOn);
#else // no LED on module
  m_bLED[0] = bOn;
  digitalWrite(WIFI_LED, bOn);
#endif
}
