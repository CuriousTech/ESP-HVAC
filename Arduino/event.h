/**The MIT License (MIT)

Copyright (c) 2016 by Greg Cunningham, CuriousTech

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <WiFiClient.h>

#define CLIENTS 4

enum Cl_type
{
  Type_Normal,
  Type_PushInstant,
  Type_Critical
};

class eventClient
{
public:
  eventClient(){}
  void set(WiFiClient cl, int t, uint8_t nType);
  bool inUse(void);
  void push(void);  // event: state, data: JSON.   Push the jscript data
  void pushInstant(void); // push changes instant if push=1
  void print(String s);   // event: print, data: text  Print anything
  void beat(void);        // 10 second keepalive
  void alert(String s);   // event: alert, data: text
  String (*jsonCallback)(void); // all the data for pushes
private:
  WiFiClient m_client;
  uint8_t  m_keepAlive; // keep-alive timer (10 seconds)
  uint8_t  m_nType;     // type 1 = critical connection
  uint16_t m_interval;  // interval to send full json created from jsonCallback
  uint16_t m_timer;     // timer for interval
};

class eventHandler
{
public:
  eventHandler(String (*callback)(void) ) : m_critical_timer(0), m_timeout(0)
  {
    for(int i = 0; i < CLIENTS; i++)
    {
      ec[i].jsonCallback = callback;
    }
  }
  void set(WiFiClient c, int interval, uint8_t nType);
  void heartbeat(void);
  void push(void); // push to all
  void pushInstant(void); // push changes to all if push=1
  void print(String s); // print remote debug
  void printf(const char *format, ...); // formatted print
  void alert(String s); // send alert
private:
  eventClient ec[CLIENTS];
  uint16_t m_critical_timer;
  uint16_t m_timeout;
};
