/************************************************************************/
/*                                                                      */
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     language.C						*/
/*  Version:  1.00							*/
/*  LastEdit: 26aug2011							*/
/*                                                                      */
/*  (c) Copyright 2010,2011 Ralf Brown/Carnegie Mellon University	*/
/*      This program is free software; you can redistribute it and/or   */
/*      modify it under the terms of the GNU General Public License as  */
/*      published by the Free Software Foundation, version 3.           */
/*                                                                      */
/*      This program is distributed in the hope that it will be         */
/*      useful, but WITHOUT ANY WARRANTY; without even the implied      */
/*      warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR         */
/*      PURPOSE.  See the GNU General Public License for more details.  */
/*                                                                      */
/*      You should have received a copy of the GNU General Public       */
/*      License (file COPYING) along with this program.  If not, see    */
/*      http://www.gnu.org/licenses/                                    */
/*                                                                      */
/************************************************************************/

#include <ctype.h>
#include <iostream>
#include <stdint.h>
#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <unistd.h>
#include "language.h"
#include "charset.h"

using namespace std ;

/************************************************************************/
/*	Global data for this module					*/
/************************************************************************/

static const char *ranges[] =
   {
      "Auto:Auto 1-0xFFFF",
      "ASCII:English 9,32,33,36-41,44-59,65-95,97-122",
      "Latin-1:English 9,32,33,36-41,44-59,65-95,97-122",
      "Latin-1:Euro 9,32,33,36-41,44-59,65-95,97-122,160-163,165,186,192-255",
      "Latin-2:English 9,32,33,36-41,44-59,65-95,97-122",
      "Latin-2:Euro 9,32,33,36-41,44-59,65-95,97-122,192-255",
      "Unicode:English 9,32,33,36-41,44-59,65-95,97-122",
      "Unicode:Euro 9,32,33,36-41,44-59,65-95,97-122,160-163,165,186,192-255,0x0100-0x024F,0x1E00-0x1EFF,0x2C60-0x2C7F,0xA720-0xA7FF",
      "Unicode:CJKV 9,32,33,44-59,0x2E80-0x2EFF,0x2FF0-0x2FFF,0x31C0-0x31EF,0x3300-0x4DBF,0x4E00-0x9FFF,0xFF00-x0FFEF,0x20000-0x2A6DF",
      "Unicode:Aboriginal 9,32,33,44-59,0x1400-0x167F",
      "Unicode:Arabic 9,32,33,44-59,0x0600-0x06FF,0x0750-0x077F,0xFB50-0xFDFF,0xFE70-x0FEFF",
      "Unicode:Armenian 9,32,33,44-59,0x0530-0x058F",
      "Unicode:Balinese 9,32,33,44-59,0x1B00-0x1B7F",
      "Unicode:Bengali 9,32,33,44-59,0x0980-0x09FF",
      "Unicode:Chinese 9,32,33,44-59,0x3100-0x312F,0x31A0-0x31BF,0x31C0-0x31EF,0x3300-0x4DBF,0x4E00-0x9FFF,0xFF00-0xFFEF,0x20000-0x2A6DF",
      "Unicode:Buginese 9,32,33,44-59,0x1A00-0x1A1F",
      "Unicode:Buhid 9,32,33,44-59,0x13A0-0x13FF",
      "Unicode:Coptic 9,32,33,44-59,0x0370-0x03FF,0x2C80-0x2CFF",
      "Unicode:Cyrillic 9,32,33,44-59,0x0400-0x04FF,0x0500-0x052F",
      "Unicode:Devanagari 9,32,33,44-59,0x0900-0x097F",
      "Unicode:Ethiopic 9,32,33,44-59,0x1200-0x139F,0x2D80-0x2DDF",
      "Unicode:Georgian 9,32,33,44-59,0x10A0-0x10FF,0x2D00-0x2D2F",
      "Unicode:Glagolitic 9,32,33,44-59,0x2C00-0x2C5F",
      "Unicode:Greek 9,32,33,44-59,0x0370-0x03FF,0x1F00-0x1FFF",
      "Unicode:Gujarati 9,32,33,44-59,0x0A80-0xAFF",
      "Unicode:Gurmukhi 9,32,33,44-59,0x0A00-0x0A7F",
      "Unicode:Hangul 9,32,33,44-59,0x1100-0x11FF,0x3130-0x318F,0xAC00-0xD7AF",
      "Unicode:Hebrew 9,32,33,44-59,0x0590-0x05FF",
      "Unicode:Japanese 9,32,33,44-59,0x3040-0x30FF,0x3190-0x319F,0x31F0-0x31FF",
      "Unicode:Kannada 9,32,33,44-59,0x0C80-0x0CFF",
      "Unicode:Khmer 9,32,33,44-59,0x1780-0x17FF,0x19E0-0x19FF",
      "Unicode:Korean 9,32,33,44-59,0x1100-0x11FF,0x3130-0x318F,0xAC00-0xD7AF",
      "Unicode:Lao 9,32,33,44-59,0x0E80-0x0EFF",
      "Unicode:Limbu 9,32,33,44-59,0x1900-0x194F",
      "Unicode:Malayalam 9,32,33,44-59,0x0D00-0xD7F",
      "Unicode:Mongolian 9,32,33,44-59,0x1800-0x18AF",
      "Unicode:Myanmar 9,32,33,44-59,0x1000-0x109F",
      "Unicode:Ogham 9,32,33,44-59,0x1680-0x169F",
      "Unicode:Runic 9,32,33,44-59,0x16A0-0x16FF",
      "Unicode:Sinhala 9,32,33,44-59,0x0D80-0x0DFF",
      "Unicode:Syriac 9,32,33,44-59,0x0700-0x074F",
      "Unicode:Tagalog 9,32,33,44-59,65-93,97-122,0x1700-0x171F",
      "Unicode:Tamil 9,32,33,44-59,0x0B80-0x0BFF",
      "Unicode:Telugu 9,32,33,44-59,0x0C00-0x0C7F",
      "Unicode:Thai 9,32,33,44-59,0x0E00-0x0E7F",
      "Unicode:Tibetan 9,32,33,44-59,0x0F00-0x0FFF",
      "EUC:English 9,32,33,36-41,44-59,65-95,97-122",
      "EUC:Asian 9,32,33,36-41,44-59,65-95,97-122,128-99999",
      "EUC-JP:English 9,32,33,36-41,44-59,65-95,97-122",
      "EUC-JP:Japanese 9,32,33,36-41,44-59,128-99999",
      "EUC-TW:English 9,32,33,36-41,44-59,65-95,97-122",
      "EUC-TW:Taiwanese 9,32,33,36-41,44-59,128-99999",
      "ShiftJIS:Japanese 9,32,33,36-41,44-59,128-99999",
      "KOI7:English 9,32,33,36-41,44-59,65-95",
      "KOI7:Russian 9,32,33,36-41,44-59,96-126",
      "KOI8-R:English 9,32,33,36-41,44-59,65-95,97-122",
      "KOI8-R:Russian 9,32,33,36-41,44-59,0xA3,0xB3,0xC0-0xFF",
      "KOI8-U:English 9,32,33,36-41,44-59,65-95,97-122",
      "KOI8-U:Russian 9,32,33,36-41,44-59,0xA3,0xB3,0xC0-0xFF",
      "KOI8-U:Ukrainian 9,32,33,36-41,44-59,0xA3,0xA4,0xA6,0xA7,0xAD,0xB3,0xB4,0xB6,0xB7,0xBD,0xC0-0xFF",
      "CP1251:English 9,32,33,36-41,44-59,65-95,97-122",
      "CP1251:Cyrillic 9,32,33,36-41,44-59,0x80,0x81,0x83,0x8A,0x8C-0x90,0x9a,0x9C-0x9F,0xA1-0xA3,0xA5,0xA8,0xAA,0xAF,0xB2-0xB4,0xB8,0xBA,0xBC-0xFF",
      "CP1251:Russian 9,32,33,36-41,44-59,0xA1-0xA3,0xA5,0xA8,0xAA,0xAF,0xB2-0xB4,0xB8,0xBA,0xBC-0xFF",
      "TIS-620:English 9,32,33,36-41,44-59,65-95,97-122",
      "TIS-620:Thai 9,32,33,36-41,44-59,0xA1-0xDA,0xDF-0xFB",
      "TSCII:English 9,32,33,36-41,44-59,65-95,97-122",
      "TSCII:Tamil 9,32,33,36-41,44-59,0x80-0x91,0x95-0xAC,0xAE-0xFE",
      "VISCII:English 9,32,33,36-41,44-59,65-95,97-122",
      "VISCII:Vietnamese 2,5,6,9,20,25,30,32,33,36-41,44-59,0x80-0xFF",
      "GB-2312:English 9,32,33,36-41,44-59,65-95,97-122",
      "GB-2312:Chinese 9,32,33,36-41,44-59,128-99999",
      "GBK:English 9,32,33,36-41,44-59,65-95,97-122",
      "GBK:Chinese 9,32,33,36-41,44-59,128-99999",
      "Big5:English 9,32,33,36-41,44-59,65-95,97-122",
      "Big5:Chinese 9,32,33,36-41,44-59,128-99999",
      0
   } ;

