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

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "watch.h"
#include "setup.h"

#include <vdr/tools.h>

struct cMutexLooker {
  cMutex& mutex;
  cMutexLooker(cMutex& m):
  mutex(m){
    mutex.Lock();
  }
  virtual ~cMutexLooker() {
    mutex.Unlock();
  }
};


ciMonWatch::ciMonWatch()
: cThread("iMonLCD: watch thread")
, m_bShutdown(false)
{
  m_nIconsForceOn = 0;
  m_nIconsForceOff = 0;
  m_nIconsForceMask = 0;

  unsigned int n;
  for(n=0;n<memberof(m_nCardIsRecording);++n) {
      m_nCardIsRecording[n] = 0;  
  }
  chPresentTime = 0;
  chFollowingTime = 0;
  chName = NULL;
  chPresentTitle = NULL;
  chPresentShortTitle = NULL;

  m_nLastVolume = cDevice::CurrentVolume();
  m_bVolumeMute = false;
  
  osdItem = NULL;
  osdMessage = NULL;

  m_pControl = NULL;
  replayTitle = NULL;
  replayShortTitle = NULL;
  replayTitleLast = NULL;
  m_eWatchMode = eLiveTV;
  m_eVideoMode = eVideoNone;
  m_eAudioMode = eAudioNone;

  m_nScrollOffset = -1;
  m_bScrollBackward = false;
}

ciMonWatch::~ciMonWatch()
{
  close();
  if(chName) { 
      delete chName;
      chName = NULL;
  }
  if(chPresentTitle) { 
      delete chPresentTitle;
      chPresentTitle = NULL;
  }
  if(chPresentShortTitle) { 
      delete chPresentShortTitle;
      chPresentShortTitle = NULL;
  }
  if(osdMessage) { 
      delete osdMessage;
      osdMessage = NULL;
  }
  if(osdItem) { 
      delete osdItem;
      osdItem = NULL;
  }
  if(replayTitle) {
    delete replayTitle;
    replayTitle = NULL;
  }
  if(replayShortTitle) {
    delete replayShortTitle;
    replayShortTitle = NULL;
  }
}

int ciMonWatch::open(const char* szDevice, eProtocol pro) {
    int iRet = ciMonLCD::open(szDevice,pro);
    if(0==iRet) {
        m_bShutdown = false;
        m_bUpdateScreen = true;
        Start();
    }
    return iRet;
}

void ciMonWatch::close() {

  if(Running()) {
    m_bShutdown = true;
    usleep(500000);
    Cancel();
  }

  cTimer* t = Timers.GetNextActiveTimer();    

  switch(theSetup.m_nOnExit) {
    case eOnExitMode_NEXTTIMER: {
      isyslog("iMonLCD: closing, show only next timer.");
      this->setLineLength(0,0,0,0);

      this->clear();
      if(t) {
        struct tm l;
        cString topic;
        time_t tn = time(NULL);
        time_t tt = t->StartTime();
        localtime_r(&tt, &l);
        if((tt - tn) > 86400) {
          // next timer more then 24h 
          topic = cString::sprintf("%d. %02d:%02d %s", l.tm_mday, l.tm_hour, l.tm_min, t->File());
        } else {
          // next timer (today)
          topic = cString::sprintf("%02d:%02d %s", l.tm_hour, l.tm_min, t->File());
        }
        this->DrawText(0,0,topic);
        this->icons(eIconTime);
      } else {
        this->DrawText(0,0,tr("None active timer"));
        this->icons(0);
      }
      this->flush();
      break;
    }
    case eOnExitMode_SHOWMSG: {
      isyslog("iMonLCD: closing, leaving \"last\" message.");
      break;
    } 
    case eOnExitMode_BLANKSCREEN: {
      isyslog("iMonLCD: closing, turning backlight off.");
      SendCmdShutdown();
      break;
    } 
    case eOnExitMode_WAKEUP: {
      isyslog("iMonLCD: closing, set wakeup time and showing clock.");
      SendCmdClock(t ? t->StartTime() - (theSetup.m_nWakeup * 60) : NULL);
      break;
    } 
    default:
    case eOnExitMode_SHOWCLOCK: {
      isyslog("iMonLCD: closing, showing clock.");
      SendCmdClock(NULL);
      break;
    } 
  }


  ciMonLCD::close();
}

