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

#include <vdr/plugin.h>
#include <getopt.h>
#include <string.h>

#include "imon.h"
#include "watch.h"
#include "status.h"
#include "setup.h"

static const char *VERSION        = "0.0.2";

static const char *DEFAULT_LCDDEVICE  = "/dev/lcd0";

class cPluginImonlcd : public cPlugin {
private:
  ciMonStatusMonitor *statusMonitor;
  char*              m_szDevice;
  ciMonWatch         m_dev;
  eProtocol          m_Protocol;
  bool               m_bSuspend;
protected:
  bool resume();
  bool suspend();
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

const char **cPluginImonlcd::SVDRPHelpPages(void)
{
  // Return help text for SVDRP commands this plugin implements
  static const char *HelpPages[] = {
    "ON\n"
    "    Resume driver of display.",
    "OFF\n"
    "    Suspend driver of display.",
    NULL
    };
  return HelpPages;
}

cString cPluginImonlcd::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
  // Process SVDRP commands this plugin implements
  if(!strcasecmp(Command, "ON")) {
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
  } else if(!strcasecmp(Command, "OFF")) {
    if(suspend()) {
      ReplyCode=250; 
      return "driver suspended";
    } else {
      ReplyCode=251; 
      return "driver already suspended";
    }
  } else { 
    ReplyCode=501; 
    return "unknown command"; 
  }

  return NULL;
}

VDRPLUGINCREATOR(cPluginImonlcd); // Don't touch this!
