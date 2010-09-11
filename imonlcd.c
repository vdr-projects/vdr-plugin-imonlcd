/*
 * iMON LCD plugin for VDR (C++)
 *
 * (C) 2009-2010 Andreas Brachold <vdr07 AT deltab de>
 *
 * This iMON LCD plugin is free software: you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as published 
 * by the Free Software Foundation, version 3 of the License.
 *
 * See the files README and COPYING for details.
 *
 */

#include <vdr/plugin.h>
#include <getopt.h>
#include <string.h>

#include "imon.h"
#include "watch.h"
#include "status.h"
#include "setup.h"

static const char *VERSION        = "0.0.5";

static const char *DEFAULT_LCDDEVICE  = "/dev/lcd0";

class cPluginImonlcd : public cPlugin {
private:
  ciMonStatusMonitor *statusMonitor;
  char*              m_szDevice;
  ciMonWatch         m_dev;
  eProtocol          m_Protocol;
  bool               m_bSuspend;
  char*              m_szIconHelpPage;
protected:
  bool resume();
  bool suspend();

  const char* SVDRPCommandOn(const char *Option, int &ReplyCode);
  const char* SVDRPCommandOff(const char *Option, int &ReplyCode);
  const char* SVDRPCommandIcon(const char *Option, int &ReplyCode);

public:
  cPluginImonlcd(void);
  virtual ~cPluginImonlcd();
  virtual const char *Version(void) { return VERSION; }
  virtual const char *Description(void) { return tr("Control a iMON LCD"); }
  virtual const char *CommandLineHelp(void);
  virtual bool ProcessArgs(int argc, char *argv[]);
  virtual bool Initialize(void);
  virtual bool Start(void);
  virtual void Stop(void);
  virtual void Housekeeping(void);
  virtual void MainThreadHook(void);
  virtual cString Active(void);
  virtual time_t WakeupTime(void);
  virtual const char *MainMenuEntry(void) { return NULL; }
  virtual cOsdObject *MainMenuAction(void);
  virtual cMenuSetupPage *SetupMenu(void);
  virtual bool SetupParse(const char *Name, const char *Value);
  virtual bool Service(const char *Id, void *Data = NULL);
  virtual const char **SVDRPHelpPages(void);
  virtual cString SVDRPCommand(const char *Command, const char *Option, int &ReplyCode);
  };

cPluginImonlcd::cPluginImonlcd(void)
: m_Protocol(ePROTOCOL_0038)
{
  m_bSuspend = true;
  statusMonitor = NULL;
  m_szDevice = NULL;
  m_szIconHelpPage = NULL;
}

cPluginImonlcd::~cPluginImonlcd()
{
  //Cleanup if'nt throw housekeeping
  if(statusMonitor) {
    delete statusMonitor;
    statusMonitor = NULL;
  }

  if(m_szDevice) {
    free(m_szDevice);
    m_szDevice = NULL;
  }  

  if(m_szIconHelpPage) {
    free(m_szIconHelpPage);
    m_szIconHelpPage = NULL;
  }
}

const char *cPluginImonlcd::CommandLineHelp(void)
{
  // Return a string that describes all known command line options.
  return
"  -d DEV,   --device=DEV     sets the lcd-device to other device than /dev/lcd0\n"
"  -p MODE,  --protocol=MODE  sets the protocol of lcd-device\n"
"    '0038'                   For LCD with ID 15c2:0038 SoundGraph Inc (default)\n"
"    'ffdc'                   For LCD with ID 15c2:ffdc SoundGraph Inc\n";

}

bool cPluginImonlcd::ProcessArgs(int argc, char *argv[])
{
  static struct option long_options[] =
  {
    { "device",   required_argument, NULL, 'd'},
    { "protocol", required_argument, NULL, 'p'},
    { NULL}
  };

  int c;
  int option_index = 0;
  while ((c = getopt_long(argc, argv, "d:p:", long_options, &option_index)) != -1)
  {
    switch (c)
    {
      case 'd':
      {
        if(m_szDevice) {
          free(m_szDevice);
          m_szDevice = NULL;
        }
        m_szDevice = strdup(optarg);
        break;
      }
      case 'p':
      {
        if(strcasecmp(optarg,"0038") == 0) {
          m_Protocol = ePROTOCOL_0038;
        } else if(strcasecmp(optarg,"ffdc") == 0) {
          m_Protocol = ePROTOCOL_FFDC;
        }
        break;
      }
      default:
        return false;
    }
  }

  if (NULL == m_szDevice)
  {
    // neither Device given:
    // => set "/dev/lcd0" it to default
    m_szDevice = strdup(DEFAULT_LCDDEVICE);
  }

  return true;
}