void ciMonWatch::Action(void)
{
  unsigned int nLastIcons = -1;
  int nContrast = -1;

  unsigned int n;
  int nCnt = 0;
  int nLastTopProgressBar = -1;
  int nTopProgressBar = 0;
  int nLastBottomProgressBar = -1;
  int nBottomProgressBar = 0;
  eTrackType eAudioTrackType = ttNone;
  int nAudioChannel = 0;

  cTimeMs runTime;

  for (;!m_bShutdown;++nCnt) {
    
    LOCK_THREAD;

    unsigned int nIcons = 0;
    bool bUpdateIcons = false;
    bool bFlush = false;
    if(m_bShutdown)
      break;
    else {
      cMutexLooker m(mutex);

      switch(m_eWatchMode) {
        default:
        case eLiveTV:         nIcons |= eIconTV;break;
        case eReplayNormal:   nIcons |= eIconDiscSpin | eIconDiscEllispe | eIconTopMovie; break;
        case eReplayMusic:    nIcons |= eIconDiscSpin | eIconDiscEllispe | eIconTopMusic; break;
        case eReplayDVD:      nIcons |= eIconDiscSpin | eIconDiscEllispe | eIconTopDVD;   break;
        case eReplayFile:     nIcons |= eIconDiscSpin | eIconDiscEllispe | eIconTopWeb;   break;
        case eReplayImage:    nIcons |= eIconDiscSpin | eIconDiscEllispe | eIconTopPhoto; break;
        case eReplayAudioCD:  nIcons |= eIconDiscSpin | eIconDiscEllispe | eIconTopDVD;   break;
      }

      bFlush = RenderScreen();
      if(m_eWatchMode == eLiveTV) {
          if((chFollowingTime - chPresentTime) > 0) {
            nBottomProgressBar = (time(NULL) - chPresentTime) * 32 / (chFollowingTime - chPresentTime);
            if(nBottomProgressBar > 32) nBottomProgressBar = 32;
            if(nBottomProgressBar < 0)  nBottomProgressBar = 0;
          } else {
            nBottomProgressBar = 0;
          }
      } else {
        int current = 0,total = 0;
        if(ReplayPosition(current,total) && total) {
            nBottomProgressBar = current * 32 / total;
            if(nBottomProgressBar > 32) nBottomProgressBar = 32;
            if(nBottomProgressBar < 0)  nBottomProgressBar = 0;

            switch(ReplayMode()) {
                case eReplayNone:
                case eReplayPaused:
                  bUpdateIcons = false;
                  break;
                default:
                case eReplayPlay:
                  bUpdateIcons = (0 == (nCnt % 4));
                  nIcons |= eIconDiscRunSpin;
                  break;
                case eReplayBackward1:
                  nIcons |= eIconDiscSpinBackward;
                case eReplayForward1:
                  break;
                  bUpdateIcons = (0 == (nCnt % 3));
                  nIcons |= eIconDiscRunSpin;
                case eReplayBackward2:
                  nIcons |= eIconDiscSpinBackward;
                case eReplayForward2:
                  bUpdateIcons = (0 == (nCnt % 2));
                  nIcons |= eIconDiscRunSpin;
                  break;
                case eReplayBackward3:
                  nIcons |= eIconDiscSpinBackward;
                case eReplayForward3:
                  bUpdateIcons = true;
                  nIcons |= eIconDiscRunSpin;
                  break;
            }
            switch(m_eReplayMode) {
              case eReplayModeShuffle:       nIcons |= eIconShuffle;  break;
              case eReplayModeRepeat:        nIcons |= eIconRepeat;  break;
              case eReplayModeRepeatShuffle: nIcons |= eIconShuffle | eIconRepeat;  break;
              default: break;
            }

        } else {
            nBottomProgressBar = 0;
        }
      }

      switch(m_eVideoMode) {
        case eVideoMPG:   nIcons |= eIconBL_MPG;  break;
        case eVideoDivX:  nIcons |= eIconBL_DIVX;  break;
        case eVideoXviD:  nIcons |= eIconBL_XVID;  break;
        case eVideoWMV:   nIcons |= eIconBL_WMV;  break;
        default:
        case eVideoNone:  break;
      }

      if(m_eAudioMode & eAudioMPG) nIcons |= eIconBM_MPG;
      if(m_eAudioMode & eAudioAC3) nIcons |= eIconBM_AC3;
      if(m_eAudioMode & eAudioDTS) nIcons |= eIconBM_DTS;
      if(m_eAudioMode & eAudioWMA) nIcons |= eIconBR_WMA;
      if(m_eAudioMode & eAudioMP3) nIcons |= eIconBR_MP3;
      if(m_eAudioMode & eAudioOGG) nIcons |= eIconBR_OGG;
      if(m_eAudioMode & eAudioWAV) nIcons |= eIconBR_WAV;

      for(n=0;n<memberof(m_nCardIsRecording);++n) {
          if(0 != m_nCardIsRecording[n]) {
            nIcons |= eIconRecording;
            switch(n) {
              case 0: nIcons |= eIconSRC;  break;
              case 1: nIcons |= eIconSRC1; break;
              case 2: nIcons |= eIconSRC2; break;
            }
          }
      }

      if(m_bVolumeMute) {
        nIcons |= eIconVolume;
      } else {
          if(eAudioTrackType == ttNone || (0 == nCnt % 10)) {
            eAudioTrackType = cDevice::PrimaryDevice()->GetCurrentAudioTrack(); //Stereo/Dolby
            nAudioChannel   = cDevice::PrimaryDevice()->GetAudioChannel();      //0-Stereo,1-Left, 2-Right
          }
          switch(eAudioTrackType) {
            default:
              break;
            case ttAudioFirst ... ttAudioLast: {
              switch(nAudioChannel) {
                case 1:  nIcons |= eIconSpeakerL;  break;
                case 2:  nIcons |= eIconSpeakerR;  break;
                case 0:  
                default: nIcons |= eIconSpeakerLR; break;
                }
            break;
            }
            case ttDolbyFirst ... ttDolbyLast: 
              nIcons |= eIconSpeaker51;
            break;
          }
      }
    }

    runTime.Set();
    if(theSetup.m_nContrast != nContrast) {
      nContrast = theSetup.m_nContrast;
      Contrast(nContrast);
    }

    //Force icon state (defined by svdrp)
    nIcons &= ~(m_nIconsForceMask);
    nIcons |=  (m_nIconsForceOn);
    nIcons &= ~(m_nIconsForceOff);
    if(m_nIconsForceOn & eIconDiscRunSpin) {
      bUpdateIcons |= (0 == (nCnt % 4));
      nIcons &= ~(eIconDiscSpinBackward);
    }

    if(bUpdateIcons || nIcons != nLastIcons) {
      icons(nIcons);
      nLastIcons = nIcons;
    }
    if(nTopProgressBar != nLastTopProgressBar
       || nBottomProgressBar != nLastBottomProgressBar ) {

       setLineLength(nTopProgressBar, nBottomProgressBar, nTopProgressBar, nBottomProgressBar);
       nLastTopProgressBar = nTopProgressBar;
       nLastBottomProgressBar = nBottomProgressBar;

    }
    if(bFlush) {
      flush();
    }
    int nDelay = 100 - runTime.Elapsed();
    if(nDelay <= 10) {
      nDelay = 10;
    }
    cCondWait::SleepMs(nDelay);
  }
  dsyslog("iMonLCD: watch thread closed (pid=%d)", getpid());
}

