/*
 * iMON LCD plugin to VDR (C++)
 *
 * (C) 2009 Andreas Brachold <vdr07 AT deltab de>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
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
