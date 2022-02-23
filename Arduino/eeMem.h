#ifndef EEMEM_H
#define EEMEM_H

#include <Arduino.h>

struct Sched
{
  uint16_t setTemp;
  uint16_t timeSch;
  uint8_t thresh;
  uint8_t wday;
  char    name[16]; // names for small display
}; // 22

struct flags_t
{
  uint16_t Mode:3;
  uint16_t heatMode:2;
  uint16_t humidMode:3;
  uint16_t nSchedMode:3; // 0=forecast, 1=sine, 2=flat
  uint16_t nFcstSource:2; // 0=local, 1=OpenWeatherMap
  uint16_t bCelcius:1;
  uint16_t bLock:1;
  uint16_t res:1;
};

#define EESIZE (offsetof(eeMem, end) - offsetof(eeMem, size) )

class eeMem
{
public:
  eeMem();
  bool init(void);
  bool check(void);
  bool update(void);
  uint16_t getSum(void);

private:
  uint16_t Fletcher16( uint8_t* data, int count);

public:
  uint16_t size = EESIZE;            // if size changes, use defaults
  uint16_t sum = 0xAAAA;             // if sum is different from memory struct, write
  char     szSSID[24] = "";  // Enter you WiFi router SSID
  char     szSSIDPassword[24] = ""; // and password
  uint16_t coolTemp[2] = {850, 860}; // cool to temp *10 low/high F/C issue
  uint16_t heatTemp[2] = {740, 750}; // heat to temp *10 low/high F/C issue
  flags_t  b =  {0,0,0,0,0,0,0,0};   // see flags_t
  int8_t   cycleThresh[2] = {28, 8}; // temp range for cycle *10 [cool|heat] F/C issue
  uint8_t  eHeatThresh = 33;        // degree threshold to switch to gas    F/C issue
  uint16_t cycleMin = 60*4;         // min time to run a cycle in minutes
  uint16_t cycleMax = 60*30;        // max time to run a cycle in minutes
  uint16_t idleMin = 60*8;          // min time to not run in minutes
  uint16_t filterMinutes;           // resettable minutes run timer (200 hours is standard change interval)
  uint16_t fanPostDelay[2] = {60*2, 60*2}; // delay to run auto fan after [hp/cool] stops
  uint16_t fanPreTime[2] = {60*1, 60*1}; // fan pre-run before [cool/heat]
  uint16_t overrideTime = 60*10;    // time used for an override in minutes
  int8_t   tz = -5;                 // current timezone
  int8_t   adj;                     // temp sensor offset adjust by 0.1
  uint16_t rhLevel[2] = {450, 750}; // rh low/high 45%, 75%
  int8_t   awayDelta[2] = {40, -40}; // temp offset in away mode[cool][heat] by 0.1
  uint16_t awayTime = 60*8;         // time limit for away offset (in minutes)
  uint8_t  hostIp[4] = {192,168,31,100}; // Device to read local forecast info
  uint16_t hostPort = 80;
  char     cityID[8] = "4291945";   // For OpenWeatherMap  4311646
  char     password[24] = "password"; // Web interface password
  uint8_t  fcRange = 23;            // number in forecasts (3 hours)
  uint8_t  fcDisplay = 46;          // number in forecasts (3 hours)
  uint16_t iSecsDay[32][3] = { // Saved from latest
   {0,0,1794},{0,0,1794},{0,0,1196},{5460,0,9072},{14343,0,21945},{14635,0,21993},{16979,0,26391},{2783,0,5246},{2675,0,4414},{0,0,1794},{3183,0,5402},
   {6896,0,9241},{5280,0,7381},{6663,0,8945},{8941,0,11168},{10682,0,16647},{7228,0,12776},{3022,0,4879},{4659,1156,8608},{6037,0,8319},{4487,0,6289},
   {2327,0,4184},{3012,0,5050},{5853,0,7954},{14164,0,16753},{16901,0,19671},{10425,0,12589},{11859,0,14747},{10630,0,13156},{11409,0,13998},{0,0,1794}};
  uint32_t iSecsMon[12][3] = { // Save from latest (compressor,gas,fan)
   {199124,36845,336081},{16565,357781,769917},{0,170664,419895},{523,57837,146926},{27756,26256,113103},{153956,0,235082},
   {212466,0,304331},{0,0,4784},{0,0,0},{0,0,0},{113429,52189,295135},{0,251368,492489}};
  uint16_t ppkwh = 147;             // price per KWH in cents * 10000 (0.147)
  uint16_t ccf = 1190;              // nat gas cost per 1000 cubic feet in 10th of cents * 1000 ($1.190)
  uint16_t cfm = 820;               // cubic feet per minute * 1000 of furnace (0.82)         // cubic feet per minute
  uint16_t compressorWatts = 2600;  // compressorWatts
  uint8_t  fanWatts = 250;          // Blower motor watts
  uint8_t  furnaceWatts = 220;      // 1.84A inducer motor mostly
  uint8_t  humidWatts = 150;
  uint16_t furnacePost = 114;       // furnace internal fan timer
  uint16_t diffLimit = 300;         // in/out thermal differential limit. Set to 30 deg limit    F/C issue
  int16_t  fcOffset[2] = {-180,0};  // forecast offset adjust in minutes (cool/heat)
  uint16_t fanIdleMax = 60*4;       // fan idle max in minutes
  uint8_t  fanAutoRun = 5;          // 5 minutes on
  int16_t  sineOffset[2] = {0, 0};  // sine offset adjust (cool/heat)
  uint32_t sensorActive[3];          // sensor IDs ifor restart
  uint8_t  end;
}; // 512 bytes

static_assert(EESIZE <= 512, "EEPROM struct too big");

extern eeMem ee;

#endif // EEMEM_H