bool ciMonWatch::RenderScreen() {
    cString* scRender;
    bool bForce = m_bUpdateScreen;
    if(osdMessage) {
      scRender = osdMessage;
    } else if(osdItem) {
      scRender = osdItem;
    } else if(m_eWatchMode == eLiveTV) {
        if(Program()) {
          bForce = true;
        }
        if(chPresentTitle)
          scRender = chPresentTitle;
        else
          scRender = chName;
    } else {
        if(Replay()) {
          bForce = true;
        }
        scRender = replayTitle;
    }
    if(bForce) {
      m_nScrollOffset = 0;
      m_bScrollBackward = false;
    }
    if(bForce || m_nScrollOffset > 0 || m_bScrollBackward) {
      this->clear();
      if(scRender) {
        int iRet = this->DrawText(0 - m_nScrollOffset,0,(*scRender));
        switch(iRet) {
          case 0: 
            if(m_nScrollOffset <= 0) {
              m_nScrollOffset = 0;
              m_bScrollBackward = false;
              break; //Fit to screen
            }
            m_bScrollBackward = true;
          case 2:
          case 1:
            if(m_bScrollBackward) m_nScrollOffset -= 2;
            else                  m_nScrollOffset += 2;
            if(m_nScrollOffset >= 0)
              break;
          case -1:
            m_nScrollOffset = 0;
            m_bScrollBackward = false;
            break;
        }
      }

      m_bUpdateScreen = false;
      return true;
    }
    return false;
}

