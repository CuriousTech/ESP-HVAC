#ifndef BASICSENSOR_H
#define BASICSENSOR_H

#include <Arduino.h>
#include "RunningMedian.h"
#include <AM2320.h>
#include "defs.h"

class BasicInterface
{
public:
  BasicInterface(){};
  void init(bool bCF);
  int service(int8_t tcal, int8_t rhcal);
  void setLED(uint8_t no, bool bOn);
  void setCF(bool f);
  int status(void);
  void setSignal(int db){};
  bool    m_bLED[2];
  bool    m_bUpdated;
  bool    m_bCF;
  uint16_t m_dataFlags = 3;
  uint16_t m_values[6];
  uint8_t m_signal;
private:
  RunningMedian<uint16_t, 25> m_tempMedian[2];
  AM2320 m_am;
  int m_status;
};

#endif // BASICSENSOR_H
