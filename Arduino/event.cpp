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

#include "event.h"

void eventHandler::set(WiFiClient c, int interval, uint8_t nType)
{
  if(nType == Type_Critical) // critical connection
  {
    m_critical_timer = m_timeout = interval << 1; // 2 tries
  }

  for(int i = 0; i < CLIENTS; i++) // find an unused client
    if(!ec[i].inUse())
    {
      ec[i].set(c, interval, nType);
      break;
    }
}

void eventHandler::heartbeat()
{
  bool bConnected = false;

  for(int i = 0; i < CLIENTS; i++)
  {
    if(ec[i].inUse())
    {
      ec[i].beat();
      bConnected = true;
    }
  }

  if(m_timeout)
  {
    if(!bConnected) // nothing connected but a critical connection was made
    {
      if(--m_critical_timer == 0) // give it double the time requested
      {
        ESP.reset();  // all outputs will be reset to off in hvac instance on startup
      }
    }
    else
    {
      m_critical_timer = m_timeout; // still detecting a connection so reset counter
    }
  }
}

void eventHandler::push() // push to all
{
  for(int i = 0; i < CLIENTS; i++)
    ec[i].push();
}

void eventHandler::pushInstant() // push only to instant type
{
  for(int i = 0; i < CLIENTS; i++)
   ec[i].pushInstant();
}

void eventHandler::print(String s) // print remote debug
{
  for(int i = 0; i < CLIENTS; i++)
    ec[i].print(s);
}

void eventHandler::printf(const char *format, ...) {
  va_list arg;
  va_start(arg, format);
  char temp[1460];
  size_t len = ets_vsnprintf(temp, 1460, format, arg);
  for(int i = 0; i < CLIENTS; i++)
    ec[i].print(temp);
  va_end(arg);
}

void eventHandler::alert(String s) // send alert event
{
  for(int i = 0; i < CLIENTS; i++)
    ec[i].alert(s);
}

// *** eventClient ***

void eventClient::set(WiFiClient cl, int t, uint8_t nType)
{
  m_client = cl;
  m_interval = t;
  m_timer = 2; // send data in 2 seconds
  m_keepAlive = 10;
  m_nType = nType;
  m_client.print(":ok\n");
}

bool eventClient::inUse()
{
  return m_client.connected();
}

void eventClient::push()
{
  if(m_client.connected() == 0)
    return;
  m_keepAlive = 11; // anything sent resets keepalive
  m_timer = m_interval;
  String s = jsonCallback();
  m_client.print("event: state\ndata: " + s + "\n");
}

void eventClient::pushInstant()
{
  if(m_nType) // instant or critical type request
    push();
}

void eventClient::print(String s) // print event
{
  m_client.print("event: print\ndata: " + s + "\n");
}

void eventClient::alert(String s) // send a json formatted alert event
{
  m_client.print("event: alert\ndata: " + s + "\n");
}

void eventClient::beat()
{
  if(m_client.connected() == 0)
    return;

  if(--m_timer == 0)
  {
    m_timer = m_interval; // push data on requested time interval
    push();
  }
  if(--m_keepAlive == 0)
  {
    m_client.print("\n");     // send something to keep connection from timing out on other end
    m_keepAlive = 10;
  }
}