bool ciMonWatch::Replay() {
  
  if(replayTitleLast != replayTitle) {
    replayTitleLast = replayTitle;
    return true;
  }
  return false;
}

void ciMonWatch::Replaying(const cControl * Control, const char * Name, const char *FileName, bool On)
{
    cMutexLooker m(mutex);
    m_bUpdateScreen = true;
    if (On)
    {
        m_eVideoMode  = eVideoMPG;
        m_eAudioMode  = eAudioMPG;

        m_pControl = (cControl *) Control;
        m_eWatchMode = eReplayNormal;
        if(replayTitle) {
          delete replayTitle;
          replayTitle = NULL;
        }
        m_eReplayMode = eReplayModeNormal;
        if (Name && !isempty(Name))
        {
            int slen = strlen(Name);
            bool bFound = false;
            ///////////////////////////////////////////////////////////////////////
            //Looking for mp3/muggle-plugin replay : [LS] (444/666) title
            //
            if (slen > 6 &&
                *(Name+0)=='[' &&
                *(Name+3)==']' &&
                *(Name+5)=='(') {
                unsigned int i;
                for (i=6; *(Name + i) != '\0'; ++i) { //search for [xx] (xxxx) title
                    if (*(Name+i)==' ' && *(Name+i-1)==')') {
                        bFound = true;
                        break;
                    }
                }
                if (bFound) { //found mp3/muggle-plugin
                    // get loopmode
                    if ((*(Name+1) != '.') && (*(Name+2) != '.')) {
                                            m_eReplayMode = eReplayModeRepeatShuffle;
                    } else {
                      if (*(Name+1) != '.') m_eReplayMode = eReplayModeRepeat;
                      if (*(Name+2) != '.') m_eReplayMode = eReplayModeShuffle;
                    }
                    if (strlen(Name+i) > 0) { //if name isn't empty, then copy
                        replayTitle = new cString(skipspace(Name + i));
                    }
                    m_eWatchMode = eReplayMusic;
                    m_eVideoMode = eVideoNone;
                    m_eAudioMode = eAudioMP3;
                }
            }
            ///////////////////////////////////////////////////////////////////////
            //Looking for DVD-Plugin replay : 1/8 4/28,  de 2/5 ac3, no 0/7,  16:9, VOLUMENAME
            // cDvdPlayerControl::GetDisplayHeaderLine
            // titleinfo, audiolang, spulang, aspect, title
            if (!bFound && slen>7)
            {
                unsigned int i,n;
                for (n=0,i=0;*(Name+i) != '\0';++i)
                { //search volumelabel after 4*", " => xxx, xxx, xxx, xxx, title
                    if (*(Name+i)==' ' && *(Name+i-1)==',') {
                        if (++n == 4) {
                            bFound = true;
                            break;
                        }
                    }
                }
                if (bFound) //found DVD replay
                {
                    if (strlen(Name+i) > 0)
                    { // if name isn't empty, then copy
                        replayTitle = new cString(skipspace(Name + i));
                    }
                    m_eWatchMode = eReplayDVD;
                    m_eVideoMode = eVideoMPG;
                    m_eAudioMode = eAudioMPG;
                }
            }
            if (!bFound) {
                int i;
                for (i=slen-1;i>0;--i)
                {   //search reverse last Subtitle
                    // - filename contains '~' => subdirectory
                    // or filename contains '/' => subdirectory
                    switch (*(Name+i)) {
                        case '/': {
                            // look for file extentsion like .xxx or .xxxx
                            if (slen>5 && ((*(Name+slen-4) == '.') || (*(Name+slen-5) == '.')))
                            {
                                m_eWatchMode = eReplayFile;
                            }
                            else
                            {
                                break;
                            }
                        }
                        case '~': {
                            replayTitle = new cString(Name + i + 1);
                            bFound = true;
                            i = 0;
                        }
                        default:
                            break;
                    }
                }
            }

            if (0 == strncmp(Name,"[image] ",8)) {
                if (m_eWatchMode != eReplayFile) //if'nt already Name stripped-down as filename
                    replayTitle = new cString(Name + 8);
                m_eWatchMode = eReplayImage;
                m_eVideoMode = eVideoMPG;
                m_eAudioMode = eAudioNone;
                bFound = true;
            }
            else if (0 == strncmp(Name,"[audiocd] ",10)) {
                replayTitle = new cString(Name + 10);
                m_eWatchMode = eReplayAudioCD;
                m_eVideoMode = eVideoNone;
                m_eAudioMode = eAudioWAV;
                bFound = true;
            }
            if (!bFound) {
                replayTitle = new cString(Name);
            }
        }
        if (!replayTitle) {
            replayTitle = new cString(tr("Unknown title"));
        }
    }
    else
    {
      m_eWatchMode = eLiveTV;
      m_pControl = NULL;
    }
}


