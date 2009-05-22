/*
 * iMON LCD plugin to VDR (C++)
 *
 * (C) 2009 Andreas Brachold <vdr07 AT deltab de>
 *
 *- Based on lcdproc Driver for SoundGraph iMON OEM LCD Modules
 *
 * Copyright (c) 2004, Venky Raju <dev (at) venky (dot) ws>
 *               2007, Dean Harding <dean (at) codeka dotcom>
 *               2007, Christian Leuschen <christian (dot) leuschen (at) gmx (dot) de>
 *               2009, Jonathan Kyler <jkyler (at) users (dot) sourceforge (dot) net>
 *               2009, Eric Pooch < epooch (at) cox (dot) net>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>

#include <vdr/tools.h>

#include "setup.h"
#include "ffont.h"
#include "imon.h"

/*
 * Just for convenience and to have the commands at one place.
 */
static const uint64_t CMD_SET_ICONS	        = (uint64_t) 0x0100000000000000LL;
static const uint64_t CMD_SET_CONTRAST	    = (uint64_t) 0x0300000000000000LL;
static const uint64_t CMD_DISPLAY	          = (uint64_t) 0x0000000000000000LL;  // must be or'd with the applicable CMD_DISPLAY_BYTE
static const uint64_t CMD_SHUTDOWN	        = (uint64_t) 0x0000000000000008LL;  // must be or'd with the applicable CMD_DISPLAY_BYTE
static const uint64_t CMD_DISPLAY_ON	      = (uint64_t) 0x0000000000000040LL;  // must be or'd with the applicable CMD_DISPLAY_BYTE
static const uint64_t CMD_CLEAR_ALARM	      = (uint64_t) 0x0000000000000000LL;  // must be or'd with the applicable CMD_ALARM_BYTE
static const uint64_t CMD_SET_LINES0	      = (uint64_t) 0x1000000000000000LL;
static const uint64_t CMD_SET_LINES1	      = (uint64_t) 0x1100000000000000LL;
static const uint64_t CMD_SET_LINES2	      = (uint64_t) 0x1200000000000000LL;
static const uint64_t CMD_INIT		          = (uint64_t) 0x0200000000000000LL;  //not exactly sure what this does, but it's needed
static const uint64_t CMD_LOW_CONTRAST	    = (uint64_t) (CMD_SET_CONTRAST + (uint64_t) 0x00FFFFFF00580A00LL);


/* Allow for variations in Soundgraphs numerous protocols */
/* 15c2:0038 SoundGraph iMON */
static const uint64_t CMD_DISPLAY_BYTE_0038	= (uint64_t) 0x8800000000000000LL;
static const uint64_t CMD_ALARM_BYTE_0038	  = (uint64_t) 0x8a00000000000000LL;

/* 15c2:ffdc SoundGraph iMON */
static const uint64_t CMD_DISPLAY_BYTE_FFDC	= (uint64_t) 0x5000000000000000LL;
static const uint64_t CMD_ALARM_BYTE_FFDC	  = (uint64_t) 0x5100000000000000LL;

/* Byte 6 */
static const uint64_t ICON_DISK_OFF	  = (uint64_t) 0x7F7000FFFFFFFFFFLL;
static const uint64_t ICON_DISK_ON	  = (uint64_t) 0x0080FF0000000000LL;

static const uint64_t ICON_DISK_IN	  = (uint64_t) 0x0080000000000000LL;
static const uint64_t ICON_CD_IN		  = (uint64_t) 0x00806B0000000000LL;
static const uint64_t ICON_DVD_IN	    = (uint64_t) 0x0080550000000000LL;

