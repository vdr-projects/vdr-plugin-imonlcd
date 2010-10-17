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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "watch.h"
#include "setup.h"

#include <vdr/plugin.h>
#include <vdr/tools.h>
#include <vdr/config.h>
#include <vdr/i18n.h>

#define DEFAULT_ON_EXIT      eOnExitMode_BLANKSCREEN	/**< Blank the device completely */
#define DEFAULT_CONTRAST     200
#define DEFAULT_DISCMODE     1	/**< spin the "slim" disc */
#define DEFAULT_WIDTH        96
#define DEFAULT_HEIGHT       16
#define DEFAULT_FONT         "Sans:Bold"
#define DEFAULT_WAKEUP       5
#define DEFAULT_TWO_LINE_MODE	eRenderMode_SingleLine
#define DEFAULT_BIG_FONT_HEIGHT   14
#define DEFAULT_SMALL_FONT_HEIGHT 7

/// The one and only Stored setup data
cIMonSetup theSetup;

/// Ctor, load default values
cIMonSetup::cIMonSetup(void)
{
  m_nOnExit = DEFAULT_ON_EXIT;
  m_nContrast = DEFAULT_CONTRAST;
  m_bDiscMode = DEFAULT_DISCMODE;

  m_nWidth = DEFAULT_WIDTH;
  m_nHeight = DEFAULT_HEIGHT;

  m_nWakeup = DEFAULT_WAKEUP;
  m_nRenderMode = DEFAULT_TWO_LINE_MODE;
  m_nBigFontHeight = DEFAULT_BIG_FONT_HEIGHT;
  m_nSmallFontHeight = DEFAULT_SMALL_FONT_HEIGHT;

  strncpy(m_szFont,DEFAULT_FONT,sizeof(m_szFont));
}

cIMonSetup::cIMonSetup(const cIMonSetup& x)
{
  *this = x;
}

cIMonSetup& cIMonSetup::operator = (const cIMonSetup& x)
{
  m_nOnExit = x.m_nOnExit;
  m_nContrast = x.m_nContrast;
  m_bDiscMode = x.m_bDiscMode;

  m_nWidth = x.m_nWidth;
  m_nHeight = x.m_nHeight;

  m_nWakeup = x.m_nWakeup;
  m_nRenderMode = x.m_nRenderMode;
  m_nBigFontHeight = x.m_nBigFontHeight;
  m_nSmallFontHeight = x.m_nSmallFontHeight;

  strncpy(m_szFont,x.m_szFont,sizeof(m_szFont));

  return *this;
}

