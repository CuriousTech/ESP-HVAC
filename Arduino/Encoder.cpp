#include "Encoder.h"

Encoder::Encoder(int8_t aPin, int8_t bPin, void (*callback)())
{
  pinMode(aPin, INPUT_PULLUP);
  pinMode(bPin, INPUT_PULLUP);

  m_aPin = aPin;
  m_bPin = bPin;

  m_lastA = digitalRead(aPin);
  attachInterrupt(aPin, callback, FALLING);
}

Encoder::Encoder(int8_t aPin, int8_t bPin)
{
  pinMode(aPin, INPUT_PULLUP);
  pinMode(bPin, INPUT_PULLUP);

  m_aPin = aPin;
  m_bPin = bPin;

  m_lastA = digitalRead(aPin);
}

int Encoder::poll()
{
  int8_t A = digitalRead(m_aPin);

  m_dir = 0;

  if(!A && A != m_lastA)  // simulate falling edge of A
  {
    m_ticks ++;
    m_dir = digitalRead(m_bPin) ? -1 : 1;
  }

  m_lastA = A;
  return m_dir;
}

// get changes
int Encoder::read()
{
  if(m_ticks == 0)
    return 0;
  int n = m_dir * m_ticks;
  m_ticks = 0;
  return n;
}

// isr (falling edge of A)
void Encoder::isr()
{
  m_ticks++;
  m_dir = digitalRead(m_bPin) ? -1 : 1;
}
