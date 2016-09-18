#ifndef ENCODER_H
#define ENCODER_H
#include <arduino.h>

class Encoder
{
public:
  	Encoder(int8_t aPin, int8_t bPin, void (*callbackr)());
    Encoder(int8_t aPin, int8_t bPin);
    int poll(void);
    int read(void);
    void isr(void);
private:
    void (*isrCallback)(void);
    volatile int8_t m_ticks;
    volatile int8_t m_dir;
    volatile int8_t m_aPin, m_bPin;
    int8_t m_lastA;
};

#endif // ENCODER_H