/************************************************************************/
/*	Helper functions						*/
/************************************************************************/

static size_t numwords(size_t points)
{
   return (points + 31) / 32 ;
}

//----------------------------------------------------------------------

static size_t wordnumber(size_t point)
{
   return point / 32 ;
}

//----------------------------------------------------------------------

static size_t bitnumber(size_t point)
{
   return point % 32 ;
}

//----------------------------------------------------------------------

static uint32_t bitmask(size_t point)
{
   return 1UL << bitnumber(point) ;
}

//----------------------------------------------------------------------

static bool list_known_language(const char *encoding, const char *range_spec,
				bool add_comma, int &column)
{
   while (*range_spec && isspace(*range_spec))
      range_spec++ ;
   unsigned enc_len = strlen(encoding) ;
   if (strncasecmp(encoding,range_spec,enc_len) != 0 ||
       (range_spec[enc_len] != ':' && !isspace(range_spec[enc_len])))
      return false ;  // encoding does not match
   range_spec += enc_len ;
   if (*range_spec == ':')
      range_spec++ ;
   while (*range_spec && isspace(*range_spec))
      range_spec++ ;
   if (add_comma)
      {
      cerr << ',' ;
      column++ ;
      }
   if (column < 65)
      {
      cerr << ' ' ;
      column++ ;
      }
   else
      {
      cerr << endl << "\t" ;
      column = 8 ;
      }
   while (*range_spec && !isspace(*range_spec))
      {
      cerr << *range_spec ;
      range_spec++ ;
      column++ ;
      }
   return true ;
}

