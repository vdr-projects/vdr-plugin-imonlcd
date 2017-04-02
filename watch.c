/*
 * iMON LCD plugin for VDR (C++)
 *
 * (C) 2009-2012 Andreas Brachold <vdr07 AT deltab de>
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
#include <ctype.h>

#include "watch.h"
#include "setup.h"
#include "ffont.h"

#include <vdr/tools.h>
#include <vdr/shutdown.h>

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

  osdTitle = NULL;  
  osdItem = NULL;
  osdMessage = NULL;

  m_pControl = NULL;
  replayTitle = NULL;
  replayTitleLast = NULL;
  replayTime = NULL;

  currentTime = NULL;

  m_eWatchMode = eLiveTV;
  m_eVideoMode = eVideoNone;
  m_eAudioMode = eAudioNone;

  m_nScrollOffset = -1;
  m_bScrollBackward = false;
  m_bScrollNeeded = false;
}

ciMonWatch::~ciMonWatch()
{
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
  if(osdTitle) { 
      delete osdTitle;
      osdTitle = NULL;
  }
  if(osdItem) { 
      delete osdItem;
      osdItem = NULL;
  }
  if(replayTitle) {
    delete replayTitle;
    replayTitle = NULL;
  }
  if(replayTitleLast) {
    delete replayTitleLast;
    replayTitleLast = NULL;
  }
  if(replayTime) {
    delete replayTime;
    replayTime = NULL;
  }
  if(currentTime) {
    delete currentTime;
    currentTime = NULL;
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

void ciMonWatch::shutdown(int nExitMode) {

  if(Running()) {
    m_bShutdown = true;
    usleep(500000);
    Cancel();
  }

  if(this->isopen()) {
    const cTimer* t = NULL;
#if APIVERSNUM >= 20302
    cStateKey lock;
    if (const cTimers *Timers = cTimers::GetTimersRead(lock)) {
       t = Timers->GetNextActiveTimer();
       lock.Remove();
    }
#else
    t = Timers.GetNextActiveTimer();
#endif
    switch(nExitMode) {
      case eOnExitMode_NEXTTIMER: {
        isyslog("iMonLCD: closing, show only next timer.");
        this->setLineLength(0,0,0,0);

        int nTop = (theSetup.m_nHeight - pFont->Height())/2;
        this->clear();
        if(t) {
          struct tm l;
          cString topic;
          time_t tn = time(NULL);
          time_t tt = t->StartTime();
          localtime_r(&tt, &l);
          if((tt - tn) > 86400) {
            // next timer more then 24h 
            topic = cString::sprintf("%d. %02d:%02d", l.tm_mday, l.tm_hour, l.tm_min);
          } else {
            // next timer (today)
            topic = cString::sprintf("%02d:%02d", l.tm_hour, l.tm_min);
          }

          int w = pFont->Width(topic);
          if(theSetup.m_nRenderMode == eRenderMode_DualLine) {
            this->DrawText(0,0,topic);
            if((w + 3) < theSetup.m_nWidth)
                this->DrawText(w + 3,0,t->Channel()->Name());
            this->DrawText(0,pFont->Height(), t->File());
          } else {
            this->DrawText(0,nTop<0?0:nTop, topic);
            if((w + 3) < theSetup.m_nWidth)
                this->DrawText(w + 3,nTop<0?0:nTop, t->File());
          }
          this->icons(eIconTime);
        } else {
          this->DrawText(0,nTop<0?0:nTop,tr("None active timer"));
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
        SendCmdClock(t ? t->StartTime() - (theSetup.m_nWakeup * 60) : 0);
        break;
      } 
      default:
      case eOnExitMode_SHOWCLOCK: {
        isyslog("iMonLCD: closing, showing clock.");
        SendCmdClock(0);
        break;
      } 
    }
  }
  ciMonLCD::close();
}

void ciMonWatch::Action(void)
{
  unsigned int nLastIcons = -1;
  int nContrast = -1;

  unsigned int n;
  unsigned int nCnt = 0;
  int nLastTopProgressBar = -1;
  int nTopProgressBar = 0;
  int nLastBottomProgressBar = -1;
  int nBottomProgressBar = 0;
  eTrackType eAudioTrackType = ttNone;
  int nAudioChannel = 0;
  int current = 0;
  int total = 0;

  cTimeMs runTime;
  struct tm tm_r;
  bool bLastSuspend = false;

  for (;!m_bShutdown;++nCnt) {
    
    LOCK_THREAD;

    unsigned int nIcons = 0;
    bool bUpdateIcons = false;
    bool bFlush = false;
    bool bReDraw = false;
    bool bSuspend = false;

    if(m_bShutdown)
      break;
    else {
      cMutexLooker m(mutex);
      runTime.Set();

      time_t ts = time(NULL);
      if(theSetup.m_nSuspendMode != eSuspendMode_Never 
          && theSetup.m_nSuspendTimeOff != theSetup.m_nSuspendTimeOn) {
        struct tm *now = localtime_r(&ts, &tm_r);
        int clock = now->tm_hour * 100 + now->tm_min;
        if(theSetup.m_nSuspendTimeOff > theSetup.m_nSuspendTimeOn) { //like 8-20
          bSuspend = (clock >= theSetup.m_nSuspendTimeOn) 
                  && (clock <= theSetup.m_nSuspendTimeOff);
        } else { //like 0-8 and 20..24
          bSuspend = (clock >= theSetup.m_nSuspendTimeOn) 
                  || (clock <= theSetup.m_nSuspendTimeOff);
        }
        if(theSetup.m_nSuspendMode == eSuspendMode_Timed 
              && !ShutdownHandler.IsUserInactive()) {
          bSuspend = false;
        }
      }
      if(bSuspend != bLastSuspend) {
        if(bSuspend) {
          SendCmdShutdown();
        } else {
          SendCmdInit();
          nLastIcons = -1;
          nContrast = -1;
          nLastTopProgressBar = -1;
          nLastBottomProgressBar = -1;
        }
        bFlush = true;
        bReDraw = true;
        bLastSuspend = bSuspend;
      }

      if(!bSuspend) {
        // every second the clock need updates.
        if((0 == (nCnt % 5))) {
           if (theSetup.m_nRenderMode == eRenderMode_DualLine) {
            bReDraw |= CurrentTime();
          }
          if(m_eWatchMode != eLiveTV) {
            current = 0;
            total = 0;
            bReDraw |= ReplayTime(current,total);
          }
        }

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

        bFlush = RenderScreen(bReDraw);
        if(m_eWatchMode == eLiveTV) {
            if((chFollowingTime - chPresentTime) > 0) {
              nBottomProgressBar = (time(NULL) - chPresentTime) * 32 / (chFollowingTime - chPresentTime);
              if(nBottomProgressBar > 32) nBottomProgressBar = 32;
              if(nBottomProgressBar < 0)  nBottomProgressBar = 0;
            } else {
              nBottomProgressBar = 0;
            }
        } else {
          if(total) {
              nBottomProgressBar = current * 32 / total;
              if(nBottomProgressBar > 32) nBottomProgressBar = 32;
              if(nBottomProgressBar < 0)  nBottomProgressBar = 0;
          } else {
              nBottomProgressBar = 0;
          }
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
    }

    if(bFlush) {
      flush();
    }
    int nDelay = (bSuspend ? 1000 : 100) - runTime.Elapsed();
    if(nDelay <= 10) {
      nDelay = 10;
    }
    cCondWait::SleepMs(nDelay);
  }
  dsyslog("iMonLCD: watch thread closed (pid=%d)", getpid());
}

bool ciMonWatch::RenderScreen(bool bReDraw) {
    cString* scRender;
    cString* scHeader = NULL;
    bool bForce = m_bUpdateScreen;
    bool bAllowCurrentTime = false;

    if(osdMessage) {
      scRender = osdMessage;
    } else if(osdItem) {
      scHeader = osdTitle;
      scRender = osdItem;
    } else if(m_eWatchMode == eLiveTV) {
        scHeader = chName;
        if(Program()) {
          bForce = true;
        }
        if(chPresentTitle && theSetup.m_nRenderMode != eRenderMode_SingleTopic) {
          scRender = chPresentTitle;
          bAllowCurrentTime = true;
        } else {
          scHeader = currentTime;
          scRender = chName;
        }
    } else {
        if(Replay()) {
          bForce = true;
        }
        scHeader = replayTime;
        scRender = replayTitle;
        bAllowCurrentTime = true;
    }


    if(bForce) {
      m_nScrollOffset = 0;
      m_bScrollBackward = false;
      m_bScrollNeeded = true;
    }
    if(bForce || bReDraw || m_nScrollOffset > 0 || m_bScrollBackward) {
      this->clear();
      if(scRender) {
    
        int iRet = -1;
        if(theSetup.m_nRenderMode == eRenderMode_DualLine) {
          iRet = this->DrawText(0 - m_nScrollOffset,pFont->Height(), *scRender);
        } else {
          int nTop = (theSetup.m_nHeight - pFont->Height())/2;
          iRet = this->DrawText(0 - m_nScrollOffset,nTop<0?0:nTop, *scRender);
        }
        if(m_bScrollNeeded) {
          switch(iRet) {
            case 0: 
              if(m_nScrollOffset <= 0) {
                m_nScrollOffset = 0;
                m_bScrollBackward = false;
                m_bScrollNeeded = false;
                break; //Fit to screen
              }
              m_bScrollBackward = true;
            case 2:
            case 1:
              if(m_bScrollBackward) m_nScrollOffset -= 2;
              else                  m_nScrollOffset += 2;
              if(m_nScrollOffset >= 0) {
                break;
              }
            case -1:
              m_nScrollOffset = 0;
              m_bScrollBackward = false;
              m_bScrollNeeded = false;
              break;
          }
        }
      }

      if(scHeader && theSetup.m_nRenderMode == eRenderMode_DualLine) {
        if(bAllowCurrentTime && currentTime) {
          int t = pFont->Width(*currentTime);
          int w = pFont->Width(*scHeader);
          if((w + t + 3) < theSetup.m_nWidth && t < theSetup.m_nWidth) {
            this->DrawText(theSetup.m_nWidth - t, 0, *currentTime);
          } 
        }
        this->DrawText(0, 0, *scHeader);
      }

      m_bUpdateScreen = false;
      return true;
    }
    return false;
}

bool ciMonWatch::CurrentTime() {
  time_t ts = time(NULL);

  if((ts / 60) != (tsCurrentLast / 60)) {

    if(currentTime)
      delete currentTime;

    tsCurrentLast = ts;
    currentTime = new cString(TimeString(ts));
    return currentTime != NULL;
  } 
  return false;
}

bool ciMonWatch::Replay() {
  
  if(!replayTitleLast 
      || !replayTitle 
      || strcmp(*replayTitleLast,*replayTitle)) {
    if(replayTitleLast) {
      delete replayTitleLast;
      replayTitleLast = NULL;
    }
    if(replayTitle) {
      replayTitleLast = new cString(*replayTitle);
    }
    return true;
  }
  return false;
}

char *striptitle(char *s)
{
  if (s && *s) {
     for (char *p = s + strlen(s) - 1; p >= s; p--) {
         if (isspace(*p) || *p == '~' || *p == '\\')
           *p = 0;
         else
            break;
         }
     }
  return s;
}

void ciMonWatch::Replaying(const cControl * Control, const char * szName, const char *FileName, bool On)
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
        if (szName && !isempty(szName))
        {
            char* Title = NULL;
            char* Name = strdup(skipspace(szName));
            striptitle(Name); // remove space at end
            int slen = strlen(Name);
            ///////////////////////////////////////////////////////////////////////
            //Looking for mp3/muggle-plugin replay : [LS] (444/666) title
            //
            if (slen > 6 &&
                *(Name+0)=='[' &&
                *(Name+3)==']' &&
                *(Name+5)=='(') {
                int i;
                for (i=6; *(Name + i) != '\0'; ++i) { //search for [xx] (xxxx) title
                    if (*(Name+i)==' ' && *(Name+i-1)==')') { //found mp3/muggle-plugin
                        // get loopmode
                        if ((*(Name+1) != '.') && (*(Name+2) != '.')) {
                                                m_eReplayMode = eReplayModeRepeatShuffle;
                        } else {
                          if (*(Name+1) != '.') m_eReplayMode = eReplayModeRepeat;
                          if (*(Name+2) != '.') m_eReplayMode = eReplayModeShuffle;
                        }
                        if (slen > i) { //if name isn't empty, then copy
                            Title = (Name + i);
                        }
                        m_eWatchMode = eReplayMusic;
                        m_eVideoMode = eVideoNone;
                        m_eAudioMode = eAudioMP3;
                        break;
                    }
                }
            }
            ///////////////////////////////////////////////////////////////////////
            //Looking for DVD-Plugin replay : 1/8 4/28,  de 2/5 ac3, no 0/7,  16:9, VOLUMENAME
            // cDvdPlayerControl::GetDisplayHeaderLine : titleinfo, audiolang, spulang, aspect, title
            if (!Title && slen>7)
            {
                int i,n;
                for (n=0,i=0;*(Name+i) != '\0';++i)
                { //search volumelabel after 4*", " => xxx, xxx, xxx, xxx, title
                    if (*(Name+i)==' ' && *(Name+i-1)==',') {
                        if (++n == 4) {
                            if (slen > i) {
                                // if name isn't empty, then copy
                                Title = (Name + i);
                                m_eWatchMode = eReplayDVD;
                                m_eVideoMode = eVideoMPG;
                                m_eAudioMode = eAudioMPG;
                            }
                            break;
                        }
                    }
                }
            }
            if (!Title) {
                // look for file extentsion like .xxx or .xxxx
                bool bIsFile = (slen>5 && ((*(Name+slen-4) == '.') || (*(Name+slen-5) == '.'))) ? true : false;

                for (int i=slen-1;i>0;--i)
                {   //search reverse last Subtitle
                    // - filename contains '~' => subdirectory
                    // or filename contains '/' => subdirectory
                    switch (*(Name+i)) {
                        case '/':
                            if (!bIsFile) {
                                break;
                            }
                            m_eWatchMode = eReplayFile;
                        case '~': {
                            Title = (Name + i + 1);
                            i = 0;
                        }
                        default:
                            break;
                    }
                }
            }

            if (0 == strncmp(Name,"[image] ",8)) {
                if (m_eWatchMode != eReplayFile) //if'nt already Name stripped-down as filename
                    Title = (Name + 8);
                m_eWatchMode = eReplayImage;
                m_eVideoMode = eVideoMPG;
                m_eAudioMode = eAudioNone;
            }
            else if (0 == strncmp(Name,"[audiocd] ",10)) {
                Title = (Name + 10);
                m_eWatchMode = eReplayAudioCD;
                m_eVideoMode = eVideoNone;
                m_eAudioMode = eAudioWAV;
            }
            if (Title) {
                replayTitle = new cString(skipspace(Title));
            } else {
                replayTitle = new cString(skipspace(Name));
            }
            free(Name);
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

bool ciMonWatch::ReplayPosition(int &current, int &total, double& dFrameRate) const
{
  if (m_pControl && ((cControl *)m_pControl)->GetIndex(current, total, false)) {
    total = (total == 0) ? 1 : total;
#if VDRVERSNUM >= 10703
    dFrameRate = ((cControl *)m_pControl)->FramesPerSecond();
#endif
    return true;
  }
  return false;
}

const char * ciMonWatch::FormatReplayTime(int current, int total, double dFrameRate) const
{
    static char s[32];

    int cs = (int)((double)current / dFrameRate);
    int ts = (int)((double)total / dFrameRate);
    bool g = (cs > 3600) || (ts > 3600);

    int cm = cs / 60;
    cs %= 60;
    int tm = ts / 60;
    ts %= 60;

    if (total > 1) {
      if(g) {
#if VDRVERSNUM >= 10703
        snprintf(s, sizeof(s), "%s (%s)", (const char*)IndexToHMSF(current,false,dFrameRate), (const char*)IndexToHMSF(total,false,dFrameRate));
#else
        snprintf(s, sizeof(s), "%s (%s)", (const char*)IndexToHMSF(current), (const char*)IndexToHMSF(total));
#endif
      } else {
        snprintf(s, sizeof(s), "%02d:%02d (%02d:%02d)", cm, cs, tm, ts);
      } 
    }
    else {
      if(g) {
#if VDRVERSNUM >= 10703
        snprintf(s, sizeof(s), "%s", (const char*)IndexToHMSF(current,false,dFrameRate));
#else
        snprintf(s, sizeof(s), "%s", (const char*)IndexToHMSF(current));
#endif
      } else {
        snprintf(s, sizeof(s), "%02d:%02d", cm, cs);
      }
    }
    return s;
}

bool ciMonWatch::ReplayTime(int &current, int &total) {
    double dFrameRate;
#if VDRVERSNUM >= 10701
    dFrameRate = DEFAULTFRAMESPERSECOND;
#else
    dFrameRate = FRAMESPERSEC;
#endif
    if(ReplayPosition(current,total,dFrameRate) 
      && theSetup.m_nRenderMode == eRenderMode_DualLine) {
      const char * sz = FormatReplayTime(current,total,dFrameRate);
      if(!replayTime || strcmp(sz,*replayTime)) {
        if(replayTime)
          delete replayTime;
        replayTime = new cString(sz);
        return replayTime != NULL;      
      }
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
    esyslog("iMonLCD: Recording: only up to %u devices are supported by this plugin", (unsigned int) memberof(m_nCardIsRecording));
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

    const cChannel * ch = NULL;
#if APIVERSNUM >= 20302
    cStateKey lock;
    if (const cChannels *Channels = cChannels::GetChannelsRead(lock)) {
       ch = Channels->GetByNumber(ChannelNumber);
       lock.Remove();
    }
#else
    ch = Channels.GetByNumber(ChannelNumber);
#endif

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
#if APIVERSNUM >= 20302
    cStateKey lock;
    const cSchedules * schedules = cSchedules::GetSchedulesRead(lock);
#else
    cSchedulesLock lock;
    const cSchedules * schedules = cSchedules::Schedules(lock);
#endif
    if (schedules) {
      if (chID.Valid()) {
        const cSchedule * schedule = schedules->GetSchedule(chID);
        if (schedule && (p = schedule->GetPresentEvent()) != NULL) {
          if(!chPresentTime || chEventID != p->EventID()) {
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
#if APIVERSNUM >= 20302
      lock.Remove();
#endif
    }
    return bChanged;
}

void ciMonWatch::Volume(int nVolume, bool bAbsolute)
{
  cMutexLooker m(mutex);

  int nAbsVolume;

  nAbsVolume = m_nLastVolume;
  if (bAbsolute) {
      nAbsVolume = nVolume;
  } else {
      nAbsVolume += nVolume;
  }
  if(nAbsVolume > MAXVOLUME) {
      nAbsVolume = MAXVOLUME;
  }
  else if(nAbsVolume < 0) {
      nAbsVolume = 0;
  }

  if(m_nLastVolume > 0 && 0 == nAbsVolume) {
    m_bVolumeMute = true;
  }
  else if(0 == m_nLastVolume && nAbsVolume > 0) {
    m_bVolumeMute = false;
  }
  m_nLastVolume = nAbsVolume;
}


void ciMonWatch::OsdClear() {
    cMutexLooker m(mutex);
    if(osdMessage) { 
        delete osdMessage;
        osdMessage = NULL;
        m_bUpdateScreen = true;
    }
    if(osdTitle) { 
        delete osdTitle;
        osdTitle = NULL;
        m_bUpdateScreen = true;
    }
    if(osdItem) { 
        delete osdItem;
        osdItem = NULL;
        m_bUpdateScreen = true;
    }
}

void ciMonWatch::OsdTitle(const char *sz) {
    char *s = NULL;
    char *sc = NULL;
    if(sz && !isempty(sz)) {
        s = strdup(sz);
        sc = compactspace(strreplace(s,'\t',' '));
    }
    if(sc 
        && osdTitle 
        && 0 == strcmp(sc, *osdTitle)) {
      if(s) {
        free(s);
      }
      return;
    }
    cMutexLooker m(mutex);
    if(osdTitle) { 
        delete osdTitle;
        osdTitle = NULL;
        m_bUpdateScreen = true;
    }
    if(sc) {
          osdTitle = new cString(sc);
          m_bUpdateScreen = true;
    }
    if(s) {
      free(s);
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

bool ciMonWatch::SetFont(const char *szFont, bool bTwoLineMode, int nBigFontHeight, int nSmallFontHeight) {
    cMutexLooker m(mutex);
    if(ciMonLCD::SetFont(szFont, bTwoLineMode, nBigFontHeight, nSmallFontHeight)) {
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

