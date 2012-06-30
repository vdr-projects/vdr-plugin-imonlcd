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

#ifndef __IMON_BITMAP_H___
#define __IMON_BITMAP_H___

class ciMonBitmap  {
  int height;
  int width;
  unsigned int bytesPerLine;
  uchar *bitmap;
protected:
  ciMonBitmap();
public:
  ciMonBitmap( int w, int h );
  
  virtual ~ciMonBitmap();
  ciMonBitmap& operator = (const ciMonBitmap& x);
  bool operator == (const ciMonBitmap& x) const;

  void clear();
  int Height() const { return height; }
  int Width() const { return width; }
  bool SetPixel(int x, int y);

  uchar * getBitmap() const { return bitmap; };
};


#endif