/* Byte 5 */
static const uint64_t ICON_AUDIO_WMA2	= ((uint64_t) 1 << 39);
static const uint64_t ICON_AUDIO_WAV	= ((uint64_t) 1 << 38);
static const uint64_t ICON_REP	      = ((uint64_t) 1 << 37);
static const uint64_t ICON_SFL	      = ((uint64_t) 1 << 36);
static const uint64_t ICON_ALARM	    = ((uint64_t) 1 << 35);
static const uint64_t ICON_REC	      = ((uint64_t) 1 << 34);
static const uint64_t ICON_VOL	      = ((uint64_t) 1 << 33);
static const uint64_t ICON_TIME	      = ((uint64_t) 1 << 32);

/* Byte 4 */
static const uint64_t ICON_XVID	      = ((uint64_t) 1 << 31);
static const uint64_t ICON_WMV	      = ((uint64_t) 1 << 30);
static const uint64_t ICON_AUDIO_MPG  = ((uint64_t) 1 << 29);
static const uint64_t ICON_AUDIO_AC3  = ((uint64_t) 1 << 28);
static const uint64_t ICON_AUDIO_DTS  = ((uint64_t) 1 << 27);
static const uint64_t ICON_AUDIO_WMA  = ((uint64_t) 1 << 26);
static const uint64_t ICON_AUDIO_MP3  = ((uint64_t) 1 << 25);
static const uint64_t ICON_AUDIO_OGG  = ((uint64_t) 1 << 24);

/* Byte 3 */
static const uint64_t ICON_SRC	      = ((uint64_t) 1 << 23);
static const uint64_t ICON_FIT	      = ((uint64_t) 1 << 22);
static const uint64_t ICON_TV_2	      = ((uint64_t) 1 << 21);
static const uint64_t ICON_HDTV	      = ((uint64_t) 1 << 20);
static const uint64_t ICON_SCR1	      = ((uint64_t) 1 << 19);
static const uint64_t ICON_SCR2	      = ((uint64_t) 1 << 18);
static const uint64_t ICON_MPG	      = ((uint64_t) 1 << 17);
static const uint64_t ICON_DIVX	      = ((uint64_t) 1 << 16);

/* Byte 2 */
static const uint64_t ICON_SPKR_FC	  = ((uint64_t) 1 << 15);
static const uint64_t ICON_SPKR_FR	  = ((uint64_t) 1 << 14);
static const uint64_t ICON_SPKR_SL	  = ((uint64_t) 1 << 13);
static const uint64_t ICON_SPKR_LFE	  = ((uint64_t) 1 << 12);
static const uint64_t ICON_SPKR_SR	  = ((uint64_t) 1 << 11);
static const uint64_t ICON_SPKR_RL	  = ((uint64_t) 1 << 10);
static const uint64_t ICON_SPKR_SPDIF	=  ((uint64_t) 1 << 9);
static const uint64_t ICON_SPKR_RR	  =  ((uint64_t) 1 << 8);

/* Byte 1 */
static const uint64_t ICON_MUSIC	    = ((uint64_t) 1 << 7);
static const uint64_t ICON_MOVIE	    = ((uint64_t) 1 << 6);
static const uint64_t ICON_PHOTO	    = ((uint64_t) 1 << 5);
static const uint64_t ICON_CD_DVD     = ((uint64_t) 1 << 4);
static const uint64_t ICON_TV	        = ((uint64_t) 1 << 3);
static const uint64_t ICON_WEBCAST    = ((uint64_t) 1 << 2);
static const uint64_t ICON_NEWS	      = ((uint64_t) 1 << 1);
static const uint64_t ICON_SPKR_FL	  = ((uint64_t) 1 << 0);


ciMonLCD::ciMonLCD() 
{
	this->imon_fd = -1;
  this->pFont = NULL;
	this->last_cd_state = 0;
}

ciMonLCD::~ciMonLCD() {
  this->close();

  if(pFont) {
    delete pFont;
    pFont = NULL;
  }
  if(framebuf) {
    delete framebuf;
    framebuf = NULL;
  }
  if(backingstore) {
    delete backingstore;
    backingstore = NULL;
  }

}


/**
 * Initialize the driver.
 * \retval 0	   Success.
 * \retval <0	  Error.
 */
