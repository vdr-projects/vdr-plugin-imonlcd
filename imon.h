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

#ifndef __IMON_LCD_H_
#define __IMON_LCD_H_

#include "bitmap.h"

enum eProtocol {
  ePROTOCOL_FFDC   =   0,	/**< protocol ID for 15c2:ffdc device */
  ePROTOCOL_0038   =   1	/**< protocol ID for 15c2:0038 device */
};

enum eIcons {
  eIconOff = 0,
  eIconDiscSpin  = 1 << 0,

  eIconTopMusic  = 1 << 1,
  eIconTopMovie  = 2 << 1,
  eIconTopPhoto  = 3 << 1,
  eIconTopDVD    = 4 << 1,
  eIconTopTV     = 5 << 1,
  eIconTopWeb    = 6 << 1,
  eIconTopNews   = 7 << 1,
  eIconTopMask   = (eIconTopMusic|eIconTopMovie|eIconTopPhoto|eIconTopDVD|eIconTopTV|eIconTopWeb|eIconTopNews),

  eIconSpeakerL  = 1 << 4,
  eIconSpeakerR  = 2 << 4,
  eIconSpeakerLR = (eIconSpeakerL|eIconSpeakerR),
  eIconSpeaker51 = 4 << 4,
  eIconSpeaker71 = 5 << 4,
  eIconSPDIF     = 6 << 4,
  eIconMute      = 7 << 4,
  eIconSpeakerMask = (eIconSpeakerLR|eIconSpeaker51|eIconSpeaker71|eIconSPDIF|eIconMute),

  eIconSRC       = 1 << 7,
  eIconFIT       = 1 << 8,
  eIconTV        = 1 << 9,
  eIconHDTV      = 1 << 10,
  eIconSRC1      = 1 << 11,
  eIconSRC2      = 1 << 12,

  eIconBR_MP3    = 1 << 13,
  eIconBR_OGG    = 2 << 13,
  eIconBR_WMA    = 3 << 13,
  eIconBR_WAV    = 4 << 13,
  eIconBR_Mask    = (eIconBR_MP3|eIconBR_OGG|eIconBR_WMA|eIconBR_WAV),

  eIconBM_MPG    = 1 << 16,
  eIconBM_AC3    = 2 << 16,
  eIconBM_DTS    = 3 << 16,
  eIconBM_WMA    = 4 << 16,
  eIconBM_Mask    = (eIconBM_MPG|eIconBM_AC3|eIconBM_DTS|eIconBM_WMA),

  eIconBL_MPG    = 1 << 19,
  eIconBL_DIVX   = 2 << 19,
  eIconBL_XVID   = 3 << 19,
  eIconBL_WMV    = 4 << 19,
  eIconBL_Mask    = (eIconBL_MPG|eIconBL_DIVX|eIconBL_XVID|eIconBL_WMV),

  eIconVolume    = 1 << 22,
  eIconTime      = 1 << 23,
  eIconAlarm     = 1 << 24,

  eIconRecording = 1 << 25,

  eIconRepeat    = 1 << 26,
  eIconShuffle   = 1 << 27,

  eIconDiscEllispe = 1 << 28,

  eIconDiscRunSpin = 1 << 29,
  eIconDiscSpinBackward = 1 << 30
};

class ciMonFont;
class ciMonLCD {

	int imon_fd;

  ciMonFont*   pFont;
	/* framebuffer and backingstore for current contents */
	ciMonBitmap* framebuf;
	ciMonBitmap* backingstore;

	/* store commands appropriate for the version of the iMON LCD */
	uint64_t cmd_display;
	uint64_t cmd_shutdown;
	uint64_t cmd_display_on;
	uint64_t cmd_clear_alarm;

	/*
	 * record the last "state" of the CD icon so that we can "animate"
	 * it.
	 */
	int last_cd_state;

protected:

  void setLineLength(int topLine, int botLine, int topProgress, int botProgress);
  void setBuiltinProgressBars(int topLine, int botLine, int topProgress, int botProgress);
  int lengthToPixels(int length);

  bool SendCmd(const uint64_t & cmdData);
  bool SendCmdClock(time_t tAlarm);
  bool SendCmdShutdown();
  bool Contrast(int nContrast);
public:
  ciMonLCD();
  virtual ~ciMonLCD();

  virtual int open(const char* szDevice, eProtocol pro);
  virtual void close ();

  bool isopen() const { return imon_fd >= 0; }
  void clear ();
  int DrawText(int x, int y, const char* string);
  bool flush ();

  bool icons(unsigned int state);
  virtual bool SetFont(const char *szFont, int m_bTwoLineMode);
};
#endif



