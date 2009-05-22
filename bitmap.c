/*
 * iMON LCD plugin to VDR (C++)
 *
 * (C) 2009 Andreas Brachold <vdr07 AT deltab de>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 *
 */

#include <vdr/tools.h>
#include "bitmap.h"

ciMonBitmap::ciMonBitmap(int w, int h) {
  width = w;
  height = h;

  // lines are byte aligned
  bytesPerLine = (width + 7) / 8;

 	bitmap = MALLOC(uchar, bytesPerLine * height);
  clear();
}

ciMonBitmap::ciMonBitmap() {
  height = 0;
  width = 0;
  bitmap = NULL;
}

ciMonBitmap::~ciMonBitmap() {

  if(bitmap)  
    free(bitmap);
  bitmap = NULL;
}

ciMonBitmap& ciMonBitmap::operator = (const ciMonBitmap& x) {

  if(height != x.height
    || width != x.width
    || bitmap == NULL) {
  
    if(bitmap)  
      free(bitmap);
    bitmap = NULL;

    height = x.height;
    width  = x.width;

    bytesPerLine = (width + 7) / 8;

    if(height && width)
    	bitmap = MALLOC(uchar, bytesPerLine * height);
  }
  if(x.bitmap)
  	memcpy(bitmap, x.bitmap, bytesPerLine * height);
  return *this;
}

bool ciMonBitmap::operator == (const ciMonBitmap& x) const {

  if(height != x.height
    || width != x.width
    || bitmap == NULL
    || x.bitmap == NULL)
    return false;
	return ((memcmp(x.bitmap, bitmap, bytesPerLine * height)) == 0);
}


void ciMonBitmap::clear() {
    if (bitmap)
      memset(bitmap, 0x00, bytesPerLine * height);
}

bool ciMonBitmap::SetPixel(int x, int y)
{
    unsigned char c;
    unsigned int n;

    if (!bitmap)
        return false;

    if (x >= width || x < 0)
        return false;
    if (y >= height || y < 0)
        return false;

    n = x + ((y / 8) * width);
    c = 0x80 >> (y % 8);

    if(n >= (bytesPerLine * height))
        return false;

    bitmap[n] |= c;
    return true;
}