int ciMonLCD::open(const char* szDevice, eProtocol pro)
{
  if(!SetFont(theSetup.m_szFont)) {
		return -1;
  }

	isyslog("iMonLCD: using Device %s, with 15c2:%s", szDevice, 
              (pro == ePROTOCOL_FFDC ? "ffdc":"0038"));

	/* Open device for writing */
	if ((this->imon_fd = ::open(szDevice, O_WRONLY)) < 0) {
		esyslog("iMonLCD: ERROR opening %s (%s).", szDevice, strerror(errno));
		esyslog("iMonLCD: Did you load the iMON kernel module?");
		return -1;
	}

	/* Set commands based on protocol version */
	if (pro == ePROTOCOL_FFDC) {
		this->cmd_display =      (CMD_DISPLAY | CMD_DISPLAY_BYTE_FFDC);
		this->cmd_shutdown =     (CMD_SHUTDOWN | CMD_DISPLAY_BYTE_FFDC);
		this->cmd_display_on =   (CMD_DISPLAY_ON | CMD_DISPLAY_BYTE_FFDC);
		this->cmd_clear_alarm =  (CMD_CLEAR_ALARM | CMD_ALARM_BYTE_FFDC);
	} else //if (pro == PROTOCOL_0038) 
  {
		this->cmd_display =      (CMD_DISPLAY | CMD_DISPLAY_BYTE_0038);
		this->cmd_shutdown =     (CMD_SHUTDOWN | CMD_DISPLAY_BYTE_0038);
		this->cmd_display_on =   (CMD_DISPLAY_ON | CMD_DISPLAY_BYTE_0038);
		this->cmd_clear_alarm =  (CMD_CLEAR_ALARM | CMD_ALARM_BYTE_0038);
	}

	/* Make sure the frame buffer is there... */
	this->framebuf = new ciMonBitmap(theSetup.m_nWidth,theSetup.m_nHeight);
	if (this->framebuf == NULL) {
		esyslog("iMonLCD: unable to allocate framebuffer");
		return -1;
	}

	/* Make sure the framebuffer backing store is there... */
	this->backingstore = new ciMonBitmap(theSetup.m_nWidth,theSetup.m_nHeight);
	if (this->backingstore == NULL) {
		esyslog("iMonLCD: unable to create framebuffer backing store");
		return -1;
	}
  backingstore->SetPixel(0,0);//make dirty

  SendCmd(this->cmd_clear_alarm);
  SendCmd(this->cmd_display_on);
	SendCmd(CMD_INIT);	/* unknown, required init command */
	SendCmd(CMD_SET_ICONS);
	/* clear the progress-bars on top and bottom of the display */
	SendCmd(CMD_SET_LINES0);
	SendCmd(CMD_SET_LINES1);
	SendCmd(CMD_SET_LINES2);

	Contrast(theSetup.m_nContrast);

	dsyslog("iMonLCD: init() done");

	return 0;
}

/**
 * Close the driver (do necessary clean-up).
 */
void ciMonLCD::close()
{
	time_t tt;
	struct tm *t;
	uint64_t data;

	if (this->imon_fd >= 0) {
		if (theSetup.m_nOnExit == eOnExitMode_SHOWMSG) {
			/*
			 * "show message" means "do nothing" - the
			 * message is there already
			 */
			isyslog("iMonLCD: closing, leaving \"last\" message.");
		} else if (theSetup.m_nOnExit == eOnExitMode_BLANKSCREEN) {
			/*
			 * turning backlight off (confirmed for my
			 * Silverstone LCD) (as "cybrmage" at
			 * mediaportal pointed out, his LCD is an
			 * Antec built-in one and turns completely
			 * off with this command)
			 */
			isyslog("iMonLCD: closing, turning backlight off.");
			SendCmd(this->cmd_shutdown);
			SendCmd(this->cmd_clear_alarm);
		} else {
			/*
			 * by default, show the big clock. We need to
			 * set it to the current time, then it just
			 * keeps counting automatically.
			 */
			isyslog("iMonLCD: closing, showing clock.");

			tt = time(NULL);
			t = localtime(&tt);

			data = this->cmd_display;
			data += ((uint64_t) t->tm_sec << 48);
			data += ((uint64_t) t->tm_min << 40);
			data += ((uint64_t) t->tm_hour << 32);
			data += ((uint64_t) t->tm_mday << 24);
			data += ((uint64_t) t->tm_mon << 16);
			data += (((uint64_t) t->tm_year) << 8);
			data += 0x80;
			SendCmd(data);
			SendCmd(this->cmd_clear_alarm);
		}

		::close(this->imon_fd);
    this->imon_fd = -1;
	}
}