bool cPluginImonlcd::Initialize(void)
{
  // Initialize any background activities the plugin shall perform.
  return true;
}

bool cPluginImonlcd::resume() {

  if(m_bSuspend
      && NULL != m_szDevice 
      && 0 == m_dev.open(m_szDevice,m_Protocol)) {
        m_bSuspend = false;
      return true;
  }
  return false;
}

bool cPluginImonlcd::suspend() {
  if(!m_bSuspend) {
    m_dev.close();
    m_bSuspend = true;
    return true;
  }
  return false;
}

bool cPluginImonlcd::Start(void)
{
  if(resume()) {
      statusMonitor = new ciMonStatusMonitor(&m_dev);
      if(NULL == statusMonitor){
        esyslog("iMonLCD: can't create ciMonStatusMonitor!");
        return (false);
      }
  }
  return true;
}

void cPluginImonlcd::Stop(void)
{
  if(statusMonitor) {
    delete statusMonitor;
    statusMonitor = NULL;
  }

  m_dev.close();
  
  if(m_szDevice) {
    free(m_szDevice);
    m_szDevice = NULL;
  }  
}

void cPluginImonlcd::Housekeeping(void)
{
  // Perform any cleanup or other regular tasks.
}

void cPluginImonlcd::MainThreadHook(void)
{
  // Perform actions in the context of the main program thread.
  // WARNING: Use with great care - see PLUGINS.html!
}

cString cPluginImonlcd::Active(void)
{
  // Return a message string if shutdown should be postponed
  return NULL;
}

time_t cPluginImonlcd::WakeupTime(void)
{
  // Return custom wakeup time for shutdown script
  return 0;
}

cOsdObject *cPluginImonlcd::MainMenuAction(void)
{
  return NULL;
}

cMenuSetupPage *cPluginImonlcd::SetupMenu(void)
{
  return new ciMonMenuSetup(&m_dev);
}

bool cPluginImonlcd::SetupParse(const char *szName, const char *szValue)
{
  return theSetup.SetupParse(szName,szValue);
}

bool cPluginImonlcd::Service(const char *Id, void *Data)
{
  // Handle custom service requests from other plugins
  return false;
}

const char* cPluginImonlcd::SVDRPCommandOn(const char *Option, int &ReplyCode)
{
    if(!m_bSuspend) {
      ReplyCode=251; 
      return "driver already resumed";
    }
    if(resume()) {
      ReplyCode=250; 
      return "driver resumed";
    } else {
      ReplyCode=554; 
      return "driver could not resumed";
    }
}

const char* cPluginImonlcd::SVDRPCommandOff(const char *Option, int &ReplyCode)
{
    if(suspend()) {
      ReplyCode=250; 
      return "driver suspended";
    } else {
      ReplyCode=251; 
      return "driver already suspended";
    }
}

static const struct  {
    unsigned int nIcon;
    const char* szIcon;    
} icontable[] = {
    { eIconTopMusic  , "Music" },
    { eIconTopMovie  , "Movie" },
    { eIconTopPhoto  , "Photo" },
    { eIconTopDVD    , "DVD" },
    { eIconTopTV     , "TopTV" },
    { eIconTopWeb    , "Web" },
    { eIconTopNews   , "News" },
    { eIconSpeakerL  , "SpeakerL" },
    { eIconSpeakerR  , "SpeakerR" },
    { eIconSpeakerLR , "SpeakerLR" },
    { eIconSpeaker51 , "Speaker51" },
    { eIconSpeaker71 , "Speaker71" },
    { eIconSPDIF     , "SPDIF" },
    { eIconMute      , "MUTE" },
    { eIconDiscSpin|eIconDiscRunSpin|eIconDiscEllispe  , "DISC" },
    { eIconTV        , "TV" },
    { eIconHDTV      , "HDTV" },
    { eIconSRC       , "SRC" },
    { eIconSRC1      , "SRC1" },
    { eIconSRC2      , "SRC2" },
    { eIconFIT       , "FIT" },
    { eIconBL_MPG    , "MPG" },
    { eIconBL_DIVX   , "DIVX" },
    { eIconBL_XVID   , "XVID" },
    { eIconBL_WMV    , "WMV" },
    { eIconBM_MPG    , "MPGA" },
    { eIconBM_AC3    , "AC3" },
    { eIconBM_DTS    , "DTS" },
    { eIconBM_WMA    , "WMAA" },
    { eIconBR_MP3    , "MP3" },
    { eIconBR_OGG    , "OGG" },
    { eIconBR_WMA    , "WMA" },
    { eIconBR_WAV    , "WAV" },
    { eIconVolume    , "VOL" },
    { eIconTime      , "TIME" },
    { eIconAlarm     , "ALARM" },
    { eIconRecording , "REC" },
    { eIconRepeat    , "REP" },
    { eIconShuffle   , "SFL" }
};