/// compare profilenames and load there value 
bool cIMonSetup::SetupParse(const char *szName, const char *szValue)
{
  // Width
  if(!strcasecmp(szName, "Width")) {
    int n = atoi(szValue);
    if ((n < 0) || (n > 320)) {
		    esyslog("iMonLCD: Width must be between 0 and 320; using default %d",
		           DEFAULT_WIDTH);
		    n = DEFAULT_WIDTH;
    }
    m_nWidth = n;
    return true;
  }
  // Height
  if(!strcasecmp(szName, "Height")) {
    int n = atoi(szValue);
    if ((n < 0) || (n > 240)) {
		    esyslog("iMonLCD: Height must be between 0 and 240; using default %d",
		           DEFAULT_HEIGHT);
		    n = DEFAULT_HEIGHT;
    }
    m_nHeight = n;
    return true;
  }

  // OnExit
  if(!strcasecmp(szName, "OnExit")) {
    int n = atoi(szValue);
    if ((n < eOnExitMode_SHOWMSG) || (n >= eOnExitMode_LASTITEM)) {
		    esyslog("iMonLCD: OnExit must be between %d and %d; using default %d",
		           eOnExitMode_SHOWMSG, eOnExitMode_LASTITEM, DEFAULT_ON_EXIT);
		    n = DEFAULT_ON_EXIT;
    }
    m_nOnExit = n;
    return true;
  }

  // Contrast
  if(!strcasecmp(szName, "Contrast")) {
    int n = atoi(szValue);
    if ((n < 0) || (n > 1000)) {
		    esyslog("iMonLCD: Contrast must be between 0 and 1000, using default %d",
		           DEFAULT_CONTRAST);
		    n = DEFAULT_CONTRAST;
    }
    m_nContrast = n;
    return true;
  }
  // Font
  if(!strcasecmp(szName, "Font")) {
    if(szValue) {
      cStringList fontNames;
      cFont::GetAvailableFontNames(&fontNames);
      if(fontNames.Find(szValue)>=0) {
        strncpy(m_szFont,szValue,sizeof(m_szFont));
        return true;
      }
    }
    esyslog("iMonLCD: Font '%s' not found, using default %s", 
        szValue, DEFAULT_FONT);
    strncpy(m_szFont,DEFAULT_FONT,sizeof(m_szFont));
    return true;
  }

  // DiscMode
  if(!strcasecmp(szName, "DiscMode")) {
    m_bDiscMode = atoi(szValue) == 0?0:1;
    return true;
  }

  // Wakeup
  if(!strcasecmp(szName, "Wakeup")) {
    int n = atoi(szValue);
    if ((n < 0) || (n > 1440)) {
		    esyslog("iMonLCD: Wakeup must be between 0 and 1440, using default %d",
		           DEFAULT_WAKEUP);
		    n = DEFAULT_WAKEUP;
    }
    m_nWakeup = n;
    return true;
  }
  // BigFont
  if(!strcasecmp(szName, "BigFont")) {
    int n = atoi(szValue);
    if ((n < 5) || (n > 24)) {
		    esyslog("iMonLCD:  BigFont must be between 5 and 24, using default %d",
		           DEFAULT_BIG_FONT_HEIGHT);
		    n = DEFAULT_BIG_FONT_HEIGHT;
    }
    m_nBigFontHeight = n;
    return true;
  }
  // SmallFont
  if(!strcasecmp(szName, "SmallFont")) {
    int n = atoi(szValue);
    if ((n < 5) || (n > 24)) {
		    esyslog("iMonLCD:  SmallFont must be between 5 and 24, using default %d",
		           DEFAULT_SMALL_FONT_HEIGHT);
		    n = DEFAULT_SMALL_FONT_HEIGHT;
    }
    m_nSmallFontHeight = n;
    return true;
  }

  // Two Line Mode
  if(!strcasecmp(szName, "TwoLineMode")) {
    int n = atoi(szValue);
    if ((n < eRenderMode_SingleLine) || (n >= eRenderMode_LASTITEM)) {
      esyslog("iMonLCD:  TwoLineMode must be between %d and %d, using default %d", 
              eRenderMode_SingleLine, eRenderMode_LASTITEM, DEFAULT_TWO_LINE_MODE );
      n = DEFAULT_TWO_LINE_MODE;
    }
    m_nRenderMode = n;
    return true;
  }

  //Unknow parameter
  return false;
}


// --- ciMonMenuSetup --------------------------------------------------------
void ciMonMenuSetup::Store(void)
{
  theSetup = m_tmpSetup;

  SetupStore("Width",      theSetup.m_nWidth);
  SetupStore("Height",     theSetup.m_nHeight);
  SetupStore("OnExit",     theSetup.m_nOnExit);
  SetupStore("Contrast",   theSetup.m_nContrast);
  SetupStore("DiscMode",   theSetup.m_bDiscMode);
  SetupStore("Font",       theSetup.m_szFont);
  SetupStore("BigFont",    theSetup.m_nBigFontHeight);
  SetupStore("SmallFont",  theSetup.m_nSmallFontHeight);
  SetupStore("Wakeup",     theSetup.m_nWakeup);
  SetupStore("TwoLineMode",theSetup.m_nRenderMode);
}

