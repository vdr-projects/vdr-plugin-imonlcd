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

#ifndef __IMON_WATCH_H
#define __IMON_WATCH_H

#include <vdr/thread.h>
#include <vdr/status.h>
#include "imon.h"

enum eWatchMode {
    eUndefined,
    eLiveTV,
    eReplayNormal,
    eReplayMusic,
    eReplayDVD,
    eReplayFile,
    eReplayImage,
    eReplayAudioCD
};

enum eReplayState {
    eReplayNone,
  	eReplayPlay,
  	eReplayPaused,
  	eReplayForward1,
  	eReplayForward2,
  	eReplayForward3,
  	eReplayBackward1,
  	eReplayBackward2,
  	eReplayBackward3
};

enum eReplayMode {
    eReplayModeNormal,
  	eReplayModeShuffle,
  	eReplayModeRepeat,
  	eReplayModeRepeatShuffle,
};

enum eVideoMode {
    eVideoNone,
    eVideoMPG,
    eVideoDivX,
    eVideoXviD,
    eVideoWMV
};

enum eAudioMode {
    eAudioNone  = 0,
    eAudioMPG   = 1 << 0,
    eAudioAC3   = 1 << 1,
    eAudioDTS   = 1 << 2,
    eAudioWMA   = 1 << 3,
    eAudioMP3   = 1 << 4,
    eAudioOGG   = 1 << 5,
    eAudioWAV   = 1 << 6
};

enum eIconState { 
  eIconStateQuery, 
  eIconStateOn, 
  eIconStateOff, 
  eIconStateAuto 
};


class ciMonWatch
 : public  ciMonLCD
 , protected cThread {
private:
  cMutex mutex;

  volatile bool m_bShutdown;

  eWatchMode m_eWatchMode;
  eVideoMode m_eVideoMode;
  int        m_eAudioMode;

  int   m_nScrollOffset;
  bool  m_bScrollBackward;
  bool  m_bUpdateScreen;

  int   m_nCardIsRecording[16];

  unsigned int m_nIconsForceOn;
  unsigned int m_nIconsForceOff;
  unsigned int m_nIconsForceMask;

  const cControl *m_pControl;

  tChannelID  chID;
  tEventID    chEventID;
  time_t      chPresentTime;
  time_t      chFollowingTime;
  cString*    chName;
  cString*    chPresentTitle;
  cString*    chPresentShortTitle;

  int   m_nLastVolume;
  bool  m_bVolumeMute;

  cString*    osdItem;
  cString*    osdMessage;

  eReplayMode m_eReplayMode;
  cString* replayTitle;
  cString* replayTitleLast;

  time_t   tsCurrentLast;
  cString* currentTime;
protected:
  virtual void Action(void);
  bool Program();
  bool Replay();
  bool RenderScreen();
  eReplayState ReplayMode() const;
  bool ReplayPosition(int &current, int &total) const;
  bool CurrentTime();
public:
  ciMonWatch();
  virtual ~ciMonWatch();

  virtual int open(const char* szDevice, eProtocol pro);
  virtual void close ();

  void Replaying(const cControl *pControl, const char *szName, const char *szFileName, bool bOn);
  void Recording(const cDevice *pDevice, const char *szName, const char *szFileName, bool bOn);
  void Channel(int nChannelNumber);
  bool Volume(int nVolume, bool bAbsolute);

  void OsdClear();
  void OsdCurrentItem(const char *sz);
  void OsdStatusMessage(const char *sz);

  virtual bool SetFont(const char *szFont, int bTwoLineMode);

  eIconState ForceIcon(unsigned int nIcon, eIconState nState);
};

#endif