/**
 * Clear the screen.
 */
void ciMonLCD::clear()
{
  if(framebuf)
    framebuf->clear();
}


/**
 * Flush data on screen to the LCD.
 */
bool ciMonLCD::flush()
{
  const int packetSize = 7;

	unsigned char msb;
	int offset = 0;

	/*
	 * The display only provides for a complete screen refresh. If
	 * nothing has changed, don't refresh.
	 */
  if ((*backingstore) == (*framebuf))
	  return true;

	/* send buffer for one command or display data */
	unsigned char tx_buf[8];

	int err;
  const uchar* fb = framebuf->getBitmap();
	int bytes = framebuf->Width() / 8 * framebuf->Height();
  

	for (msb = 0x20; msb < 0x3c; msb++) {
		/* Copy the packet data from the frame buffer. */
    int nCopy = packetSize;
    if(bytes < nCopy) {
      if(bytes > 0)
        nCopy = bytes;
      else
        nCopy = 0;
    }
    if(nCopy!=packetSize)
  		memset(tx_buf, 0xFF , packetSize);
    if(nCopy)
  		memcpy(tx_buf, fb + offset, nCopy);

		/* Add the memory register byte to the packet data. */
		tx_buf[packetSize] = msb;

    //uint64_t *v = (uint64_t*)tx_buf;
    //dsyslog("iMonLCD: writing : %08llx", *v);

  	err = write(this->imon_fd, tx_buf, sizeof(tx_buf));
    cCondWait::SleepMs(2);

  	if (err <= 0)
  		esyslog("iMonLCD: error writing to file descriptor: %d (%s)", err, strerror(errno));

		offset += packetSize;
    bytes -= packetSize;
	}

	/* Update the backing store. */
  (*backingstore) = (*framebuf);
  return (err > 0);
}


/**
 * Print a string on the screen at position (x,y).
 * The upper-left corner is (1,1), the lower-right corner is (this->width, this->height).
 * \param x        Horizontal character position (column).
 * \param y        Vertical character position (row).
 * \param string   String that gets written.
 */
int ciMonLCD::DrawText(int x, int y, const char* string)
{
  if(pFont && framebuf)
    return pFont->DrawText(framebuf, x, y, string, 1024);
  return -1;
}


/**
 * Sets the "icons state" for the device. We use this to control the icons
   * around the outside the display. 
 *
 * \param state    This symbols to display.
 */
