/*
 * iMON LCD plugin for VDR (C++)
 *
 * (C) 2009 Andreas Brachold <vdr07 AT deltab de>
 *
 * This iMON LCD plugin is free software: you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as published 
 * by the Free Software Foundation, version 3 of the License.
 *
 * See the files README and COPYING for details.
 *
 */

#include <stdint.h>
#include <time.h>
#include <vdr/eitscan.h>

#include "watch.h"
#include "status.h"

//#define MOREDEBUGMSG

// ---
ciMonStatusMonitor::ciMonStatusMonitor(ciMonWatch*    pDev)
: m_pDev(pDev)
{

}

void ciMonStatusMonitor::ChannelSwitch(const cDevice *pDevice, int nChannelNumber)
{
    if (nChannelNumber > 0 
        && pDevice->IsPrimaryDevice() 
        && !EITScanner.UsesDevice(pDevice)
        && (nChannelNumber == cDevice::CurrentChannel()))
    {
#ifdef MOREDEBUGMSG
        dsyslog("iMonLCD: channel switched to %d on DVB %d", nChannelNumber, pDevice->CardIndex());
#endif
        m_pDev->Channel(nChannelNumber);
    }
}

void ciMonStatusMonitor::SetVolume(int Volume, bool Absolute)
{
#ifdef MOREDEBUGMSG
  dsyslog("iMonLCD: SetVolume  %d %d", Volume, Absolute);
#endif
  m_pDev->Volume(Volume,Absolute);
}

void ciMonStatusMonitor::Recording(const cDevice *pDevice, const char *szName, const char *szFileName, bool bOn)
{
#ifdef MOREDEBUGMSG
  dsyslog("iMonLCD: Recording  %d %s", pDevice->CardIndex(), szName);
#endif
  m_pDev->Recording(pDevice,szName,szFileName,bOn);
}

void ciMonStatusMonitor::Replaying(const cControl *pControl, const char *szName, const char *szFileName, bool bOn)
{
#ifdef MOREDEBUGMSG
  dsyslog("iMonLCD: Replaying  %s", szName);
#endif
  m_pDev->Replaying(pControl,szName,szFileName,bOn);
}

void ciMonStatusMonitor::OsdClear(void)
{
#ifdef MOREDEBUGMSG
  dsyslog("iMonLCD: OsdClear");
#endif
  m_pDev->OsdClear();
}

void ciMonStatusMonitor::OsdTitle(const char *Title)
{
#ifdef MOREDEBUGMSG
  dsyslog("iMonLCD: OsdTitle '%s'", Title);
#endif
  m_pDev->OsdTitle(Title);
}

void ciMonStatusMonitor::OsdStatusMessage(const char *szMessage)
{
#ifdef MOREDEBUGMSG
  dsyslog("iMonLCD: OsdStatusMessage '%s'", szMessage ? szMessage : "NULL");
#endif
  m_pDev->OsdStatusMessage(szMessage);
}

void ciMonStatusMonitor::OsdHelpKeys(const char *Red, const char *Green, const char *Yellow, const char *Blue)
{
#ifdef unusedMOREDEBUGMSG
  dsyslog("iMonLCD: OsdHelpKeys %s - %s - %s - %s", Red, Green, Yellow, Blue);
#endif
}

void ciMonStatusMonitor::OsdCurrentItem(const char *szText)
{
#ifdef MOREDEBUGMSG
  dsyslog("iMonLCD: OsdCurrentItem %s", szText);
#endif
  m_pDev->OsdCurrentItem(szText);
}

void ciMonStatusMonitor::OsdTextItem(const char *Text, bool Scroll)
{
#ifdef MOREDEBUGMSG
  dsyslog("iMonLCD: OsdTextItem %s %d", Text, Scroll);
#endif
}

void ciMonStatusMonitor::OsdChannel(const char *Text)
{
#ifdef MOREDEBUGMSG
  dsyslog("iMonLCD: OsdChannel %s", Text);
#endif
}

void ciMonStatusMonitor::OsdProgramme(time_t PresentTime, const char *PresentTitle, const char *PresentSubtitle, time_t FollowingTime, const char *FollowingTitle, const char *FollowingSubtitle)
{
#ifdef unusedMOREDEBUGMSG
  char buffer[25];
  struct tm tm_r;
  dsyslog("iMonLCD: OsdProgramme");
  strftime(buffer, sizeof(buffer), "%R", localtime_r(&PresentTime, &tm_r));
  dsyslog("%5s %s", buffer, PresentTitle);
  dsyslog("%5s %s", "", PresentSubtitle);
  strftime(buffer, sizeof(buffer), "%R", localtime_r(&FollowingTime, &tm_r));
  dsyslog("%5s %s", buffer, FollowingTitle);
  dsyslog("%5s %s", "", FollowingSubtitle);
#endif
}

