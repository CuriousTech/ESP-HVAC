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
#define SNS_PR_INC (1 << 2) // Future - increment priority weight
#define SNS_PR_DEC (1 << 3) // Future - decrement priority weight
#define SNS_TOPRI (1 << 4) // 1=timer is for priority, 0=for average
#define SNS_WARN  (1 << 7) // internal flag for data timeout
#define SNS_NEG   (1 << 8)  // From remote or page, set this bit to disable a flag above

#endif // DEFS_H