//----------------------------------------------------------------------

static const char *set_valid_range(const char *range_spec,
				   CodePoints *cp)
{
   bool bad_spec = false ;
   if (range_spec)
      {
      while (*range_spec && isspace(*range_spec))
	 range_spec++ ;
      if (!*range_spec)
	 return range_spec ;
      char *end ;
      unsigned long first = strtoul(range_spec,&end,0) ;
      if (end != range_spec)
	 {
	 range_spec = end ;
	 unsigned long last = first ;
	 while (*range_spec && isspace(*range_spec))
	    range_spec++ ;
	 if (*range_spec == '-')
	    {
	    range_spec++ ;
	    while (*range_spec && isspace(*range_spec))
	       range_spec++ ;
	    last = strtoul(range_spec,&end,0) ;
	    if (end == range_spec)
	       {
	       bad_spec = true ;
	       last = first ;
	       }
	    else
	       range_spec = end ;
	    }
	 for (unsigned long i = first ; i <= last ; i++)
	    {
	    cp->setPoint((wchar_t)i) ;
	    }
	 while (*range_spec && isspace(*range_spec))
	    range_spec++ ;
	 if (*range_spec == ',')
	    range_spec++ ;
	 else if (*range_spec && !isdigit(*range_spec))
	    bad_spec = true ;
	 }
      }
   if (bad_spec)
      {
      // oops, invalid spec, skip to end
      cerr << "Error in language range specification near\n\t"
	   << range_spec << endl << endl ;
      range_spec = strchr(range_spec,'\0') ;
      }
   return range_spec ;
}

//----------------------------------------------------------------------

static void set_valid_ranges(const char *range_spec, CodePoints *cp)
{
   while (*range_spec && isspace(*range_spec))
      range_spec++ ;
   while (*range_spec)
      {
      range_spec = set_valid_range(range_spec,cp) ;
      }
   return ;
}

//----------------------------------------------------------------------

static void list_known_languages(const char *encoding)
{
   bool add_comma = false ;
   cerr << "       " ;
   int column = 7 ;
   for (int i = 0 ; ranges[i] ; i++)
      {
      if (list_known_language(encoding,ranges[i],add_comma,column))
	 add_comma = true ;
      }
   return ;
}

