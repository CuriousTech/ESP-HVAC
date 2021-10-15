#include "tempArray.h"
#include "eeMem.h"
#include "jsonstring.h"
#include <TimeLib.h>
#include <FS.h>

extern void WsSend(String s);

void TempArray::init(uint16_t flags)
{
  File F;

  F = SPIFFS.open("/weekly", "r");
  F.read((byte*) &m_weekly, sizeof(m_weekly));
  F.close();
  F = SPIFFS.open("/daily", "r");
  F.read((byte*) &m_daily, sizeof(m_daily));
  F.close();
  m_dataFlags = flags;
}

void TempArray::saveData()
{
  File F;

  F = SPIFFS.open("/weekly", "w");
  F.write((byte*) &m_weekly, sizeof(m_weekly));
  F.close();
  F = SPIFFS.open("/daily", "w");
  F.write((byte*) &m_daily, sizeof(m_daily));
  F.close();
}

void TempArray::update(uint16_t Values[])
{
  if(Values[DE_TEMP] == 0 && Values[DE_RH] == 0)
    return;

  for(int i = 0; i < DE_COUNT; i++)
    if(Values[i] > m_peakVal[i])
      m_peakVal[i] = Values[i];
  if(m_nWeek >= 0)
    logLH(Values, m_weekly, m_nWeek);
  if(m_nWeekDay >= 0)
    logLH(Values, m_daily, m_nWeekDay);

 WsSend("print;update");
  m_sampleCount++;
}

void TempArray::logLH(uint16_t Values[], LHLog log[], int idx)
{
  if(Values[DE_TEMP] < log[idx].temp[0]) log[idx].temp[0] = Values[DE_TEMP];
  checkAlert("Temp", false, Values[DE_TEMP], ee.wAlertLevel[0]);
  if(Values[DE_TEMP] > log[idx].temp[1]) log[idx].temp[1] = Values[DE_TEMP];
  checkAlert("Temp", true, Values[DE_TEMP], ee.wAlertLevel[1]);

  if(Values[DE_RH ] < log[idx].rh[0])   log[idx].rh[0] =  Values[DE_RH];
  checkAlert("Rh", false, Values[DE_RH], ee.wAlertLevel[2]);
  if(Values[DE_RH ] > log[idx].rh[1])   log[idx].rh[1] =  Values[DE_RH];
  checkAlert("Rh", true, Values[DE_RH], ee.wAlertLevel[3]);

  if(Values[DE_CO2] < log[idx].co2[0])  log[idx].co2[0] = Values[DE_CO2];
  checkAlert("CO2", false, Values[DE_CO2], ee.wAlertLevel[4]);
  if(Values[DE_CO2] > log[idx].co2[1])  log[idx].co2[1] = Values[DE_CO2];
  checkAlert("CO2", true, Values[DE_CO2], ee.wAlertLevel[5]);

  if(Values[DE_VOC] < log[idx].voc[0])  log[idx].voc[0] = Values[DE_VOC];
  checkAlert("VOC", false, Values[DE_VOC], ee.wAlertLevel[6]);
  if(Values[DE_VOC] > log[idx].voc[1])  log[idx].voc[1] = Values[DE_VOC];
  checkAlert("VOC", true, Values[DE_VOC], ee.wAlertLevel[7]);

  if(Values[DE_CH2O] < log[idx].ch2o[0]) log[idx].ch2o[0] = Values[DE_CH2O];
  checkAlert("CH2O", false, Values[DE_CH2O], ee.wAlertLevel[8]);
  if(Values[DE_CH2O] > log[idx].ch2o[1]) log[idx].ch2o[1] = Values[DE_CH2O];
  checkAlert("CH2O", true, Values[DE_CH2O], ee.wAlertLevel[9]);
}

void TempArray::checkAlert(String sName, bool bUD, uint16_t nNow, uint16_t nAlert)
{
  if(m_bSilence || nAlert == 0)
    return;

  if( (bUD && nNow > nAlert) || (!bUD && nNow < nAlert) )
  {
    String s = "alert;{\"text\":\"";
    s += sName;
    s += bUD ? " above " : " below ";
    s += nAlert;
    s += " at ";
    s += nNow;
    s += "\"}";
    WsSend(s);
    // Todo: send report
  }
}

void TempArray::rangeAlert(char *name, int16_t val)
{
  String s = "alert;{\"text\":\"";
  s += name;
  s += " diff out of range ";
  s += val;
  s += "\"}";
  WsSend(s);
}

void TempArray::resetLogEntry(LHLog log[], int idx)
{
  log[idx].temp[0] = 0xFFFF;
  log[idx].temp[1] = 0;
  log[idx].rh[0] = 0xFFFF;
  log[idx].rh[1] = 0;
  log[idx].co2[0] = 0xFFFF;
  log[idx].co2[1] = 0;
  log[idx].ch2o[0] = 0xFF;
  log[idx].ch2o[1] = 0;
  log[idx].voc[0] = 0xFF;
  log[idx].voc[1] = 0;
}