eReplayState ciMonWatch::ReplayMode() const
{
  bool Play = false, Forward = false;
  int Speed = -1;
	if (m_pControl
        && ((cControl *)m_pControl)->GetReplayMode(Play,Forward,Speed))
  {
    // 'Play' tells whether we are playing or pausing, 'Forward' tells whether
    // we are going forward or backward and 'Speed' is -1 if this is normal
    // play/pause mode, 0 if it is single speed fast/slow forward/back mode
    // and >0 if this is multi speed mode.
    switch(Speed) {
      default:
      case -1: 
        return Play ? eReplayPlay : eReplayPaused;
      case 0: 
      case 1: 
        return Forward ? eReplayForward1 : eReplayBackward1;
      case 2: 
        return Forward ? eReplayForward2 : eReplayBackward2;
      case 3: 
        return Forward ? eReplayForward3 : eReplayBackward3;
    }
  }
  return eReplayNone;
}

bool ciMonWatch::ReplayPosition(int &current, int &total) const
{
  if (m_pControl && ((cControl *)m_pControl)->GetIndex(current, total, false)) {
    total = (total == 0) ? 1 : total;
    return true;
  }
  return false;
}

void ciMonWatch::Recording(const cDevice *pDevice, const char *szName, const char *szFileName, bool bOn)
{
  cMutexLooker m(mutex);

  unsigned int nCardIndex = pDevice->CardIndex();
  if (nCardIndex > memberof(m_nCardIsRecording) - 1 )
    nCardIndex = memberof(m_nCardIsRecording)-1;

  if (nCardIndex < memberof(m_nCardIsRecording)) {
    if (bOn) {
      ++m_nCardIsRecording[nCardIndex];
    }
    else {
      if (m_nCardIsRecording[nCardIndex] > 0)
        --m_nCardIsRecording[nCardIndex];
    }
  }
  else {
    esyslog("iMonLCD: Recording: only up to %d devices are supported by this plugin", memberof(m_nCardIsRecording));
  }
}

void ciMonWatch::Channel(int ChannelNumber)
{
    cMutexLooker m(mutex);
    if(chPresentTitle) { 
        delete chPresentTitle;
        chPresentTitle = NULL;
    }
    if(chPresentShortTitle) { 
        delete chPresentShortTitle;
        chPresentShortTitle = NULL;
    }
    if(chName) { 
        delete chName;
        chName = NULL;
    }

    m_eVideoMode = eVideoNone;
    m_eAudioMode = eAudioNone;

    cChannel * ch = Channels.GetByNumber(ChannelNumber);
    if(ch) {
      chID = ch->GetChannelID();
      chPresentTime = 0;
      chFollowingTime = 0;
      if (!isempty(ch->Name())) {
          chName = new cString(ch->Name());
      }
      if(ch->Vpid())  m_eVideoMode  = eVideoMPG;
      if(ch->Apid(0)) m_eAudioMode |= eAudioMPG;
      if(ch->Dpid(0)) m_eAudioMode |= eAudioAC3;
    }
    m_eWatchMode = eLiveTV;
    m_bUpdateScreen = true;
    m_nScrollOffset = 0;
    m_bScrollBackward = false;
}

bool ciMonWatch::Program() {

    bool bChanged = false;
    const cEvent * p = NULL;
    cSchedulesLock schedulesLock;
    const cSchedules * schedules = cSchedules::Schedules(schedulesLock);
    if (chID.Valid() && schedules)
    {
        const cSchedule * schedule = schedules->GetSchedule(chID);
        if (schedule)
        {
            if ((p = schedule->GetPresentEvent()) != NULL)
            {
                if(chPresentTime && chEventID == p->EventID()) {
                  return false;
                }

                bChanged  = true;
                chEventID = p->EventID();
                chPresentTime = p->StartTime();
                chFollowingTime = p->EndTime();

                if(chPresentTitle) { 
                    delete chPresentTitle;
                    chPresentTitle = NULL;
                }
                if (!isempty(p->Title())) {
                    chPresentTitle = new cString(p->Title());
                }

                if(chPresentShortTitle) { 
                    delete chPresentShortTitle;
                    chPresentShortTitle = NULL;
                }
                if (!isempty(p->ShortText())) {
                    chPresentShortTitle = new cString(p->ShortText());
                }
            }
        }
    }
    return bChanged;
}


