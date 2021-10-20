class jsonString
{
public:
  jsonString(const char *pLabel = NULL, int rsv = 0)
  {
    m_cnt = 0;
    if(rsv) s.reserve(rsv);
    if(pLabel)
      s = pLabel, s += ";";
    s += "{";
  }
        
  String Close(void)
  {
    s += "}";
    return s;
  }

  void Var(char *key, int iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(char *key, uint32_t iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(char *key, long int iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(char *key, float fVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += fVal;
    m_cnt++;
  }
  
  void Var(char *key, bool bVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += bVal ? 1:0;
    m_cnt++;
  }
  
  void Var(char *key, char *sVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":\"";
    s += sVal;
    s += "\"";
    m_cnt++;
  }
  
  void Var(char *key, String sVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":\"";
    s += sVal;
    s += "\"";
    m_cnt++;
  }

  void Array(char *key, uint16_t iVal[], int n)
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
  void Array(char *key, Sched sVal[], int n)
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

  void ArrayCost(char *key, uint16_t iVal[], int n)
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

  void Array(char *key, Sensor sns[])
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
        s += sns[i].flags;
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