void TempArray::add(uint32_t date, AsyncWebSocket &ws, int WsClientID)
{
  if((m_peakVal[DE_TEMP] == 0 && m_peakVal[DE_RH] == 0) || m_bValidDate == false) // nothing to do
    return;

  tempArr *p = &m_log[m_idx];

  if(m_lastDate == 0)
    m_lastDate = date;
  p->m.tmdiff = constrain(date - m_lastDate, 0, 0xFFF);
  m_lastDate = date;

  int16_t val = m_lastVal[DE_TEMP] - m_peakVal[DE_TEMP];
  p->m.temp = val;
  if(p->m.temp != val )
  {
    p->m.temp /= 2;
    rangeAlert("Temp", val);
    p->m.error |= DF_TEMP;
  }
  val = m_lastVal[DE_RH ] - m_peakVal[DE_RH ];
  p->m.rh = val;
  if(p->m.rh != val )
  {
    p->m.rh /= 2;
    rangeAlert("Rh", val);
    p->m.error |= DF_RH;
  }

  if(m_dataFlags & DF_CO2)
  {
    val = m_lastVal[DE_CO2] - m_peakVal[DE_CO2];
    p->m.co2 = val;
    if(p->m.co2 != val )
    {
      p->m.co2 /= 2;
      rangeAlert("CO2", val );
      p->m.error |= DF_CO2;
    }
  }
  if(m_dataFlags & DF_CH2O)
  {
    val = m_lastVal[DE_CH2O] - m_peakVal[DE_CH2O];
    p->m.ch2o = val;
    if(p->m.ch2o != val )
    {
      p->m.ch2o /= 2;
      rangeAlert("CH2O", val );
      p->m.error |= DF_CH2O;
    }
  }
  if(m_dataFlags & DF_VOC)
  {
    val = m_lastVal[DE_VOC ] - m_peakVal[DE_VOC ];
    p->m.voc  = val;
    if(p->m.voc != val )
    {
      p->m.voc /= 2;
      rangeAlert("VOC", val );
      p->m.error |= DF_VOC;
    }
  }

  tmElements_t tm;
  breakTime(date, tm);
  tm.Hour = tm.Minute = tm.Second = 0;
  tm.Wday = 1;
  tm.Month = 1;
  tm.Day = 1;

  uint32_t tmDiff = date - makeTime(tm);
  int8_t week = tmDiff / (60*60*24*7);
  bool bReset = (m_nWeek != -1 && week != m_nWeek);

  m_nWeek = week;

  if(bReset)
  {
    resetLogEntry(m_weekly, m_nWeek);
  }

  if(weekday()-1 != m_nWeekDay)
  {
    bReset = (m_nWeekDay != -1);
    m_nWeekDay = weekday() - 1;
    if(bReset)
    {
      saveData();
      resetLogEntry(m_daily, m_nWeekDay);
    }
  }

  m_sampleCount = 0;
  sendNew(m_peakVal, date, ws, WsClientID);
  memcpy(&m_lastVal, m_peakVal, sizeof(m_lastVal));
  memset(&m_peakVal, 0, sizeof(m_peakVal)); // reset the peaks

  if(++m_idx >= LOG_CNT)
    m_idx = 0;
  m_log[m_idx].m.u[0] = 0; // mark as invalid data/end
}

bool TempArray::get(int &pidx, int n)
{
  if(n < 0 || n > LOG_CNT-1) // convert 0-(LOG_CNT-1) to reverse index circular buffer
    return false;
  int idx = m_idx - 1 - n; // 0 = last entry
  if(idx < 0) idx += LOG_CNT;
  if(m_log[idx].m.u[0] == 0) // invalid data
    return false;
  pidx = idx;
  return true;
}

#define CHUNK_SIZE 800