bool ciMonLCD::icons(int state)
{
	uint64_t icon = 0x0;

	/* bit 0 : disc icon (0=off, 1='spin') */
	if ((state & eIconDiscSpin) != 0) {
    bool bSpinIcon = ((state & eIconDiscRunSpin) != 0);
    bool bSpinBackward ((state & eIconDiscSpinBackward) != 0);

		switch (bSpinIcon ? this->last_cd_state : 3) {
		case 0:
			this->last_cd_state = bSpinBackward ? 3 : 1;
			if (theSetup.m_bDiscMode == 1)
				/* all on except top & bottom */
				icon |= ((uint64_t) (255 - 128 - 8) << 40);
			else
				/* top & bottom on */
				icon |= ((uint64_t) (128 | 8) << 40);
			break;
		case 1:
			this->last_cd_state = bSpinBackward ? 0 : 2;
			if (theSetup.m_bDiscMode == 1)
				/* all on except top-right & bottom-left */
				icon |= ((uint64_t) (255 - 16 - 1) << 40);
			else
				/* top-right & bottom-left on */
				icon |= ((uint64_t) (1 | 16) << 40);
			break;
		case 2:
			this->last_cd_state = bSpinBackward ? 1 : 3;
			if (theSetup.m_bDiscMode == 1)
				/* all on except right & left */
				icon |= ((uint64_t) (255 - 32 - 2) << 40);
			else
				/* right & left on */
				icon |= ((uint64_t) (32 | 2) << 40);
			break;
		default:
			this->last_cd_state = bSpinBackward ? 2 : 0;
			if (theSetup.m_bDiscMode == 1)
				/* all on except top-left & bottom-right */
				icon |= ((uint64_t) (255 - 64 - 4) << 40);
			else
				/* top-left & bottom-right on */
				icon |= ((uint64_t) (4 | 64) << 40);
			break;
		}
	}

	/*
	 * bit 1,2,3 : top row (0=none, 1=music, 2=movie, 3=photo, 4=CD/DVD,
	 * 5=TV, 6=Web, 7=News/Weather)
	 */
	switch (state & eIconTopMask) {
		case eIconTopMusic: icon |= ICON_MUSIC;  break;
		case eIconTopMovie: icon |= ICON_MOVIE;  break;
		case eIconTopPhoto: icon |= ICON_PHOTO;  break;
		case eIconTopDVD:   icon |= ICON_CD_DVD;	break;
		case eIconTopTV:    icon |= ICON_TV;			break;
		case eIconTopWeb:   icon |= ICON_WEBCAST;break;
		case eIconTopNews:  icon |= ICON_NEWS;   break;
		default:                                      break;
	}

	/* bit 4,5 : 'speaker' icons (0=off, 1=L+R, 2=5.1ch, 3=7.1ch) */
	switch (state & eIconSpeakerMask) {
    case eIconSpeakerL:  icon |= ICON_SPKR_FL; break;
		case eIconSpeakerR:  icon |= ICON_SPKR_FR; break;
		case eIconSpeakerLR: icon |= ICON_SPKR_FL | ICON_SPKR_FR;		break;
		case eIconSpeaker51: 
      icon |= ICON_SPKR_FL | ICON_SPKR_FC | ICON_SPKR_FR | ICON_SPKR_RL | ICON_SPKR_RR;
			break;
		case eIconSpeaker71:
			icon |= ICON_SPKR_FL | ICON_SPKR_FC | ICON_SPKR_FR | ICON_SPKR_RL | ICON_SPKR_RR | ICON_SPKR_SL | ICON_SPKR_SR;
			break;
		case eIconSPDIF:
			icon |= ICON_SPKR_SPDIF;
		case eIconMute:
		default: break;
	}

	icon |= ((state & eIconSRC) != 0) ?  ICON_SRC : 0;
	icon |= ((state & eIconFIT) != 0) ?  ICON_FIT : 0;
	icon |= ((state & eIconTV) != 0) ?   ICON_TV_2 : 0;
	icon |= ((state & eIconHDTV) != 0) ? ICON_HDTV : 0;
	icon |= ((state & eIconSRC1) != 0) ? ICON_SCR1 : 0;
	icon |= ((state & eIconSRC2) != 0) ? ICON_SCR2 : 0;

	/* bottom-right icons (MP3,OGG,WMA,WAV) */
  switch (state & eIconBR_Mask) {
		case eIconBR_MP3: icon |= ICON_AUDIO_MP3; break;
		case eIconBR_OGG: icon |= ICON_AUDIO_OGG; break;
		case eIconBR_WMA: icon |= ICON_AUDIO_WMA2; break;
		case eIconBR_WAV: icon |= ICON_AUDIO_WAV; break;
		default:
			break;
	}

	/* bottom-middle icons (MPG,AC3,DTS,WMA) */
  switch (state & eIconBM_Mask) {
		case eIconBM_MPG: icon |= ICON_AUDIO_MPG; break;
		case eIconBM_AC3: icon |= ICON_AUDIO_AC3; break;
		case eIconBM_DTS: icon |= ICON_AUDIO_DTS; break;
		case eIconBM_WMA: icon |= ICON_AUDIO_WMA; break;
		default: break;
	}

	/* bottom-left icons (MPG,DIVX,XVID,WMV) */
  switch (state & eIconBL_Mask) {
		case eIconBL_MPG:   icon |= ICON_MPG;  break;
		case eIconBL_DIVX:  icon |= ICON_DIVX; break;
		case eIconBL_XVID:  icon |= ICON_XVID; break;
		case eIconBL_WMV:   icon |= ICON_WMV;  break;
		default: break;
	}

	icon |= ((state & eIconVolume) != 0)      ? ICON_VOL : 0;
	icon |= ((state & eIconTIME) != 0)        ? ICON_TIME : 0;
	icon |= ((state & eIconALARM) != 0)       ? ICON_ALARM : 0;
	icon |= ((state & eIconRecording) != 0)   ? ICON_REC : 0;
	icon |= ((state & eIconRepeat) != 0)      ? ICON_REP : 0;
	icon |= ((state & eIconShuffle) != 0)     ? ICON_SFL : 0;
	icon |= ((state & eIconDiscEllispe) != 0) ? ICON_DISK_IN : 0;

	return SendCmd(CMD_SET_ICONS | icon);
}

