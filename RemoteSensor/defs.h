#ifndef DEFS_H
#define DEFS_H

enum DataEnum{
  DE_TEMP,
  DE_RH,
  DE_CO2,
  DE_CH2O,
  DE_VOC,
  DE_COUNT
};

#define DF_TEMP (1<<DE_TEMP)
#define DF_RH   (1<<DE_RH)
#define DF_CO2  (1<<DE_CO2)
#define DF_CH2O (1<<DE_CH2O)
#define DF_VOC  (1<<DE_VOC)

// From HVAC.h
#define SNS_PRI   (1 << 0) // Single sensor overrides all others including internal
#define SNS_EN    (1 << 1) // Enabled = averaged between all enabled
#define SNS_C     (1 << 2) // Data from remote sensor is C or F
#define SNS_F     (1 << 3) // ""
#define SNS_TOPRI (1 << 4) // 1=timer is for priority, 0=for average
#define SNS_LO    (1 << 5) // lo/hi unused as of now
#define SNS_HI    (1 << 6)
#define SNS_WARN  (1 << 7) // internal flag for data timeout
#define SNS_NEG   (1 << 8)  // From remote or page, set this bit to disable a flag above

#endif // DEFS_H
