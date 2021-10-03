#include "tempArray.h"
#include "eeMem.h"
#include "jsonstring.h"

extern void WsSend(String s);

void TempArray::update(uint16_t *pValues)
{
  if(pValues[DE_TEMP] == 0 && pValues[DE_RH] == 0)
    return;

  for(int i = 0; i < DE_COUNT; i++)
    if(pValues[i] > m_peakVal[i])
      m_peakVal[i] = pValues[i];

  m_sampleCount++;
}

void TempArray::add(uint16_t flags, uint32_t date, AsyncWebSocket &ws, int WsClientID)
{
  if(m_peakVal[DE_TEMP] == 0 && m_peakVal[DE_RH] == 0) // nothing to do
    return;

  tempArr *p = &m_log[m_idx];

  m_flags = flags;
  if(m_lastDate == 0)
    m_lastDate = date;
  p->m.tmdiff = date - m_lastDate;
  m_lastDate = date;

  p->m.temp = m_lastVal[DE_TEMP] - m_peakVal[DE_TEMP];
  p->m.rh   = m_lastVal[DE_RH ]  - m_peakVal[DE_RH ];

  if(p->m.temp != ( m_lastVal[DE_TEMP] - m_peakVal[DE_TEMP]) )
  {
    p->m.temp /= 2;
    String s = "alert;Temp diff out of range ";
    s += ( m_lastVal[DE_TEMP] - m_peakVal[DE_TEMP]);
    WsSend(s);
  }
  if(p->m.rh != ( m_lastVal[DE_RH] - m_peakVal[DE_RH]) )
  {
    p->m.rh /= 2;
    String s = "alert;Rh diff out of range ";
    s += ( m_lastVal[DE_RH] - m_peakVal[DE_RH]);
    WsSend(s);
  }

  if(m_flags & DF_CO2)
  {
    p->m.co2  = m_lastVal[DE_CO2]  - m_peakVal[DE_CO2];
    if(p->m.co2 != ( m_lastVal[DE_CO2] - m_peakVal[DE_CO2]) )
    {
      p->m.co2 /= 2;
      WsSend("alert;CO2 diff out of range");
    }
  }
  if(m_flags & DF_CH2O)
  {
    p->m.ch2o = m_lastVal[DE_CH2O] - m_peakVal[DE_CH2O];
    if(p->m.ch2o != ( m_lastVal[DE_CH2O] - m_peakVal[DE_CH2O]) )
    {
      p->m.ch2o /= 2;
      WsSend("alert;CO2 diff out of range");
    }
  }
  if(m_flags & DF_VOC)
  {
    p->m.voc  = m_lastVal[DE_VOC ] - m_peakVal[DE_VOC ];
    if(p->m.voc != ( m_lastVal[DE_VOC] - m_peakVal[DE_VOC]) )
    {
      p->m.voc /= 2;
      WsSend("alert;VOC diff out of range");
    }
  }

  m_sampleCount = 0;
  sendNew(m_peakVal, flags, date, ws, WsClientID);
  memcpy(&m_lastVal, m_peakVal, sizeof(m_lastVal));
  memset(&m_peakVal, 0, sizeof(m_peakVal)); // reset the peaks

  if(++m_idx >= LOG_CNT)
    m_idx = 0;
  m_log[m_idx].m.u[0] = 0; // mark as invalid data/end
}

bool TempArray::get(tempArr *pa, int n)
{
  if(n < 0 || n > LOG_CNT-1) // convert 0-(LOG_CNT-1) to reverse index circular buffer
    return false;
  int idx = m_idx - 1 - n; // 0 = last entry
  if(idx < 0) idx += LOG_CNT;
  if(m_log[idx].m.u[0] == 0) // invalid data
    return false;
  memcpy(pa, &m_log[idx], sizeof(tempArr));
  return true;
}

#define CHUNK_SIZE 800

// send the log in chucks of CHUNK_SIZE
void TempArray::historyDump(bool bStart, AsyncWebSocket &ws, int WsClientID)
{
  static bool bSending;
  static int entryIdx;

  if(bStart)
    bSending = true;
  if(bSending == false)
    return;

  tempArr gpt;

  if(bStart)
  {
    entryIdx = 0;
    if( get(&gpt, 0) == false)
    {
      bSending = false;
      return;
    }

    jsonString js("ref");

    js.Var("tb"  , m_lastDate); // date of first entry
    js.Var("temp", m_lastVal[ DE_TEMP ]);
    js.Var("rh"  , m_lastVal[ DE_RH ]);
    js.Var("co2" , m_lastVal[ DE_CO2 ]);
    js.Var("ch2o", m_lastVal[ DE_CH2O ]);
    js.Var("voc" , m_lastVal[ DE_VOC ]);

    ws.text(WsClientID, js.Close());
  }

  String out;
  out.reserve(CHUNK_SIZE + 100);

  out = "data;{\"d\":[";

  bool bC = false;

  for(; entryIdx < LOG_CNT - 1 && out.length() < CHUNK_SIZE && get(&gpt, entryIdx); entryIdx++)
  {
    int len = out.length();
    if(bC) out += ",";
    bC = true;
    out += "[";         // [seconds, temp, rh],
    out += gpt.m.tmdiff; // seconds differential from next entry
    out += ",";
    out += gpt.m.temp;
    out += ",";
    out += gpt.m.rh;
    if(m_flags & DF_CO2 )
    {
      out += ",";
      out += gpt.m.co2;
    }
    if(m_flags & DF_CH2O )
    {
      out += ",";
      out += gpt.m.ch2o;
    }
    if(m_flags & DF_VOC )
    {
      out += ",";
      out += gpt.m.voc;
    }
    out += "]";
    if( out.length() == len) // memory full
      break;
  }
  if(bC) // don't send blank
  {
    out += "]}";
    ws.text(WsClientID, out);
  }
  else
    bSending = false;
}

void TempArray::sendNew(uint16_t *pValues, uint16_t flags, uint32_t date, AsyncWebSocket &ws, int WsClientID)
{
  String out = "data2;{\"d\":[[";

  out += m_lastDate;
  out += ",";
  out += pValues[DE_TEMP];
  out += ",";
  out += pValues[DE_RH];
  if(flags & DF_CO2)
  {
    out += ",";
    out += pValues[DE_CO2];
  }
  if(flags & DF_CH2O )
  {
    out += ",";
    out += pValues[DE_CH2O];
  }
  if(flags & DF_VOC )
  {
    out += ",";
    out += pValues[DE_VOC];
  }
    
  out += "]]}";
  ws.text(WsClientID, out);
}