// send the log in chucks of CHUNK_SIZE
void TempArray::historyDump(bool bStart, AsyncWebSocket &ws, int WsClientID)
{
  static bool bSending;
  static int entryIdx;

  int aidx;
  if(bStart)
  {
    m_nSending = 1;

    entryIdx = 0;
    if( get(aidx, 0) == false)
    {
      bSending = false;
      ws.text(WsClientID, "data;{\"d\":[]}");
      return;
    }

    jsonString js("ref");

    js.Var("tb"  , m_lastDate); // date of first entry
    char *labels[] = {"Temp", "Rh", "CO2", "CH2O", "VOC", NULL};
    js.Array("label", labels);
    uint16_t decimals[] = {1, 1, 0, 0, 0};
    js.Array("dec", decimals, sizeof(decimals)/sizeof(uint16_t));
    js.Array("base", m_lastVal, sizeof(m_lastVal)/sizeof(uint16_t));
    js.Array("alert", ee.wAlertLevel, sizeof(ee.wAlertLevel)/sizeof(uint16_t));
    ws.text(WsClientID, js.Close());
  }

  switch(m_nSending)
  {
    case 0: return;
    case 1: // daily
      logDump(true, ws, WsClientID, 1);
      m_nSending = 2;
      return;
    case 2: // weekly
      if(logDump(true, ws, WsClientID, 0) == false)
        m_nSending = 4; // completed
      else
        m_nSending = 3;
      return;
    case 3: // weekly cont
      if(logDump(false, ws, WsClientID, 0) == false)
        m_nSending = 4;
      return;
  }

  // minute
  String out;
  out.reserve(CHUNK_SIZE + 100);

  out = "data;{\"d\":[";

  bool bC = false;

  for(; entryIdx < LOG_CNT - 1 && out.length() < CHUNK_SIZE && get(aidx, entryIdx); entryIdx++)
  {
    int len = out.length();
    if(bC) out += ",";
    bC = true;
    out += "[";         // [seconds, temp, rh],
    out += m_log[aidx].m.tmdiff; // seconds differential from next entry
    out += ",";
    out += m_log[aidx].m.temp;
    out += ",";
    out += m_log[aidx].m.rh;
    if(m_dataFlags & DF_CO2 )
    {
      out += ",";
      out += m_log[aidx].m.co2;
    }
    if(m_dataFlags & DF_CH2O )
    {
      out += ",";
      out += m_log[aidx].m.ch2o;
    }
    if(m_dataFlags & DF_VOC )
    {
      out += ",";
      out += m_log[aidx].m.voc;
    }
    out += ",";
    out += m_log[aidx].m.error;
    out += "]";
    if( out.length() == len) // memory full
    {
    ws.text(WsClientID, "print;mem");
      break;
    }
  }
  out += "]}";
  if(bC == false) // done
    m_nSending = 0;
  else
    ws.text(WsClientID, out);
}

bool TempArray::logDump(bool bStart, AsyncWebSocket &ws, int WsClientID, int logType)
{
  static bool bSending[3];
  static int entryIdx;

  if(bStart)
    bSending[logType] = true;
  if(bSending[logType] == false)
    return false;

  if(bStart)
    entryIdx = 0;

  String out;
  out.reserve(CHUNK_SIZE + 100);

  LHLog *pLog = m_daily;
  int nCount = 7;

  switch(logType)
  {
    case 0:// Weekly
      pLog = m_weekly;
      nCount = 52;
      out = "weekly;{\"d\":[";
      break;
    case 1:// daily
      pLog = m_daily;
      nCount = 7;
      out = "daily;{\"d\":[";
      break;
  }

  bool bC = false;

  for(; entryIdx < nCount && out.length() < CHUNK_SIZE; entryIdx++)
  {
    int len = out.length();
    if(bC) out += ",";
    bC = true;
    out += "[";
    out += pLog[entryIdx].temp[0];
    out += ",";
    out += pLog[entryIdx].temp[1];
    out += ",";
    out += pLog[entryIdx].rh[0];
    out += ",";
    out += pLog[entryIdx].rh[1];
    if(m_dataFlags & DF_CO2 )
    {
      out += ",";
      out += pLog[entryIdx].co2[0];
      out += ",";
      out += pLog[entryIdx].co2[1];
    }
    if(m_dataFlags & DF_CH2O )
    {
      out += ",";
      out += pLog[entryIdx].ch2o[0];
      out += ",";
      out += pLog[entryIdx].ch2o[1];
    }
    if(m_dataFlags & DF_VOC )
    {
      out += ",";
      out += pLog[entryIdx].voc[0];
      out += ",";
      out += pLog[entryIdx].voc[1];
    }
    out += "]";
    if( out.length() == len) // memory full
      break;
  }
  out += "]}";
  if(bC)
    ws.text(WsClientID, out);
  else
    bSending[logType] = false;
  return bC;
}

void TempArray::sendNew(uint16_t Values[], uint32_t date, AsyncWebSocket &ws, int WsClientID)
{
  String out = "data2;{\"d\":[[";

  out += m_lastDate;
  out += ",";
  out += Values[DE_TEMP];
  out += ",";
  out += Values[DE_RH];
  if(m_dataFlags & DF_CO2)
  {
    out += ",";
    out += Values[DE_CO2];
  }
  if(m_dataFlags & DF_CH2O )
  {
    out += ",";
    out += Values[DE_CH2O];
  }
  if(m_dataFlags & DF_VOC )
  {
    out += ",";
    out += Values[DE_VOC];
  }
    
  out += "]]}";
  ws.text(WsClientID, out);
}
