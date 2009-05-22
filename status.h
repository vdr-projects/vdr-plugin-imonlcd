/*
 * iMON LCD plugin to VDR (C++)
 *
 * (C) 2009 Andreas Brachold <vdr07 AT deltab de>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#ifndef __IMON_STATUS__H
#define __IMON_STATUS__H

#include "imon.h"
#include <vdr/status.h>

class ciMonWatch;

class ciMonStatusMonitor : public cStatus {
  ciMonWatch*    m_pDev;
 public: 
  ciMonStatusMonitor(ciMonWatch*    pDev);
 protected:
  virtual void ChannelSwitch(const cDevice *Device, int ChannelNumber);
  virtual void Recording(const cDevice *pDevice, const char *szName, const char *szFileName, bool bOn);
  virtual void Replaying(const cControl *pControl, const char *szName, const char *szFileName, bool bOn);
  virtual void SetVolume(int Volume, bool Absolute);
  virtual void OsdClear(void);
  virtual void OsdTitle(const char *Title);
  virtual void OsdStatusMessage(const char *Message);
  virtual void OsdHelpKeys(const char *Red, const char *Green, const char *Yellow, const char *Blue);
  virtual void OsdCurrentItem(const char *Text);
  virtual void OsdTextItem(const char *Text, bool Scroll);
  virtual void OsdChannel(const char *Text);
  virtual void OsdProgramme(time_t PresentTime, const char *PresentTitle, const char *PresentSubtitle, time_t FollowingTime, const char *FollowingTitle, const char *FollowingSubtitle);
};

#endif
