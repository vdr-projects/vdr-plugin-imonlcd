/*
 * iMON LCD plugin to VDR (C++)
 *
 * (C) 2009 Andreas Brachold <vdr07 AT deltab de>
 *
 *- Glyph handling based on <vdr/font.c>
 *
 * This code is distributed under the terms and conditions of the
 * GNU GENERAL PUBLIC LICENSE. See the file COPYING for details.
 */

#include <vdr/tools.h>
#include "ffont.h"

// --- ciMonFont ---------------------------------------------------------

#define KERNING_UNKNOWN  (-10000)

ciMonGlyph::ciMonGlyph(uint CharCode, FT_GlyphSlotRec_ *GlyphData)
{
  charCode = CharCode;
  advanceX = GlyphData->advance.x >> 6;
  advanceY = GlyphData->advance.y >> 6;
  left = GlyphData->bitmap_left;
  top = GlyphData->bitmap_top;
  width = GlyphData->bitmap.width;
  rows = GlyphData->bitmap.rows;
  pitch = GlyphData->bitmap.pitch;
  bitmap = MALLOC(uchar, rows * pitch);
  memcpy(bitmap, GlyphData->bitmap.buffer, rows * pitch);
}

ciMonGlyph::~ciMonGlyph()
{
  free(bitmap);
}

int ciMonGlyph::GeciMonKerningCache(uint PrevSym) const
{
  for (int i = kerningCache.Size(); --i > 0; ) {
      if (kerningCache[i].prevSym == PrevSym)
         return kerningCache[i].kerning;
      }
  return KERNING_UNKNOWN;
}

void ciMonGlyph::SeciMonKerningCache(uint PrevSym, int Kerning)
{
  kerningCache.Append(ciMonKerning(PrevSym, Kerning));
}



ciMonFont::ciMonFont(const char *Name, int CharHeight, int CharWidth)
{
  height = 0;
  bottom = 0;
  int error = FT_Init_FreeType(&library);
  if (!error) {
     error = FT_New_Face(library, Name, 0, &face);
     if (!error) {
        if (face->num_fixed_sizes && face->available_sizes) { // fixed font
           // TODO what exactly does all this mean?
           height = face->available_sizes->height;
           for (uint sym ='A'; sym < 'z'; sym++) { // search for descender for fixed font FIXME
               FT_UInt glyph_index = FT_Get_Char_Index(face, sym);
               error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
               if (!error) {
                  error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
                  if (!error) {
                     if (face->glyph->bitmap.rows-face->glyph->bitmap_top > bottom)
                        bottom = face->glyph->bitmap.rows-face->glyph->bitmap_top;
                     }
                  else
                     esyslog("iMonLCD: FreeType: error %d in FT_Render_Glyph", error);
                  }
               else
                  esyslog("iMonLCD: FreeType: error %d in FT_Load_Glyph", error);
               }
           }
        else {
           error = FT_Set_Char_Size(face, // handle to face object
                                    CharWidth * 64,  // CharWidth in 1/64th of points
                                    CharHeight * 64, // CharHeight in 1/64th of points
                                    0,    // horizontal device resolution
                                    0);   // vertical device resolution
           if (!error) {
              height = ((face->size->metrics.ascender-face->size->metrics.descender) + 63) / 64;
              bottom = abs((face->size->metrics.descender - 63) / 64);
              }
           else
              esyslog("iMonLCD: FreeType: error %d during FT_Set_Char_Size (font = %s)\n", error, Name);
           }
        }
     else
        esyslog("iMonLCD: FreeType: load error %d (font = %s)", error, Name);
     }
  else
     esyslog("iMonLCD: FreeType: initialization error %d (font = %s)", error, Name);
}

ciMonFont::~ciMonFont()
{
  FT_Done_Face(face);
  FT_Done_FreeType(library);
}

