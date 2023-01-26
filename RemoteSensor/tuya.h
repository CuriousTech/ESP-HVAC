#ifndef TUYA_H
#define TUYA_H

#include <Arduino.h>
#include "defs.h"

// Tuya commands
enum TuyaCmd{
  TC_HEARTBEAT,     // 0  MCU responds with 0 or 1
  TC_QUERY_PRODUCT, // 0x01 MCU responds with long product string
  TC_MCU_CONF,    //  0x02
  TC_WIFI_STATE,  // 0x03
  TC_WIFI_RESET,  // 0x04
  TC_WIFI_SELECT, // 0x05
  TC_SET_DP,      // 0x06
  TC_STATE,       // 0x07
  TC_QUERY_STATE, // 0x08
  TC_SET_DATE = 0x1C,
  TC_SIGNAL = 0x24,  // From MCU, respond with 00-D9 for singal strength
  TC_UNK1 = 0x2B,  // From MCU, respond with 02-D4
};

enum TuyaDataType{
  TT_NULL,
  TT_BOOL,  // 0x01
  TT_VALUE, // 0x02
  TT_STRING,// 0x03
  TT_ENUM,  // 0x04
};

enum TuyaDataRegister{
  TR_NULL,
  TR_TEMP1, // Tuya clock
  TR_CHO2 = 2, // Rh on Tuya clock
  TR_TEMP = 0x12, // C
  TR_RH  = 0x13, // %
  TR_VOC = 0x15, // mg/m3
  TR_CO2 = 0x16, // ppm
};

class TuyaInterface
{
public:
  TuyaInterface(){};
  void init(bool bCF);
  int service(int8_t tcal, int8_t rhcal);
  void setLED(uint8_t no, bool bOn);
  void setCF(bool f);
  void setSignal(int db);
  int status(void);
  bool    m_bLED[2];
  bool    m_bUpdated = false;
  bool    m_bCF = false;
  uint16_t m_dataFlags;
  uint16_t m_values[6];
private:
  bool writeSerial(uint8_t cmd, uint8_t *p = NULL, uint16_t len = 0);
  void checkStatus(void);
  void sendDate(void);
  uint8_t m_cs = 2;
  int m_status = 0;
  uint8_t m_mcuConf[2];
  uint8_t m_signal = 0;
};

#endif // TUYA_H