ciMonMenuSetup::ciMonMenuSetup(ciMonWatch*    pDev)
: m_tmpSetup(theSetup)
, m_pDev(pDev)
{
  SetSection(tr("iMON LCD"));

  cFont::GetAvailableFontNames(&fontNames);
  fontNames.Insert(strdup(DEFAULT_FONT));
  fontIndex = max(0, fontNames.Find(m_tmpSetup.m_szFont));

  Add(new cMenuEditIntItem (tr("Contrast"),           
        &m_tmpSetup.m_nContrast,        
        0, 1000));

  Add(new cMenuEditStraItem(tr("Default font"),           
        &fontIndex, fontNames.Size(), &fontNames[0]));
  Add(new cMenuEditIntItem (tr("Height of big font"),           
        &m_tmpSetup.m_nBigFontHeight,        
        5, 24));
  Add(new cMenuEditIntItem (tr("Height of small font"),           
        &m_tmpSetup.m_nSmallFontHeight,        
        5, 24));

  Add(new cMenuEditBoolItem(tr("Disc spinning mode"),                    
        &m_tmpSetup.m_bDiscMode,    
        tr("Slim disc"), tr("Full disc")));

  static const char * szRenderMode[3];
  szRenderMode[eRenderMode_SingleLine] = tr("Single line");
  szRenderMode[eRenderMode_DualLine] = tr("Dual lines");
  szRenderMode[eRenderMode_SingleTopic] = tr("Only topic");

  Add(new cMenuEditStraItem(tr("Render mode"),                    
        &m_tmpSetup.m_nRenderMode,    
        memberof(szRenderMode), szRenderMode));

  static const char * szExitModes[eOnExitMode_LASTITEM];
  szExitModes[eOnExitMode_SHOWMSG]      = tr("Do nothing");
  szExitModes[eOnExitMode_SHOWCLOCK]    = tr("Showing clock");
  szExitModes[eOnExitMode_BLANKSCREEN]  = tr("Turning backlight off");
  szExitModes[eOnExitMode_NEXTTIMER]    = tr("Show next timer");
  szExitModes[eOnExitMode_WAKEUP]       = tr("Wake up on next timer");

  Add(new cMenuEditStraItem (tr("Exit mode"),           
        &m_tmpSetup.m_nOnExit,
        memberof(szExitModes), szExitModes));

  Add(new cMenuEditIntItem (tr("Margin time at wake up (min)"),
        &m_tmpSetup.m_nWakeup,        
        0, 1440));

/* Adjust need add moment restart
  Add(new cMenuEditIntItem (tr("Display width"),           
        &m_tmpSetup.m_nWidth,        
        0, 320));

  Add(new cMenuEditIntItem (tr("Display height"),           
        &m_tmpSetup.m_nHeight,        
        0, 240));
*/
}

eOSState ciMonMenuSetup::ProcessKey(eKeys nKey)
{
  if(nKey == kOk) {
    // Store edited Values
    Utf8Strn0Cpy(m_tmpSetup.m_szFont, fontNames[fontIndex], sizeof(m_tmpSetup.m_szFont));
    if (0 != strcmp(m_tmpSetup.m_szFont, theSetup.m_szFont)
        || m_tmpSetup.m_nRenderMode != theSetup.m_nRenderMode
        || ( m_tmpSetup.m_nRenderMode != eRenderMode_DualLine && (m_tmpSetup.m_nBigFontHeight != theSetup.m_nBigFontHeight))
        || ( m_tmpSetup.m_nRenderMode == eRenderMode_DualLine && (m_tmpSetup.m_nSmallFontHeight != theSetup.m_nSmallFontHeight))
      ) {
        m_pDev->SetFont(m_tmpSetup.m_szFont, 
                        m_tmpSetup.m_nRenderMode == eRenderMode_DualLine ? true : false, 
                        m_tmpSetup.m_nBigFontHeight, 
                        m_tmpSetup.m_nSmallFontHeight);
    }
	}
  return cMenuSetupPage::ProcessKey(nKey);
}