const char* cPluginImonlcd::SVDRPCommandIcon(const char *Option, int &ReplyCode)
{
  if(m_bSuspend) {
      ReplyCode=251; 
      return "driver suspended";
  }
  if( Option && strlen(Option) < 256) {
    char* request = strdup(Option);
    char* cmd = NULL;
    if( request ) {
      char* tmp = request;
      eIconState m = eIconStateQuery;
      cmd = strtok_r(request, " ", &tmp);
      if(cmd) {
        char* param = strtok_r(0, " ", &tmp);
        if(param) {
          if(!strcasecmp(param,"ON")) {
            m = eIconStateOn;
          } else if(!strcasecmp(param,"OFF")) {
            m = eIconStateOff;
          } else if(!strcasecmp(param,"AUTO")) {
            m = eIconStateAuto;
          } else {
            ReplyCode=501; 
            free(request);
            return "unknown icon state";
          }
        }
      }

      ReplyCode=501; 
      const char* szReplay = "unknown icon title";
      if(cmd) {
        unsigned int i;
        for(i = 0; i < (sizeof(icontable)/sizeof(*icontable));++i)
        {
            if(0 == strcasecmp(cmd,icontable[i].szIcon))    
            {
                eIconState r = m_dev.ForceIcon(icontable[i].nIcon, m);
                switch(r) {
                  case eIconStateAuto:
                    ReplyCode=250; 
                    szReplay = "icon state 'auto'";
                    break;
                  case eIconStateOn:
                    ReplyCode=251; 
                    szReplay = "icon state 'on'";
                    break;
                  case eIconStateOff:
                    ReplyCode=252; 
                    szReplay = "icon state 'off'";
                    break;
                  default:
                    break;
                }
                break;
            }
        }
      }
      free(request);
      return szReplay;
    }
  }
  ReplyCode=501; 
  return "wrong parameter";
}

cString cPluginImonlcd::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
  ReplyCode=501; 
  const char* szReplay = "unknown command";

  if(!strcasecmp(Command, "ON")) {
    szReplay = SVDRPCommandOn(Option,ReplyCode);
  } else if(!strcasecmp(Command, "OFF")) {
    szReplay = SVDRPCommandOff(Option,ReplyCode);
  } else if(!strcasecmp(Command, "ICON")) {
    szReplay = SVDRPCommandIcon(Option,ReplyCode);
  } 

  dsyslog("iMonLCD: SVDRP %s %s - %d (%s)", Command, Option, ReplyCode, szReplay);
  return szReplay;
}

const char **cPluginImonlcd::SVDRPHelpPages(void)
{
  if(!m_szIconHelpPage) {
    unsigned int i,l,k;
    for(i = 0,l = 0, k = (sizeof(icontable)/sizeof(*icontable)); i < k;++i) {
      l += strlen(icontable[i].szIcon);
      l += 2;
    }
    l += 80;

    m_szIconHelpPage = (char*) calloc(l + 1,1);
    if(m_szIconHelpPage) {
      strncat(m_szIconHelpPage, "ICON [name] [on|off|auto]\n    Force state of icon. Names of icons are:", l);
      for(i = 0; i < k;++i) {
        if((i % 7) == 0) {
          strncat(m_szIconHelpPage, "\n    ", l - (strlen(m_szIconHelpPage) + 5));
        } else {
          strncat(m_szIconHelpPage, ",", l - (strlen(m_szIconHelpPage) + 1));
        }
        strncat(m_szIconHelpPage, icontable[i].szIcon, 
          l - (strlen(m_szIconHelpPage) + strlen(icontable[i].szIcon)));
      }
    }
  }

  // Return help text for SVDRP commands this plugin implements
  static const char *HelpPages[] = {
    "ON\n"
    "    Resume driver of display.\n",
    "OFF\n"
    "    Suspend driver of display.\n",
    "ICON [name] [on|off|auto]\n"
    "    Force state of icon.\n",
    NULL
    };
  if(m_szIconHelpPage)
    HelpPages[2] = m_szIconHelpPage;
  return HelpPages;
}
VDRPLUGINCREATOR(cPluginImonlcd); // Don't touch this!