/**
 * Sends a command to the screen. The kernel module expects data to be
 * sent in 8 byte chunks, so for simplicity, we allow you to define
 * the data as a 64-bit integer.
 * However, we have to reverse the bytes to the order the display requires.
 *
 * \param value  The data to send. Must be in a format that is recognized by
 *               the device. The kernel module doesn't actually do validation.
 * \return  <= 0 error writing to file descriptor.
 */
bool ciMonLCD::SendCmd(const uint64_t & cmdData) {
	unsigned int i;
	unsigned char buf[8];

  //dsyslog("iMonLCD: writing : %08llx", cmdData);

	/* Fill the send buffer. */
	for (i = 0; i < sizeof(buf); i++) {
		buf[i] = (unsigned char)((cmdData >> (i * 8)) & 0xFF);
	}

	int err;
	err = write(this->imon_fd, buf, sizeof(buf));
  cCondWait::SleepMs(2);

	if (err <= 0)
		esyslog("iMonLCD: error writing to file descriptor: %d (%s)", err, strerror(errno));

  return (err > 0);
}

/**
 * Sets the contrast of the display.
 *
 * \param promille  The value the contrast is set to in promille (0 = lowest
 *                  contrast; 1000 = highest contrast).
 * \return 0 on failure, >0 on success.
 */
bool ciMonLCD::Contrast(int nContrast)
{
	if (nContrast < 0) {
		nContrast = 0;
	} else if (nContrast > 1000) {
		nContrast = 1000;
	}

	/*
	 * Send contrast normalized to the hardware-understandable-value (0
	 * to 40). 0 is the lowest and 40 is the highest. The actual
	 * perceived contrast varies depending on the type of display.
	 */
	return SendCmd(CMD_LOW_CONTRAST + (uint64_t) (nContrast / 25));
}

/**
 * Sets the length of the built-in progress-bars and lines.
 * Values from -32 to 32 are allowed. Positive values indicate that bars extend
 * from left to right, negative values indicate that the run from right to left.
 * Conventient method to simplify setting the bars with "human understandable
 * values".
 *
 * \see setBuiltinProgressBars, lengthToPixels
 *
 * \param topLine      Length of the top line (-32 to 32)
 * \param botLine      Length of the bottom line (-32 to 32)
 * \param topProgress  Length of the top progress bar (-32 to 32)
 * \param botProgress  Length of the bottom progress bar (-32 to 32)
 */
