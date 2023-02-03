#include "eeMem.h"

class jsonString
{
public:
  jsonString(const char *pLabel = NULL, int rsv = 0)
  {
    m_cnt = 0;
    if(rsv) s.reserve(rsv);
    s = String("{");
    if(pLabel)
    {
      s += "\"cmd\":\"";
      s += pLabel, s += "\",";
    }
  }
        
  String Close(void)
  {
    s += "}";
    return s;
  }

  void Var(const char *key, int iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(const char *key, uint32_t iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(const char *key, long int iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(const char *key, float fVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += fVal;
    m_cnt++;
  }
  
  void Var(const char *key, bool bVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += bVal ? 1:0;
    m_cnt++;
  }
  
  void Var(const char *key, const char *sVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":\"";
    s += sVal;
    s += "\"";
    m_cnt++;
  }
  
  void Var(const char *key, String sVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":\"";
    s += sVal;
    s += "\"";
    m_cnt++;
  }

  void Array(const char *key, uint16_t iVal[], int n)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";
    for(int i = 0; i < n; i++)
    {
      if(i) s += ",";
      s += iVal[i];
    }
    s += "]";
    m_cnt++;
  }

 // custom arrays for waterbed
  void Array(const char *key, Sched sVal[], int n)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";

    for(int i = 0; i < n; i++)
    {
      if(i) s += ",";
      s += "[\"";  s += sVal[i].name; s += "\",";
      s += sVal[i].timeSch;
      s += ","; s += String( (float)sVal[i].setTemp/10, 1 );
      s += ","; s += String( (float)sVal[i].thresh/10,1 );
      s += "]";
    }
    s += "]";
    m_cnt++;
  }

  void ArrayCost(const char *key, uint16_t iVal[], int n)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";
    for(int i = 0; i < n; i++)
    {
      if(i) s += ",";
      s += (float)iVal[i]/100;
    }
    s += "]";
    m_cnt++;
  }

  void Array(const char *key, Sensor sns[])
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";
    bool bSend = false;
    for(int i = 0; i < SNS_CNT; i++)
    {
      if(sns[i].IP)
      {
        if(bSend) s += ",";
        bSend = true;
        s += "[";
        IPAddress ip(sns[i].IP);
        s += "\"";
        s += ip.toString();
        s += "\"";
        s += ",";
        s += sns[i].temp;
        s += ",";
        s += sns[i].rh;
        s += ",";
        s += sns[i].f.val;
        s += ",\"";
        s += (char*)&sns[i].ID;
        s += "\"]";
      }
    }
    s += "]";
    m_cnt++;
  }

protected:
  String s;
  int m_cnt;
};