/************************************************************************/
/*	Methods for class CodePoints					*/
/************************************************************************/

CodePoints::CodePoints(size_t numpoints)
{
   m_points = (uint32_t*)calloc(sizeof(uint32_t),numwords(numpoints)) ;
   if (m_points)
      m_numpoints = numpoints ;
   return ;
}

//----------------------------------------------------------------------

CodePoints::CodePoints(const CharacterSet *set, const char *language)
{
   size_t numpoints = set->encodingSize() ;
   m_points = (uint32_t*)calloc(sizeof(uint32_t),numwords(numpoints)) ;
   if (m_points)
      {
      m_numpoints = numpoints ;
      bool ranges_set = false ;
      for (int i = 0 ; ranges[i] ; i++)
	 {
	 if (setValidPoints(set->encoding(),language,ranges[i]))
	    {
	    ranges_set = true ;
	    break ;
	    }
	 }
      if (!ranges_set)
	 {
	 cerr << "Language '" << language
	      << "' not known for specified encoding." << endl ;
	 cerr << "Known languages are:" << endl ;
	 list_known_languages(set->encoding()) ;
	 cerr << endl ;
	 exit(1) ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

CodePoints::CodePoints(const CharacterSet *set, const char *language_spec,
		       bool is_filename)
{
   size_t numpoints = set->encodingSize() ;
   m_points = (uint32_t*)calloc(sizeof(uint32_t),numwords(numpoints)) ;
   if (m_points)
      {
      m_numpoints = numpoints ;
      if (is_filename)
	 {
	 FILE *fp = fopen(language_spec,"r") ;
	 char line[16384] ;
	 if (fp && fgets(line,sizeof(line),fp) != 0)
	    {
	    set_valid_ranges(line,this) ;
	    }
	 else
	    {
	    cerr << "Unable to read '" << language_spec
		 << "' to get desired character ranges." << endl ;
	    }
	 if (fp)
	    fclose(fp) ;
	 }
      else
	 {
	 set_valid_ranges(language_spec,this) ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

CodePoints::~CodePoints()
{
   if (m_points)
      free(m_points) ;
   m_points = 0 ;
   m_numpoints = 0 ;
   return ;
}

//----------------------------------------------------------------------

bool CodePoints::setValidPoints(const char *encoding, const char *language,
				const char *range_spec)
{
   if (encoding && language && range_spec && *range_spec)
      {
      bool auto_language = strcasecmp(language,"Auto") == 0 ;
      bool auto_encoding = strcasecmp(encoding,"Auto") == 0 ;
      while (*range_spec && isspace(*range_spec))
	 range_spec++ ;
      unsigned enc_len = strlen(encoding) ;
      if (!auto_language &&
	  (strncasecmp(encoding,range_spec,enc_len) != 0 ||
	   (range_spec[enc_len] != ':' && !isspace(range_spec[enc_len]))))
	 return false ;  // encoding does not match
      while (*range_spec && *range_spec != ':')
	 range_spec++ ;
      if (*range_spec == ':')
	 range_spec++ ;
      while (*range_spec && isspace(*range_spec))
	 range_spec++ ;
      unsigned lang_len = strlen(language) ;
      if (!auto_encoding &&
	  (strncasecmp(language,range_spec,lang_len) != 0 ||
	   !isspace(range_spec[lang_len])))
	 return false ;  // language does not match
      range_spec += lang_len ;
      set_valid_ranges(range_spec,this) ;
      return true ;
      }
   return false ;
}

//----------------------------------------------------------------------

void CodePoints::setPoint(size_t cp)
{
   if (cp < m_numpoints)
      m_points[wordnumber(cp)] |= bitmask(cp) ;
   return ;
}

//----------------------------------------------------------------------

void CodePoints::clearPoint(size_t cp)
{
   if (cp < m_numpoints)
      m_points[wordnumber(cp)] &= ~bitmask(cp) ;
   return ;
}

//----------------------------------------------------------------------

bool CodePoints::validPoint(size_t cp) const
{
   if (cp >= m_numpoints)
      return true ;			// don't know about it, so say it's OK
   return (m_points[wordnumber(cp)] & bitmask(cp)) != 0 ;
}

// end of file language.C //