void ciMonLCD::setLineLength(int topLine, int botLine, int topProgress, int botProgress)
{
	setBuiltinProgressBars(lengthToPixels(topLine),
			       lengthToPixels(botLine),
			       lengthToPixels(topProgress),
			       lengthToPixels(botProgress)
	);
}


/**
 * Sets the length of the built-in progress-bars and lines.
 * Values from -32 to 32 are allowed. Positive values indicate that bars extend
 * from left to right, negative values indicate that the run from right to left.
 *
 * \param topLine      Pitmap of the top line
 * \param botLine      Pitmap of the bottom line
 * \param topProgress  Pitmap of the top progress bar
 * \param botProgress  Pitmap of the bottom progress bar
 */
void ciMonLCD::setBuiltinProgressBars(int topLine, int botLine,
		       int topProgress, int botProgress)
{
	/* Least sig. bit is on the right */
	uint64_t data;

	/* send bytes 1-4 of topLine and 1-3 of topProgress */
	data = (uint64_t) topLine & 0x00000000FFFFFFFFLL;
	data |= (((uint64_t) topProgress) << 8 * 4) & 0x00FFFFFF00000000LL;
	SendCmd(CMD_SET_LINES0 | data);

	/* send byte 4 of topProgress, bytes 1-4 of botProgress and 1-2 of botLine */
	data = (((uint64_t) topProgress) >> 8 * 3) & 0x00000000000000FFLL;
	data |= (((uint64_t) botProgress) << 8) & 0x000000FFFFFFFF00LL;
	data |= (((uint64_t) botLine) << 8 * 5) & 0x00FFFF0000000000LL;
	SendCmd(CMD_SET_LINES1 | data);

	/* send remaining bytes 3-4 of botLine */
	data = ((uint64_t) botLine) >> 8 * 2;
	SendCmd(CMD_SET_LINES2 | data);
}

/**
 * Maps values to corresponding pixmaps for the built-in progress bars.
 * Values from -32 to 32 are allowed. Positive values indicate that bars extend
 * from left to right, negative values indicate that they run from right to left.
 *
 * \param length The length of the bar.
 * \return The pixmap that represents the given length.
 */
int ciMonLCD::lengthToPixels(int length)
{
	int pixLen[] =
	{
		0x00, 0x00000080, 0x000000c0, 0x000000e0, 0x000000f0,
		0x000000f8, 0x000000fc, 0x000000fe, 0x000000ff,
		0x000080ff, 0x0000c0ff, 0x0000e0ff, 0x0000f0ff,
		0x0000f8ff, 0x0000fcff, 0x0000feff, 0x0000ffff,
		0x0080ffff, 0x00c0ffff, 0x00e0ffff, 0x00f0ffff,
		0x00f8ffff, 0x00fcffff, 0x00feffff, 0x00ffffff,
		0x80ffffff, 0xc0ffffff, 0xe0ffffff, 0xf0ffffff,
		0xf8ffffff, 0xfcffffff, 0xfeffffff, 0xffffffff
	};

	if (abs(length) > 32)
		return (0);

	if (length >= 0)
		return pixLen[length];
	else
		return (pixLen[32 + length] ^ 0xffffffff);
}

bool ciMonLCD::SetFont(const char *szFont) {

  ciMonFont* tmpFont = NULL;

  cString sFileName = cFont::GetFontFileName(szFont);
  if(!isempty(sFileName)) {
    tmpFont = new ciMonFont(sFileName,12,11);
  } else {
		esyslog("iMonLCD: unable to find file for font '%s'",szFont);
  }
  if(tmpFont) {
    if(pFont) {
      delete pFont;
    }
    pFont = tmpFont;
    return true;
  }
  return false;
}