bool ciMonWatch::Volume(int nVolume, bool bAbsolute)
{
  cMutexLooker m(mutex);

  int nAbsVolume;

  nAbsVolume = m_nLastVolume;
  if (bAbsolute) {
      nAbsVolume=100*nVolume/255;
  } else {
      nAbsVolume=nAbsVolume+100*nVolume/255;
  }

  bool bStateIsChanged = false;
  if(m_nLastVolume > 0 && 0 == nAbsVolume) {
    m_bVolumeMute = true;
    bStateIsChanged = true;
  }
  else if(0 == m_nLastVolume && nAbsVolume > 0) {
    m_bVolumeMute = false;
    bStateIsChanged = true;
  }
  m_nLastVolume = nAbsVolume;

  return bStateIsChanged;
}


void ciMonWatch::OsdClear() {
    cMutexLooker m(mutex);
    if(osdMessage) { 
        delete osdMessage;
        osdMessage = NULL;
        m_bUpdateScreen = true;
    }
    if(osdItem) { 
        delete osdItem;
        osdItem = NULL;
        m_bUpdateScreen = true;
    }
}

void ciMonWatch::OsdCurrentItem(const char *sz)
{
    char *s = NULL;
    char *sc = NULL;
    if(sz && !isempty(sz)) {
        s = strdup(sz);
        sc = compactspace(strreplace(s,'\t',' '));
    }
    if(sc 
        && osdItem 
        && 0 == strcmp(sc, *osdItem)) {
      if(s) {
        free(s);
      }
      return;
    }
    cMutexLooker m(mutex);
    if(osdItem) { 
        delete osdItem;
        osdItem = NULL;
        m_bUpdateScreen = true;
    }
    if(sc) {
          osdItem = new cString(sc);
          m_bUpdateScreen = true;
    }
    if(s) {
      free(s);
    }
}

void ciMonWatch::OsdStatusMessage(const char *sz)
{
    char *s = NULL;
    char *sc = NULL;
    if(sz && !isempty(sz)) {
        s = strdup(sz);
        sc = compactspace(strreplace(s,'\t',' '));
    }
    if(sc 
        && osdMessage 
        && 0 == strcmp(sc, *osdMessage)) {
      if(s) {
        free(s);
      }
      return;
    }
    cMutexLooker m(mutex);
    if(osdMessage) { 
        delete osdMessage;
        osdMessage = NULL;
        m_bUpdateScreen = true;
    }
    if(sc) {
          osdMessage = new cString(sc);
          m_bUpdateScreen = true;
    }
    if(s) {
      free(s);
    }
}

bool ciMonWatch::SetFont(const char *szFont) {
    cMutexLooker m(mutex);
    if(ciMonLCD::SetFont(szFont)) {
      m_bUpdateScreen = true;
      return true;
    }
    return false;
}

eIconState ciMonWatch::ForceIcon(unsigned int nIcon, eIconState nState) {

  unsigned int nIconOff = nIcon;
  if(nIconOff & eIconTopMask)
    nIconOff |= eIconTopMask;
  else if(nIconOff & eIconSpeakerMask)
    nIconOff |= eIconSpeakerMask;
  else if(nIconOff & eIconBR_Mask)
    nIconOff |= eIconBR_Mask;
  else if(nIconOff & eIconBM_Mask)
    nIconOff |= eIconBM_Mask;
  else if(nIconOff & eIconBL_Mask)
    nIconOff |= eIconBL_Mask;
  
  switch(nState) {
    case eIconStateAuto:
      m_nIconsForceOn   &= ~(nIconOff);
      m_nIconsForceOff  &= ~(nIconOff);
      m_nIconsForceMask &= ~(nIconOff);
      break;
    case eIconStateOn:
      m_nIconsForceOn   |=   nIcon;
      m_nIconsForceOff  &= ~(nIconOff);
      m_nIconsForceMask |=   nIconOff;
      break;
    case eIconStateOff:
      m_nIconsForceOff  |=   nIcon;
      m_nIconsForceOn   &= ~(nIconOff);
      m_nIconsForceMask |=   nIconOff;
      break;
    default:
      break;
  }
  if(m_nIconsForceOn  & nIcon) return eIconStateOn;
  if(m_nIconsForceOff & nIcon) return eIconStateOff;
  return eIconStateAuto;
}

