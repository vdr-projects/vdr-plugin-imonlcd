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

#ifndef __IMON_SETUP_H___
#define __IMON_SETUP_H___

#include <vdr/menuitems.h>
#define memberof(x) (sizeof(x)/sizeof(*x))

struct cIMonSetup 
{
  int          m_nOnExit;
  int          m_nContrast;
	int          m_bDiscMode;

  int          m_nWidth;
  int          m_nHeight;

  char         m_szFont[256];

  cIMonSetup(void);
  cIMonSetup(const cIMonSetup& x);
  cIMonSetup& operator = (const cIMonSetup& x);
  
  /// Parse our own setup parameters and store their values.
  bool SetupParse(const char *szName, const char *szValue);
  
};

class ciMonWatch;
class ciMonMenuSetup
	:public cMenuSetupPage 
{
  cIMonSetup  m_tmpSetup;
  ciMonWatch* m_pDev;
  cStringList fontNames;
  int         fontIndex;
protected:
  virtual void Store(void);
  virtual eOSState ProcessKey(eKeys nKey);
public:
  ciMonMenuSetup(ciMonWatch*    pDev);
};


/// The exported one and only Stored setup data
extern cIMonSetup theSetup;

#endif  //__IMON_SETUP_H___