int ciMonFont::Kerning(ciMonGlyph *Glyph, uint PrevSym) const
{
  int kerning = 0;
  if (Glyph && PrevSym) {
     kerning = Glyph->GeciMonKerningCache(PrevSym);
     if (kerning == KERNING_UNKNOWN) {
        FT_Vector delta;
        FT_UInt glyph_index = FT_Get_Char_Index(face, Glyph->CharCode());
        FT_UInt glyph_index_prev = FT_Get_Char_Index(face, PrevSym);
        FT_Get_Kerning(face, glyph_index_prev, glyph_index, FT_KERNING_DEFAULT, &delta);
        kerning = delta.x / 64;
        Glyph->SeciMonKerningCache(PrevSym, kerning);
        }
     }
  return kerning;
}

ciMonGlyph* ciMonFont::Glyph(uint CharCode) const
{
  // Non-breaking space:
  if (CharCode == 0xA0)
     CharCode = 0x20;

  // Lookup in cache:
  cList<ciMonGlyph> *glyphCache = &glyphCacheMonochrome;
  for (ciMonGlyph *g = glyphCache->First(); g; g = glyphCache->Next(g)) {
      if (g->CharCode() == CharCode)
         return g;
      }

  FT_UInt glyph_index = FT_Get_Char_Index(face, CharCode);

  // Load glyph image into the slot (erase previous one):
  int error = FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT);
  if (error)
     esyslog("iMonLCD: FreeType: error during FT_Load_Glyph");
  else {
     error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_MONO);
     if (error)
        esyslog("iMonLCD: FreeType: error during FT_Render_Glyph %d, %d\n", CharCode, glyph_index);
     else { //new bitmap
        ciMonGlyph *Glyph = new ciMonGlyph(CharCode, face->glyph);
        glyphCache->Add(Glyph);
        return Glyph;
        }
     }
#define UNKNOWN_GLYPH_INDICATOR '?'
  if (CharCode != UNKNOWN_GLYPH_INDICATOR)
     return Glyph(UNKNOWN_GLYPH_INDICATOR);
  return NULL;
}

int ciMonFont::Width(uint c) const
{
  ciMonGlyph *g = Glyph(c);
  return g ? g->AdvanceX() : 0;
}

int ciMonFont::Width(const char *s) const
{
  int w = 0;
  if (s) {
     uint prevSym = 0;
     while (*s) {
           int sl = Utf8CharLen(s);
           uint sym = Utf8CharGet(s, sl);
           s += sl;
           ciMonGlyph *g = Glyph(sym);
           if (g)
              w += g->AdvanceX() + Kerning(g, prevSym);
           prevSym = sym;
           }
     }
  return w;
}

int ciMonFont::DrawText(ciMonBitmap *Bitmap, int x, int y, const char *s, int Width) const
{
  if (s && height) { // checking height to make sure we actually have a valid font
     uint prevSym = 0;
     while (*s) {
           int sl = Utf8CharLen(s);
           uint sym = Utf8CharGet(s, sl);
           s += sl;
           ciMonGlyph *g = Glyph(sym);
           if (!g)
              continue;
           int kerning = Kerning(g, prevSym);
           prevSym = sym;
           uchar *buffer = g->Bitmap();
           int symWidth = g->Width();
           if (Width && x + symWidth + g->Left() + kerning - 1 > Width)
              return 1; // we don't draw partial characters
           if (x + symWidth + g->Left() + kerning > 0) {
              for (int row = 0; row < g->Rows(); row++) {
                  for (int pitch = 0; pitch < g->Pitch(); pitch++) {
                      uchar bt = *(buffer + (row * g->Pitch() + pitch));
                      for (int col = 0; col < 8 && col + pitch * 8 <= symWidth; col++) {
                         if (bt & 0x80)
                            Bitmap->SetPixel(x + col + pitch * 8 + g->Left() + kerning,
                                             y + row + (height - Bottom() - g->Top()));
                         bt <<= 1;
                         }
                      }
                  }
              }
           x += g->AdvanceX() + kerning;
           if (x > Bitmap->Width() - 1)
              return 1;
           }
        return 0;
     }
  return -1;
}


