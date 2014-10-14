/************************************************************************/
/*                                                                      */
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     charset.C							*/
/*  Version:  1.23							*/
/*  LastEdit: 23aug2013							*/
/*                                                                      */
/*  (c) Copyright 2010,2011,2012,2013					*/
/*		 Ralf Brown/Carnegie Mellon University			*/
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

#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include "charset.h"
#include "langident/roman.h"
#include "FramepaC.h"

using namespace std ;

/************************************************************************/
/************************************************************************/

#ifndef lengthof
#  define lengthof(x) (sizeof(x)/sizeof((x)[0]))
#endif /* lengthof */

/************************************************************************/
/*	Types local to this module					*/
/************************************************************************/

struct create
   {
   // no members
   public:
      virtual CharacterSet* makeSet(const char *name, 
				    const char *enc) const  = 0 ;
   } ;

template <typename T>
struct create_set : public create
   {
   public:
      virtual CharacterSet* makeSet(const char *name, const char *enc) const
	 { return new T(name,enc) ; }
   } ;

//----------------------------------------------------------------------

struct CharacterSetDescription
   {
   public:
      const char *name ;
      const char *shortname ;
      const char *enc_class ;
      create     *creator ;
   } ;

//----------------------------------------------------------------------

class CharacterSetASCII : public CharacterSet
   {
   public:
      CharacterSetASCII(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 256 ; }
      virtual int consumeNewlines(const unsigned char *s, size_t buflen) const ;
      // as a tie-breaker when requested to extract both ASCII and multi-byte
      //   character sets and we encounter a pure ASCII string, give ASCII
      //   a tiny edge
      virtual double detectionReliability() const { return 1.00001 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetLatin1 : public CharacterSetASCII
   {
   public:
      CharacterSetLatin1(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetLatin2 : public CharacterSetLatin1
   {
   public:
      CharacterSetLatin2(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
   } ;

//----------------------------------------------------------------------

class CharacterSetISO8859_5 : public CharacterSetLatin1
   {
   public:
      CharacterSetISO8859_5(const char *name, const char *enc)
	 : CharacterSetLatin1(name,enc) {}
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
   } ;

//----------------------------------------------------------------------

class CharacterSetISO8859_6 : public CharacterSetASCII
   {
   public:
      CharacterSetISO8859_6(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetISO8859_7 : public CharacterSetASCII
   {
   public:
      CharacterSetISO8859_7(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetISO8859_8 : public CharacterSetASCII
   {
   public:
      CharacterSetISO8859_8(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

// Latin-6 (ISO-8859-10) is Latin-1 with a bunch of high-bit characters
//   substituted
class CharacterSetLatin6 : public CharacterSetLatin1
   {
   public:
      CharacterSetLatin6(const char *name, const char *enc)
	 : CharacterSetLatin1(name,enc) {}
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
   } ;

//----------------------------------------------------------------------

// Latin-7 (ISO-8859-13 "Baltic Rim")
class CharacterSetLatin7 : public CharacterSetASCII
   {
   public:
      CharacterSetLatin7(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
   } ;

//----------------------------------------------------------------------

// Latin-10 (ISO-8859-15) is Latin-1 with a handful of high-bit characters
//   substituted to make it more suitable to Western European languages
class CharacterSetLatin10 : public CharacterSetLatin1
   {
   public:
      CharacterSetLatin10(const char *name, const char *enc)
	 : CharacterSetLatin1(name,enc) {}
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
   } ;

//----------------------------------------------------------------------

class CharacterSetASCII16BE : public CharacterSetASCII
   {
   public:
      CharacterSetASCII16BE(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 256 ; }
      virtual bool filterNUL() const { return true ; }
      virtual bool bigEndian() const { return true ; }
      virtual unsigned alignment() const { return 2 ; }
      virtual int consumeNewlines(const unsigned char *s, size_t buflen) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetASCII16LE : public CharacterSetASCII
   {
   public:
      CharacterSetASCII16LE(const char *name, const char *enc) ;
      virtual bool filterNUL() const { return true ; }
      virtual unsigned alignment() const { return 2 ; }
      virtual int consumeNewlines(const unsigned char *s, size_t buflen) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetASCII32BE : public CharacterSetASCII16BE
   {
   public:
      CharacterSetASCII32BE(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool filterNUL() const { return true ; }
      virtual bool bigEndian() const { return true ; }
      virtual unsigned alignment() const { return 4 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetASCII32LE : public CharacterSetASCII16LE
   {
   public:
      CharacterSetASCII32LE(const char *name, const char *enc) ;
      virtual bool filterNUL() const { return true ; }
      virtual unsigned alignment() const { return 4 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetUnicodeBE : public CharacterSet
   {
   public:
      CharacterSetUnicodeBE(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 65536 ; }
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
      virtual double detectionReliability() const { return 0.5 ; }
      virtual bool bigEndian() const { return true ; }
      virtual unsigned alignment() const { return 2 ; }
      virtual bool romanizable(const unsigned char *s, size_t buflen) const ;
      virtual int consumeNewlines(const unsigned char *s, size_t buflen) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetUnicodeLE : public CharacterSet
   {
   public:
      CharacterSetUnicodeLE(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 65536 ; }
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
      virtual double detectionReliability() const { return 0.5 ; }
      virtual unsigned alignment() const { return 2 ; }
      virtual bool romanizable(const unsigned char *s, size_t buflen) const ;
      virtual int consumeNewlines(const unsigned char *s, size_t buflen) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetUTF8 : public CharacterSet
   {
   public:
      CharacterSetUTF8(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 17 * 65536 ; }
      virtual bool romanizable(const unsigned char *s, size_t buflen) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetUTF8Ext : public CharacterSetUTF8
   {
   public:
      CharacterSetUTF8Ext(const char *name, const char *enc) ;
      //actually permits 31-bit codes, but we can't store that many, so just
      //  re-use the standard UTF-8 size
      //virtual size_t encodingSize() const { return 17 * 65536 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetUTF32BE : public CharacterSetUTF8
   {
   public:
      CharacterSetUTF32BE(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
      virtual double detectionReliability() const { return 0.8 ; }
      virtual bool bigEndian() const { return true ; }
      virtual unsigned alignment() const { return 4 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetUTF32LE : public CharacterSetUTF8
   {
   public:
      CharacterSetUTF32LE(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
      virtual double detectionReliability() const { return 0.8 ; }
      virtual unsigned alignment() const { return 4 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetUTF_EBCDIC : public CharacterSetUTF8
   {
   public:
      CharacterSetUTF_EBCDIC(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetISO2022 : public CharacterSetASCII
   {
   public:
      CharacterSetISO2022(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetEUC : public CharacterSet
   {
   public:
      CharacterSetEUC(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 128 + (128 * 128) ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetEUC_JP : public CharacterSet
   {
   public:
      CharacterSetEUC_JP(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 128 + (128 * 128) ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetEUC_TW : public CharacterSet
   {
   public:
      CharacterSetEUC_TW(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const
	 { return 128 + 17 * ((0xFE - 0xA0) * (0xFE - 0xA0)) ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetShiftJIS : public CharacterSet
   {
   public:
      CharacterSetShiftJIS(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 128 + (128 * 128) ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetUTF7 : public CharacterSetASCII
   {
   public:
      CharacterSetUTF7(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 17 * 256 ; }
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetHZ : public CharacterSetASCII
   {
   public:
      CharacterSetHZ(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 128 + (94 * 94) ; }
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetAscii85 : public CharacterSetASCII
   {
   public:
      CharacterSetAscii85(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 17 * 256 ; }
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetKOI7 : public CharacterSetASCII
   {
   public:
      CharacterSetKOI7(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetKOI8R : public CharacterSetASCII
   {
   public:
      CharacterSetKOI8R(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetKOI8U : public CharacterSetKOI8R
   {
   public:
      CharacterSetKOI8U(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetCP437 : public CharacterSetASCII
   {
   public:
      CharacterSetCP437(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetCP737 : public CharacterSetCP437
   {
   public:
      CharacterSetCP737(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetCP862 : public CharacterSetCP437
   {
   public:
      CharacterSetCP862(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetCP866 : public CharacterSetCP437
   {
   public:
      CharacterSetCP866(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetRUSCII : public CharacterSetCP866
   {
   public:
      CharacterSetRUSCII(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetKamenicky : public CharacterSetCP437
   {
   public:
      CharacterSetKamenicky(const char *name, const char *enc)
	 : CharacterSetCP437(name,enc) {}
      virtual bool isAlphaNum(wchar_t codepoint) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetMazovia : public CharacterSetCP437
   {
   public:
      CharacterSetMazovia(const char *name, const char *enc)
	 : CharacterSetCP437(name,enc) {}
      virtual bool isAlphaNum(wchar_t codepoint) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetMIK : public CharacterSetASCII
   {
   public:
      CharacterSetMIK(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetIranSystem : public CharacterSetASCII
   {
   public:
      CharacterSetIranSystem(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetCP1251 : public CharacterSetKOI8R
   {
   public:
      CharacterSetCP1251(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetCP1252 : public CharacterSetLatin1
   {
   public:
      CharacterSetCP1252(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
   } ;

//----------------------------------------------------------------------

class CharacterSetCP1255 : public CharacterSetISO8859_8
   {
   public:
      CharacterSetCP1255(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetCP1256 : public CharacterSetASCII
   {
   public:
      CharacterSetCP1256(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetArmSCII8 : public CharacterSetLatin1
   {
   public:
      CharacterSetArmSCII8(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
   } ;

//----------------------------------------------------------------------

class CharacterSetMacCyrillic : public CharacterSetLatin1
   {
   public:
      CharacterSetMacCyrillic(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
   } ;

//----------------------------------------------------------------------

class CharacterSetTIS620 : public CharacterSetASCII
   {
   public:
      CharacterSetTIS620(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetTSCII : public CharacterSetASCII
   {
   public:
      CharacterSetTSCII(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetISCII : public CharacterSetASCII
   {
   public:
      CharacterSetISCII(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat = OF_UTF8) ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetVISCII : public CharacterSetASCII
   {
   public:
      CharacterSetVISCII(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetGB2312 : public CharacterSet
   {
   public:
      CharacterSetGB2312(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 128 + (128 * 128) ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetGBKlevel1 : public CharacterSet
   {
   public:
      CharacterSetGBKlevel1(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 128 + (128 * 192) ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetGBKlevel2 : public CharacterSetGBKlevel1
   {
   public:
      CharacterSetGBKlevel2(const char *name, const char *enc) ;
   } ;

//----------------------------------------------------------------------

class CharacterSetGBKlevel3 : public CharacterSetGBKlevel2
   {
   public:
      CharacterSetGBKlevel3(const char *name, const char *enc) ;
   } ;

//----------------------------------------------------------------------

class CharacterSetGBK : public CharacterSetGBKlevel3
   {
   public:
      CharacterSetGBK(const char *name, const char *enc) ;
   } ;

//----------------------------------------------------------------------

class CharacterSetGB18030 : public CharacterSet
   {
   public:
      CharacterSetGB18030(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 128 + (128 * 128) ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetBig5 : public CharacterSet
   {
   public:
      CharacterSetBig5(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      // character set is really only 128 + 89 * 156, but it's easier
      //   if we just use the range of the extended Big5 set so that
      //   we can share code
      virtual size_t encodingSize() const { return 128 + (126 * 156) ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetBig5Ext : public CharacterSetBig5
   {
   public:
      CharacterSetBig5Ext(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
   } ;

//----------------------------------------------------------------------

class CharacterSetISO6937 : public CharacterSet
   {
   public:
      CharacterSetISO6937(const char *name, const char *enc) ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual size_t encodingSize() const { return 256 + (15 * 64) ; }
   } ;

//----------------------------------------------------------------------

class CharacterSetGEOSTD8 : public CharacterSetASCII
   {
   public:
      CharacterSetGEOSTD8(const char *name, const char *enc) ;
      virtual bool isAlphaNum(wchar_t codepoint) const ;
      virtual double detectionReliability() const { return 1.0 ; }
   } ;

/************************************************************************/
/*	Global data for this module					*/
/************************************************************************/

static FrMutex iconv_mutex ;
static FrCriticalSection cache_critsect ;

static const char *default_encoding = "ASCII" ;

static create_set<CharacterSetASCII> create_ASCII ;
static create_set<CharacterSetLatin1> create_Latin1 ;
static create_set<CharacterSetLatin1> create_Latin2 ;
static create_set<CharacterSetASCII16BE> create_ASCII16BE ;
static create_set<CharacterSetASCII16LE> create_ASCII16LE ;
static create_set<CharacterSetASCII32BE> create_ASCII32BE ;
static create_set<CharacterSetASCII32LE> create_ASCII32LE ;
static create_set<CharacterSetUnicodeBE> create_UnicodeBE ;
static create_set<CharacterSetUnicodeLE> create_UnicodeLE ;
static create_set<CharacterSetUTF8> create_UTF8 ;
static create_set<CharacterSetUTF8> create_UTF8Ext ;
static create_set<CharacterSetUTF32BE> create_UTF32BE ;
static create_set<CharacterSetUTF32LE> create_UTF32LE ;
static create_set<CharacterSetUTF_EBCDIC> create_UTF_EBCDIC ;
static create_set<CharacterSetISO8859_5> create_ISO8859_5 ;
static create_set<CharacterSetISO8859_6> create_ISO8859_6 ;
static create_set<CharacterSetISO8859_7> create_ISO8859_7 ;
static create_set<CharacterSetISO8859_8> create_ISO8859_8 ;
static create_set<CharacterSetLatin6> create_ISO8859_10 ;
static create_set<CharacterSetLatin7> create_ISO8859_13 ;
static create_set<CharacterSetLatin10> create_ISO8859_15 ;
static create_set<CharacterSetISO2022> create_ISO2022 ;
static create_set<CharacterSetEUC> create_EUC ;
static create_set<CharacterSetEUC_JP> create_EUC_JP ;
static create_set<CharacterSetEUC_TW> create_EUC_TW ;
static create_set<CharacterSetShiftJIS> create_ShiftJIS ;
static create_set<CharacterSetAscii85> create_Ascii85 ;
static create_set<CharacterSetHZ> create_HZ ;
static create_set<CharacterSetUTF7> create_UTF7 ;
static create_set<CharacterSetKOI7> create_KOI7 ;
static create_set<CharacterSetKOI8R> create_KOI8R ;
static create_set<CharacterSetKOI8U> create_KOI8U ;
static create_set<CharacterSetCP437> create_CP437 ;
static create_set<CharacterSetCP737> create_CP737 ;
static create_set<CharacterSetCP866> create_CP866 ;
static create_set<CharacterSetRUSCII> create_RUSCII ;
static create_set<CharacterSetKamenicky> create_Kamenicky ;
static create_set<CharacterSetMazovia> create_Mazovia ;
static create_set<CharacterSetMIK> create_MIK ;
static create_set<CharacterSetIranSystem> create_IranSystem ;
static create_set<CharacterSetCP862> create_CP862 ;
static create_set<CharacterSetCP1251> create_CP1251 ;
static create_set<CharacterSetCP1252> create_CP1252 ;
static create_set<CharacterSetCP1255> create_CP1255 ;
static create_set<CharacterSetCP1256> create_CP1256 ;
static create_set<CharacterSetArmSCII8> create_ArmSCII8 ;
static create_set<CharacterSetMacCyrillic> create_MacCyrillic ;
static create_set<CharacterSetTIS620> create_TIS620 ;
static create_set<CharacterSetTSCII> create_TSCII ;
static create_set<CharacterSetISCII> create_ISCII ;
static create_set<CharacterSetVISCII> create_VISCII ;
static create_set<CharacterSetGB2312> create_GB2312 ;
static create_set<CharacterSetGBK> create_GBK ;
static create_set<CharacterSetGBKlevel1> create_GBKlevel1 ;
static create_set<CharacterSetGBKlevel2> create_GBKlevel2 ;
static create_set<CharacterSetGBKlevel3> create_GBKlevel3 ;
static create_set<CharacterSetGB18030> create_GB18030 ;
static create_set<CharacterSetBig5> create_Big5 ;
static create_set<CharacterSetBig5Ext> create_Big5Ext ;
static create_set<CharacterSetISO6937> create_ISO6937 ;
static create_set<CharacterSetGEOSTD8> create_GEOSTD8 ;

static const CharacterSetDescription sets[] =
   {
      { "Latin-2",	"Lat2",		"Latin-2",	&create_Latin2 },
      { "iso-8859-2",	"Latin2",	"Latin-2",	&create_Latin2 },
      { "UTF-EBCDIC",	"UTF-EBCDIC",	"Unicode",	&create_UTF_EBCDIC },
      { "UTF-7",	"UTF7",		"Unicode",	&create_UTF7 },
      { "UTF-8Ext",	"UT-8Ext",	"Unicode",	&create_UTF8Ext },
      { "UTF-16BE",	"Uni-B", 	"Unicode",	&create_UnicodeBE },
      { "UTF-16LE",	"Uni-L", 	"Unicode",	&create_UnicodeLE },
      { "ISO-2022",	"ISO2022",	"ISO-2022",	&create_ISO2022 },
      { "EUC-CN",	"EUC_CN",	"GB-2312",	&create_GB2312 },
      { "EUC-KR",	"EUC_KR",	"EUC",		&create_EUC },
      { "EUC-JP",	"EUC_JP",	"EUC-JP",	&create_EUC_JP },
      { "EUC-TW",	"EUC_TW",	"EUC-TW",	&create_EUC_TW },
      { "EUC",		"EUC",		"EUC",		&create_EUC },
      { "ISO-6937",	"IEC-6937",	"ISO-6937",	&create_ISO6937 },
      { "Shift-JIS",	"Shift_JIS",	"ShiftJIS",	&create_ShiftJIS },
      { "ShiftJIS",	"SJIS",		"ShiftJIS",	&create_ShiftJIS },
      { "KOI7",		"KOI7",		"KOI7",		&create_KOI7 },
      { "KOI8-R",	"KOI8_R",	"KOI8-R",	&create_KOI8R },
      { "KOI8-U",	"KOI8_U",	"KOI8-U",	&create_KOI8U },
      { "CP437",	"DOS",		"CP437",	&create_CP437 },
      { "CP737",	"DOSGreek",	"CP737",	&create_CP737 },
      { "CP866",	"DOSCyrillic",	"CP866",	&create_CP866 },
      { "RUSCII",	"CP1125",	"RUSCII",	&create_RUSCII },
      { "Kamenicky",	"Kamenicky",	"Kamenicky",	&create_Kamenicky },
      { "Mazovia",	"Mazovia",	"Mazovia",	&create_Mazovia },
      { "MIK",		"MIK",		"MIK",		&create_MIK },
      { "IranSystem",	"IranSystem",	"IranSystem",	&create_IranSystem },
      { "CP1251",	"CP1251",	"CP1251",	&create_CP1251 },
      { "Windows-1251",	"Win-1251",	"CP1251",	&create_CP1251 },
      { "CP1255",	"CP1255",	"ISO-8859-8",	&create_CP1255 },
      { "Windows-1255",	"Win-1255",	"ISO-8859-8",	&create_CP1255 },
      { "CP1256",	"CP1256",	"CP1256",	&create_CP1256 },
      { "Windows-1256",	"Win-1256",	"CP1256",	&create_CP1256 },
      { "Windows-874",	"CP874",	"TIS-620",	&create_TIS620 },
      { "TIS-620",	"TIS620",	"TIS-620",	&create_TIS620 },
      { "ISO-8859-3",	"Latin-3",	"ISO-8859-3",	&create_ISO8859_5 },
      { "ISO-8859-5",	"Latin/Cyrillic", "ISO-8859-5",	&create_ISO8859_5 },
      { "ISO-8859-6",	"Latin/Arabic",	"ISO-8859-6",	&create_ISO8859_6 },
      { "ISO-8859-7",	"Latin/Greek",	"ISO-8859-7",	&create_ISO8859_7 },
      { "ISO-8859-8",	"Latin/Hebrew",	"ISO-8859-8",	&create_ISO8859_8 },
      { "ISO-8859-8-i",	"Latin/Hebrew-i", "ISO-8859-8",	&create_ISO8859_8 },
      { "ISO-8859-9",	"Latin-5",	"Latin-2",	&create_Latin2 },
      { "ISO-8859-10",	"Latin-6",	"Latin-6",	&create_ISO8859_10 },
      { "ISO-8859-11",	"Latin/Thai",	"TIS-620",	&create_TIS620 },
      { "ISO-8859-13",	"Latin-7",	"ISO-8859-13",	&create_ISO8859_13 },
      { "ISO-8859-15",	"Latin-10",	"Latin-10",	&create_ISO8859_15 },
      { "CP862",	"CP862",	"CP862",	&create_CP862 },
      { "TSCII",	"TSCII",	"TSCII",	&create_TSCII },
      { "ISCII",	"IS13194",	"ISCII",	&create_ISCII },
      { "VISCII",	"VISCII",	"VISCII",	&create_VISCII },
      { "GB18030",	"GB18030",	"GB18030",	&create_GB18030 },
      { "GBK/1",	"GBK/1",	"GBK",		&create_GBKlevel1 },
      { "GBK/2",	"GBK/2",	"GBK",		&create_GBKlevel2 },
      { "GBK/3",	"GBK/3",	"GBK",		&create_GBKlevel3 },
      { "GBK",		"GBK",		"GBK",		&create_GBK },
      { "GB-2312",	"GB",		"GB-2312",	&create_GB2312 },
      { "GB2312",	"GB2312",	"GB-2312",	&create_GB2312 },
      { "Big5-Ext",	"Big5x",	"Big5",		&create_Big5Ext },
      { "Big5",		"CP950",	"Big5",		&create_Big5 },
      { "HZ",		"HZ",		"HZ",		&create_HZ },
      { "Ascii85",	"Ascii85",	"Ascii85",	&create_Ascii85 },
      { "ASCII-16BE",	"b",	 	"Latin-1",	&create_ASCII16BE },
      { "ASCII-16LE",	"l",	 	"Latin-1",	&create_ASCII16LE },
      { "ASCII-32BE",	"B",	 	"Latin-1",	&create_ASCII32BE },
      { "ASCII-32LE",	"L",	 	"Latin-1",	&create_ASCII32LE },
      { "UTF-32BE",	"UTF32BE",	"Unicode",	&create_UTF32BE },
      { "UTF-32LE",	"UTF32LE",	"Unicode",	&create_UTF32LE },
      { "US-ASCII",	"ISO-IR-6",	"ASCII",	&create_ASCII },
      { "ASCII",	"s", 		"ASCII",	&create_ASCII },
      { "Latin-1",	"S", 		"Latin-1",	&create_Latin1 },
      { "ISO-8859-1",	"Latin1",	"Latin-1",	&create_Latin1 },
      { "EUC",		"e",		"EUC",		&create_EUC },
      { "UTF8",		"u",		"Unicode",	&create_UTF8 },
      { "UTF8",		"UTF-8",	"Unicode",	&create_UTF8 },
      { "GEORGIAN-ACADEMY", "GEOSTD8",	"GEOSTD8",	&create_GEOSTD8 },
      // aliases
      { "CP915",	"CP28595",      "Latin-1",	&create_ISO8859_5 },
      { "UTF16BE",	"U16B", 	"Unicode",	&create_UnicodeBE },
      { "UTF16LE",	"U16L", 	"Unicode",	&create_UnicodeLE },
      { "ELOT-928",	"Latin/Greek",	"ISO-8859-7",	&create_ISO8859_7 },
      { "Wansung",	"Wansung",	"EUC",		&create_EUC } , //EUC-KR
      { "ISO-IR-101",	"L2",		"Latin-2",	&create_Latin2 },
      { "ISO-IR-127",	"ECMA-114",	"ISO-8859-6",	&create_ISO8859_6 },

      // aliases which may not be fully implemented
      { "ISO-IR-110",	"Latin-4",	"ISO-8859-4",	&create_Latin1 },
      { "iso-8859-4",	"Latin-4",	"ISO-8859-4",	&create_Latin1 },

      // aliases to support converted mguesser data: may not be entirely correct
      { "Windows-1250",	"CP1250",	"CP1251",	&create_CP1251 },
      { "Windows-1252",	"Win-1252",	"CP1251",	&create_CP1251 },
      { "Windows-1253",	"CP1253",	"CP1251",	&create_CP1251 },
      { "Windows-1254",	"CP1254",	"CP1251",	&create_CP1251 },
      { "Windows-1257",	"CP1257",	"CP1251",	&create_CP1251 },
      { "Windows-1258",	"CP1258",	"CP1251",	&create_CP1251 },
      { "Latin3",	"Latin3",	"Latin-2",	&create_Latin2 },
      { "Latin4",	"Latin4",	"Latin-2",	&create_Latin2 },
      { "CP857",	"DOSTurkish",	"CP437",	&create_CP437 },
      // the following mguesser compatibility entries are known to be incomplete
      { "ArmSCII-8",	"ArmSCII8",	"ArmSCII",	&create_ArmSCII8 },
      { "MacCyrillic",	"MacCyrillic",	"MacCyrillic",	&create_MacCyrillic },
      // end marker
      { 0,		0,		0,		0 }
   } ;

CharacterSetCache *CharacterSetCache::s_cache = 0 ;

static const bool CP1251_alphanum[64] =
   {
      // 0x80 to 0x8F
      true, true, false, true, false, false, false, false,
      false, false, true, false, true, true, true, true,
      // 0x90 to 0x9F
      true, false, false, false, false, false, false, false,
      false, false, true, false, true, true, true, true,
      // 0xA0 to 0xAF
      true, true, true, true, false, true, false, false,
      true, false, true, false, false, false, false, true,
      // 0xB0 to 0xBF
      false, false, true, true, true, false, false, false,
      true, false, true, false, true, true, true, true
   } ;

static const bool CP1256_alphanum[128] =
   {
      // 0x80 to 0x8F
      true, true, false, false, false, false, false, false,
      false, false, true, false, true, true, true, true,
      // 0x90 to 0x9F
      true, false, false, false, false, false, false, false,
      true, false, true, false, true, false, false, true,
      // 0xA0 to 0xAF
      false, true, false, false, false, false, false, false,
      false, false, true, false, false, false, false, false,
      // 0xB0 to 0xBF
      false, false, false, false, false, false, false, false,
      false, false, false, false, false, false, false, false,
      // 0xC0 to 0xCF
      true, true, true, true, true, true, true, true,
      true, true, true, true, true, true, true, true,
      // 0xD0 to 0xDF
      true, true, true, true, true, true, true, false,
      true, true, true, true, true, true, true, true,
      // 0xE0 to 0xEF
      true, true, true, true, true, true, true, true,
      true, true, true, true, true, true, true, true,
      // 0xF0 to 0xFF
      true, true, true, true, true, true, true, false,
      true, true, true, true, true, false, false, true
   } ;

static const uint32_t EBCDIC_base[] =
   {
      // 0x80 to 0x8F
      0x00A0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x00C0, 0x00E0, 0x0100, 0x0120, 0x0140, 0x0160,
      // 0x90 to 0x9F
      0x0180, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01A0, 0x01C0, 0x01E0, 0x0200, 0x0220, 0x0240,
      // 0xA0 to 0xAF
      0x0260, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0280, 0x02A0, 0x02C0, 0,      0x02E0, 0x0300,
      // 0xB0 to 0xBF
      0x0320, 0x0340, 0x0360, 0x0380, 0x03A0, 0x03C0, 0x03E0, 0, 0x0400, 0x0800, 0x0C00, 0x1000, 0x1400, 0, 0x1800, 0x1C00,
      // 0xC0 to 0xCF
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x2000, 0x2400, 0x2800, 0x2C00, 0x3000, 0x3400,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x3800, 0x3C00, 0x4000, 0x8000, 0x10000, 0x10000,
      0, 0x10000, 0, 0, 0, 0, 0, 0, 0, 0, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
   } ;

static const unsigned char EBCDIC_map[256] =
   {
      0x00,0x01,0x02,0x03,0x9C,0x09,0x86,0x7F,
      0x97,0x8D,0x8E,0x0B,0x0C,0x0D,0x0E,0x0F,
      0x10,0x11,0x12,0x13,0x9D,0x0A,0x08,0x87,
      0x18,0x19,0x92,0x8F,0x1C,0x1D,0x1E,0x1F,
      0x80,0x81,0x82,0x83,0x84,0x85,0x17,0x1B,
      0x88,0x89,0x8A,0x8B,0x8C,0x05,0x06,0x07,
      0x90,0x91,0x16,0x93,0x94,0x95,0x96,0x04,
      0x98,0x99,0x9A,0x9B,0x14,0x15,0x9E,0x1A,
      0x20,   0,   0,   0,   0,   0,   0,   0,
         0,   0,   0,0x2E,0x3C,0x28,0x2B,0x7C,
      0x26,   0,   0,   0,   0,   0,   0,   0,
         0,   0,0x21,0x24,0x2A,0x29,0x3B,0x5E,
      0x2D,0x2F,   0,   0,   0,   0,   0,   0,
         0,   0,   0,0x2C,0x25,0x5F,0x3E,0x3F,
         0,   0,   0,   0,   0,   0,   0,   0,
         0,0x60,0x3A,0x23,0x40,0x27,0x3D,0x22,
         0,0x61,0x62,0x63,0x64,0x65,0x66,0x67,
      0x67,0x69,   0,   0,   0,   0,   0,   0,
         0,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,0x70,
      0x71,0x72,   0,   0,   0,   0,   0,   0,
         0,0x7E,0x73,0x74,0x75,0x76,0x77,0x78,
      0x79,0x7A,   0,   0,   0,0x5B,   0,   0,
         0,   0,   0,   0,   0,   0,   0,   0,
         0,   0,   0,   0,   0,0x5D,   0,   0,
      0x7B,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
      0x48,0x49,   0,   0,   0,   0,   0,   0,
      0x7D,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,
      0x51,0x52,   0,   0,   0,   0,   0,   0,
      0x5C,   0,0x53,0x54,0x55,0x56,0x57,0x58,
      0x59,0x5A,   0,   0,   0,   0,   0,   0,
      0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
      0x38,0x39,   0,   0,   0,   0,   0,0x9F,
   } ;

/************************************************************************/
/*	Helper Functions						*/
/************************************************************************/

static void initialize_ASCII(CharacterCode codes[])
{
   for (int i = 0 ; i < 128 ; i++)
      {
      if (isprint(i) || i == '\t')
	 codes[i].init(1,0,0) ;
      }
   return ;
}

//----------------------------------------------------------------------

static bool Unicode_isAlphaNum(wchar_t codepoint)
{
   // 7-bit ASCII
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   // Latin-1 codepoints
   if (codepoint >= 192 && codepoint <= 255)
      return true ;
   else if (codepoint < 192)
      return false ;
   return iswalnum(codepoint) ;
}

//----------------------------------------------------------------------

static bool romanizable_Unicode(const CharacterSet *set,
				const unsigned char *buf, size_t buflen)
{
   if (!set || !buf)
      return false ;
   size_t pos = 0 ;
   EscapeState es ;
   bool changes = false ;
   while (pos < buflen)
      {
      wchar_t codepoint ;
      int len = set->nextCodePoint(buf+pos,codepoint,es) ;
      if (len == 0)
	 break ;
      if (romanizable_codepoint(codepoint))
	 {
	 changes = true ;
	 break ;
	 }
      pos += len ;
      }
   return changes ;

}

/************************************************************************/
/*	Methods for class CharacterSetASCII				*/
/************************************************************************/

CharacterSetASCII::CharacterSetASCII(const char *name, const char *enc)
   : CharacterSet(name, enc)
{
   // all characters have been defaulted to invalid, so now set the
   //   printable ASCII characters as valid
   initialize_ASCII(m_codes) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetASCII::nextCodePoint(const unsigned char *s,
				     wchar_t &codepoint, EscapeState &) const
{
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   codepoint = s[0] ;
   return len ;
}

//----------------------------------------------------------------------

bool CharacterSetASCII::isAlphaNum(wchar_t codepoint) const
{
   return isascii(codepoint) && isalnum(codepoint) ;
}

//----------------------------------------------------------------------

int CharacterSetASCII::consumeNewlines(const unsigned char *s, size_t buflen) const
{
   int count = 0 ;
   if (s)
      {
      while (buflen > 0 && (*s == '\r' || *s == '\n'))
	 {
	 s++ ;
	 buflen-- ;
	 count++ ;
	 }
      }
   return count ;
}

/************************************************************************/
/*	Methods for class CharacterSetLatin1				*/
/************************************************************************/

CharacterSetLatin1::CharacterSetLatin1(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // low 128 ASCII characters have already been set valid
   // add the printable high-bit Latin-1 characters as valid
   for (unsigned i = 160 ; i < lengthof(m_codes)-1 ; i++)
      {
      m_codes[i].init(1,0,0) ;
      }
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetLatin1::isAlphaNum(wchar_t codepoint) const
{
   if (codepoint >= 0xC0 && codepoint <= 0xFF)
      return (codepoint != 0xD7 && codepoint != 0xF7) ;
   else
      return isascii(codepoint) && isalnum(codepoint) ;
}

//----------------------------------------------------------------------

bool CharacterSetLatin1::writeAsUTF(FILE *fp, const unsigned char *buffer,
				     size_t buflen, bool romanize,
				     OutputFormat fmt) 
{
   // Latin-1 codepoints are exactly equal to Unicode codepoints
   for (size_t i = 0 ; i < buflen ; i++)
      {
      if (!writeUTF(fp,buffer[i],romanize,fmt))
	 return false ;
      }
   return true ;
}

/************************************************************************/
/*	Methods for class CharacterSetLatin2				*/
/************************************************************************/

CharacterSetLatin2::CharacterSetLatin2(const char *name, const char *enc)
   : CharacterSetLatin1(name,enc)
{
   // valid codes are same as for Latin-1
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetLatin2::isAlphaNum(wchar_t codepoint) const
{
   return CharacterSetLatin1::isAlphaNum(codepoint) ;
}

//----------------------------------------------------------------------

bool CharacterSetLatin2::writeAsUTF(FILE *fp, const unsigned char *buffer,
				    size_t buflen, bool romanize,
				    OutputFormat fmt)
{
   // override the Latin-1 version and revert to default
   return CharacterSet::writeAsUTF(fp,buffer,buflen,romanize,fmt) ;
}

/************************************************************************/
/*	Methods for class CharacterSetISO8859_5 (Latin/Cyrillic)	*/
/************************************************************************/

bool CharacterSetISO8859_5::isAlphaNum(wchar_t codepoint) const
{
   if (codepoint >= 0xA1 && codepoint <= 0xFF)
      return (codepoint != 0xAD && codepoint != 0xF0 && codepoint != 0xFD) ;
   else
      return isascii(codepoint) && isalnum(codepoint) ;
}

//----------------------------------------------------------------------

bool CharacterSetISO8859_5::writeAsUTF(FILE *fp, const unsigned char *buffer,
				       size_t buflen, bool romanize,
				       OutputFormat fmt)
{
   // override the Latin-1 version and revert to default
   return CharacterSet::writeAsUTF(fp,buffer,buflen,romanize,fmt) ;
}

/************************************************************************/
/*	Methods for class CharacterSetISO8859_6 (Latin/Arabic)		*/
/************************************************************************/

CharacterSetISO8859_6::CharacterSetISO8859_6(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // low 128 ASCII characters have already been set valid
   // add the printable high-bit Latin/Arabic characters as valid
   m_codes[0xA0].init(1,0,0) ;
   m_codes[0xA4].init(1,0,0) ;
   m_codes[0xAC].init(1,0,0) ;
   m_codes[0xAD].init(1,0,0) ;
   m_codes[0xBB].init(1,0,0) ;
   m_codes[0xBF].init(1,0,0) ;
   for (unsigned i = 0xC1 ; i <= 0xDA ; i++)
      m_codes[i].init(1,0,0) ;
   for (unsigned i = 0xE0 ; i <= 0xF2 ; i++)
      m_codes[i].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetISO8859_6::isAlphaNum(wchar_t codepoint) const
{
   if (codepoint >= 0xC1 && codepoint <= 0xDA)
      return true ;
   else if (codepoint >= 0xE0 && codepoint <= 0xF2)
      return true ;
   else
      return isascii(codepoint) && isalnum(codepoint) ;
}

/************************************************************************/
/*	Methods for class CharacterSetISO8859_7 (Latin/Greek)		*/
/************************************************************************/

CharacterSetISO8859_7::CharacterSetISO8859_7(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // low 128 ASCII characters have already been set valid
   // add the printable high-bit Latin/Hebrew characters as valid
   m_codes[0xA0].init(1,0,0) ;
   m_codes[0xA1].init(1,0,0) ;
   m_codes[0xA2].init(1,0,0) ;
   m_codes[0xA3].init(1,0,0) ;
   m_codes[0xA9].init(1,0,0) ;
   m_codes[0xAC].init(1,0,0) ;
   m_codes[0xAD].init(1,0,0) ;
   m_codes[0xAF].init(1,0,0) ;
   m_codes[0xB6].init(1,0,0) ;
   m_codes[0xB8].init(1,0,0) ;
   m_codes[0xB9].init(1,0,0) ;
   m_codes[0xBA].init(1,0,0) ;
   m_codes[0xBC].init(1,0,0) ;
   for (unsigned i = 0xBE ; i <= 0xD1 ; i++)
      m_codes[i].init(1,0,0) ;
   for (unsigned i = 0xD3 ; i <= 0xFE ; i++)
      m_codes[i].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetISO8859_7::isAlphaNum(wchar_t codepoint) const
{
   if (codepoint >= 0xBE && codepoint <= 0xD1)
      return true ;
   else if (codepoint >= 0xD3 && codepoint <= 0xFE)
      return true ;
   else if (codepoint == 0xB6 || codepoint == 0xB8 || codepoint == 0xB9 ||
	    codepoint == 0xBA || codepoint == 0xBC)
      return true ;
   else
      return isascii(codepoint) && isalnum(codepoint) ;
}

/************************************************************************/
/*	Methods for class CharacterSetISO8859_8 (Latin/Hebrew)		*/
/************************************************************************/

CharacterSetISO8859_8::CharacterSetISO8859_8(const char *name,const char *enc)
   : CharacterSetASCII(name,enc)
{
   // low 128 ASCII characters have already been set valid
   // add the printable high-bit Latin/Hebrew characters as valid
   m_codes[0xA0].init(1,0,0) ;
   for (unsigned i = 0xA2 ; i <= 0xBE ; i++)
      m_codes[i].init(1,0,0) ;
   for (unsigned i = 0xDF ; i <= 0xFA ; i++)
      m_codes[i].init(1,0,0) ;
   m_codes[0xFD].init(1,0,0) ;
   m_codes[0xFE].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetISO8859_8::isAlphaNum(wchar_t codepoint) const
{
   if (codepoint >= 0xE0 && codepoint <= 0xFA)
      return true ;
   else
      return isascii(codepoint) && isalnum(codepoint) ;
}

/************************************************************************/
/*	Methods for class CharacterSetLatin6				*/
/************************************************************************/

bool CharacterSetLatin6::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else if (codepoint >= 192 && codepoint <= 255)
      return true ;
   else if (codepoint < 0xA1 || codepoint == 0xA7 || codepoint == 0xAD ||
	    codepoint == 0xB0 || codepoint == 0xB7 || codepoint == 0xBD)
      return false ;
   else
      return true ;
}

//----------------------------------------------------------------------

bool CharacterSetLatin6::writeAsUTF(FILE *fp, const unsigned char *buffer,
				    size_t buflen, bool romanize,
				    OutputFormat fmt)
{
   // override the Latin-1 version and revert to default
   return CharacterSet::writeAsUTF(fp,buffer,buflen,romanize,fmt) ;
}

/************************************************************************/
/*	Methods for class CharacterSetLatin7				*/
/************************************************************************/

CharacterSetLatin7::CharacterSetLatin7(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // low 128 ASCII characters have already been set valid
   // add the high-bit alphabetic characters as valid
   m_codes[0xA1].init(1,0,0) ;
   m_codes[0xA5].init(1,0,0) ;
   m_codes[0xA8].init(1,0,0) ;
   m_codes[0xAA].init(1,0,0) ;
   m_codes[0xAF].init(1,0,0) ;
   m_codes[0xB4].init(1,0,0) ;
   m_codes[0xB5].init(1,0,0) ;
   m_codes[0xB8].init(1,0,0) ;
   m_codes[0xBA].init(1,0,0) ;
   for (unsigned i = 0xBF ; i <= 0xD6 ; i++)
      {
      m_codes[i].init(1,0,0) ;
      }
   for (unsigned i = 0xD8 ; i <= 0xF6 ; i++)
      {
      m_codes[i].init(1,0,0) ;
      }
   for (unsigned i = 0xF8 ; i <= 0xFF ; i++)
      {
      m_codes[i].init(1,0,0) ;
      }
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetLatin7::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else if (codepoint >= 192 && codepoint <= 255)
      return true ;
   else if (codepoint < 0xA1 || codepoint == 0xA7 || codepoint == 0xAD ||
	    codepoint == 0xB0 || codepoint == 0xB7 || codepoint == 0xBD)
      return false ;
   else
      return true ;
}

//----------------------------------------------------------------------

bool CharacterSetLatin7::writeAsUTF(FILE *fp, const unsigned char *buffer,
				    size_t buflen, bool romanize,
				    OutputFormat fmt)
{
   // override the Latin-1 version and revert to default
   return CharacterSet::writeAsUTF(fp,buffer,buflen,romanize,fmt) ;
}

/************************************************************************/
/*	Methods for class CharacterSetLatin10				*/
/************************************************************************/

bool CharacterSetLatin10::isAlphaNum(wchar_t codepoint) const
{
   if (codepoint >= 192 && codepoint <= 255)
      return true ;
   else if (codepoint == 0xA6 || codepoint == 0xA8 || codepoint == 0xB4 ||
	    codepoint == 0xB8 || codepoint == 0xBC || codepoint == 0xBD ||
	    codepoint == 0xBE)
      return true ;
   else
      return isascii(codepoint) && isalnum(codepoint) ;
}

//----------------------------------------------------------------------

bool CharacterSetLatin10::writeAsUTF(FILE *fp, const unsigned char *buffer,
				     size_t buflen, bool romanize,
				     OutputFormat fmt)
{
   // override the Latin-1 version and revert to default
   return CharacterSet::writeAsUTF(fp,buffer,buflen,romanize,fmt) ;
}

/************************************************************************/
/*	Methods for class CharacterSetASCII16BE				*/
/************************************************************************/

CharacterSetASCII16BE::CharacterSetASCII16BE(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // all characters have been defaulted to invalid, so now set the
   //   big-endian 16-bit code points 9 (tab) through 255 as valid
   m_codes[0].init(2,9,255) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetASCII16BE::nextCodePoint(const unsigned char *s,
					 wchar_t &codepoint,
					 EscapeState &) const
{
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   if (len == 2 && (s[1] == '\t' || s[1] > 0x7F || isprint(s[1])))
      codepoint = s[1] ;
   else
      {
      codepoint = 0 ;
      len = 0 ;
      }
   return len ;
}

//----------------------------------------------------------------------

bool CharacterSetASCII16BE::isAlphaNum(wchar_t codepoint) const
{
   return isascii(codepoint) && isalnum(codepoint) ;
}

//----------------------------------------------------------------------

int CharacterSetASCII16BE::consumeNewlines(const unsigned char *s, size_t buflen) const
{
   int count = 0 ;
   if (s)
      {
      while (buflen > 1 && s[0] == '\0' && (s[1] == '\r' || s[1] == '\n'))
	 {
	 s += 2 ;
	 buflen -= 2 ;
	 count += 2 ;
	 }
      }
   return count ;
}

/************************************************************************/
/*	Methods for class CharacterSetASCII16LE				*/
/************************************************************************/

CharacterSetASCII16LE::CharacterSetASCII16LE(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // set the little-endian 16-bit code points corresponding to
   //   printable ASCII characters as valid
   for (unsigned i = 0 ; i < lengthof(m_codes) ; i++)
      {
      if (isprint(i) || i == '\t')
	 m_codes[i].init(2,0,0) ;
      }
   return ;
}

//----------------------------------------------------------------------

int CharacterSetASCII16LE::consumeNewlines(const unsigned char *s, size_t buflen) const
{
   int count = 0 ;
   if (s)
      {
      while (buflen > 1 && s[1] == '\0' && (s[0] == '\r' || s[0] == '\n'))
	 {
	 s += 2 ;
	 buflen -= 2 ;
	 count += 2 ;
	 }
      }
   return count ;
}

/************************************************************************/
/*	Methods for class CharacterSetASCII32BE				*/
/************************************************************************/

CharacterSetASCII32BE::CharacterSetASCII32BE(const char *name, const char *enc)
   : CharacterSetASCII16BE(name,enc)
{
   // all characters have been defaulted to invalid, so now set the
   //   big-endian 32-bit code points starting with 00/00 valid
   m_codes[0].init(4,0,0) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetASCII32BE::nextCodePoint(const unsigned char *s,
					 wchar_t &codepoint,
					 EscapeState &) const
{
   int len = m_codes[s[0]].length() ;
   if (len == 4 && s[1] == 0 && s[2] == 0 &&
       (s[3] == '\t' || (s[3] == '\n' && newlineOK()) || s[3] >= ' '))
      {
      codepoint = s[3] ;
      return len ;
      }
   else
      {
      codepoint = 0 ;
      return 0 ;
      }
}

/************************************************************************/
/*	Methods for class CharacterSetASCII32LE				*/
/************************************************************************/

CharacterSetASCII32LE::CharacterSetASCII32LE(const char *name, const char *enc)
   : CharacterSetASCII16LE(name,enc)
{
   // set the little-endian 32-bit code points corresponding to
   //   printable ASCII characters as valid
   for (unsigned i = 0 ; i < lengthof(m_codes) ; i++)
      {
      if (isprint(i) || i == '\t')
	 m_codes[i].init(4,0,0) ;
      }
   return ;
}

/************************************************************************/
/*	Methods for class CharacterSetUnicodeBE				*/
/************************************************************************/

CharacterSetUnicodeBE::CharacterSetUnicodeBE(const char *name, const char *enc)
   : CharacterSet(name,enc)
{
   // unfortunately, pretty much any 16-bit code is allowable....
   for (unsigned i = 0 ; i < lengthof(m_codes) ; i++)
      {
      if (i == 0x0A || i == 0x0C || i == 0x0D)
	 {
	 // omit reversed-endian \f, \r and \n, which fortunately are not
	 //  a valid characters anyway
	 m_codes[i].init(2,0x01,0xFF) ;
	 }
      else
	 m_codes[i].init(2,0x00,0xFF) ;
      }
   return ;
}

//----------------------------------------------------------------------

int CharacterSetUnicodeBE::nextCodePoint(const unsigned char *s,
					 wchar_t &codepoint, 
					 EscapeState &) const
{
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   codepoint = (s[0] << 8) | s[1] ;
   if ((codepoint == '\t' || iswprint(codepoint) || (codepoint == '\n' && newlineOK())) &&
       codepoint != 0xFEFF) // BOM, for which iswprint() returns 1
      return len ;
   else
      {
      codepoint = 0 ;
      return 0 ;
      }
}

//----------------------------------------------------------------------

bool CharacterSetUnicodeBE::isAlphaNum(wchar_t codepoint) const
{
   return Unicode_isAlphaNum(codepoint) ;
}

//----------------------------------------------------------------------

bool CharacterSetUnicodeBE::writeAsUTF(FILE *fp, const unsigned char *buffer,
				       size_t buflen, bool romanize,
				       OutputFormat fmt)
{
   for (size_t i = 0 ; i < buflen ; i += 2)
      {
      wchar_t codepoint = (buffer[i] << 8) | buffer[i+1] ;
      if (!writeUTF(fp,codepoint,romanize,fmt))
	 return false ;
      }
   return true ;
}

//----------------------------------------------------------------------

bool CharacterSetUnicodeBE::romanizable(const unsigned char *buf, size_t buflen) const
{
   return romanizable_Unicode(this,buf,buflen) ;
}

//----------------------------------------------------------------------

int CharacterSetUnicodeBE::consumeNewlines(const unsigned char *s, size_t buflen) const
{
   int count = 0 ;
   if (s)
      {
      while (buflen > 1 && s[0] == '\0' && (s[1] == '\r' || s[1] == '\n'))
	 {
	 s += 2 ;
	 buflen -= 2 ;
	 count += 2 ;
	 }
      }
   return count ;
}

/************************************************************************/
/*	Methods for class CharacterSetUnicodeLE				*/
/************************************************************************/

CharacterSetUnicodeLE::CharacterSetUnicodeLE(const char *name, const char *enc)
   : CharacterSet(name,enc)
{
   // unfortunately, pretty much any 16-bit code is allowable....
   for (unsigned i = 0 ; i < lengthof(m_codes) ; i++)
      {
      m_codes[i].init(2,0x00,0xFF) ;
      }
   return ;
}

//----------------------------------------------------------------------

int CharacterSetUnicodeLE::nextCodePoint(const unsigned char *s,
					 wchar_t &codepoint,
					 EscapeState &) const
{
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   codepoint = (s[1] << 8) | s[0] ;
   if ((codepoint == '\t' || iswprint(codepoint) || (codepoint == '\n' && newlineOK())) &&
       codepoint != 0xFEFF) // BOM, for which iswprint() returns 1
      return len ;
   else
      {
      codepoint = 0 ;
      return 0 ;
      }
}

//----------------------------------------------------------------------

bool CharacterSetUnicodeLE::isAlphaNum(wchar_t codepoint) const
{
   return Unicode_isAlphaNum(codepoint) ;
}

//----------------------------------------------------------------------

bool CharacterSetUnicodeLE::writeAsUTF(FILE *fp, const unsigned char *buffer,
				       size_t buflen, bool romanize,
				       OutputFormat fmt)
{
   for (size_t i = 0 ; i < buflen ; i += 2)
      {
      wchar_t codepoint = (buffer[i+1] << 8) | buffer[i] ;
      if (!writeUTF(fp,codepoint,romanize,fmt))
	 return false ;
      }
   return true ;
}

//----------------------------------------------------------------------

bool CharacterSetUnicodeLE::romanizable(const unsigned char *buf, size_t buflen) const
{
   return romanizable_Unicode(this,buf,buflen) ;
}

//----------------------------------------------------------------------

int CharacterSetUnicodeLE::consumeNewlines(const unsigned char *s, size_t buflen) const
{
   int count = 0 ;
   if (s)
      {
      while (buflen > 1 && s[1] == '\0' && (s[0] == '\r' || s[0] == '\n'))
	 {
	 s += 2 ;
	 buflen -= 2 ;
	 count += 2 ;
	 }
      }
   return count ;
}

/************************************************************************/
/*	Methods for class CharacterSetUTF_EBCDIC			*/
/************************************************************************/

CharacterSetUTF_EBCDIC::CharacterSetUTF_EBCDIC(const char *nm,const char *enc)
   : CharacterSetUTF8(nm,enc)
{
   // all characters have been defaulted to invalid, so now set the
   //   printable EBCDIC characters as valid
   m_codes[0x40].init(1,0,0) ;
   for (int i = 0x4B ; i <= 0x50 ; i++)
      m_codes[i].init(1,0,0) ;
   for (int i = 0x5A ; i <= 0x61 ; i++)
      m_codes[i].init(1,0,0) ;
   for (int i = 0x6B ; i <= 0x6F ; i++)
      m_codes[i].init(1,0,0) ;
   for (int i = 0x79 ; i <= 0x7F ; i++)
      m_codes[i].init(1,0,0) ;
   for (int i = 0x80 ; i <= 0xA0 ; i += 0x10)
      for (int j = 1 ; j <= 9 ; j++)
	 m_codes[i+j].init(1,0,0) ;
   m_codes[0xAD].init(1,0,0) ;
   m_codes[0xBD].init(1,0,0) ;
   for (int i = 0xC0 ; i <= 0xF0 ; i += 0x10)
      for (int j = 0 ; j <= 9 ; j++)
	 m_codes[i+j].init(1,0,0) ;
   // next, set the initial bytes of multi-byte characters as valid
   for (int i = 0x80 ; i <= 0xA0 ; i += 0x10)
      {
      m_codes[i].init(2,0xA0,0xBF) ;
      for (int j = 0x0A ; j <= 0x0F ; j++)
	 {
	 if (i + j != 0xAD)
	    m_codes[i+j].init(2,0xA0,0xBF) ;
	 }
      }
   for (int i = 0xB0 ; i <= 0xB6 ; i++)
      m_codes[i].init(2,0xA0,0xBF) ;
   for (int i = 0xB7 ; i <= 0xBF ; i++)
      {
      if (i != 0xBD)
	 m_codes[i].init(3,0xA0,0xBF) ;
      }
   for (int i = 0xC0 ; i <= 0xCF ; i ++)
      m_codes[i].init(3,0xA0,0xBF) ;
   m_codes[0xDA].init(3,0xA0,0xBF) ;
   m_codes[0xDB].init(3,0xA0,0xBF) ;
   for (int i = 0xDC ; i <= 0xDF ; i++)
      m_codes[i].init(4,0xA0,0xBF) ;
   m_codes[0xE1].init(4,0xA0,0xBF) ;
   m_codes[0xEA].init(4,0xA0,0xBF) ;
   m_codes[0xEB].init(4,0xA0,0xBF) ;
   m_codes[0xEC].init(5,0xA0,0xBF) ;
   m_codes[0xED].init(5,0xA0,0xBF) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetUTF_EBCDIC::nextCodePoint(const unsigned char *s,
					  wchar_t &codepoint, 
					  EscapeState &) const
{
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   wchar_t cp = len ? EBCDIC_map[s[0]] : 0 ;
   if (len > 1 && EBCDIC_base[s[0]] > 0)
      {
      uint32_t value = 0 ;
      for (int i = 1 ; i <= len ; i++)
	 {
	 value <<= 5 ;
	 value |= (s[1] - 0xA0) ;
	 }
      cp = EBCDIC_base[s[0]] + value ;
      }
   codepoint = cp ;
   return len ;
}

/************************************************************************/
/*	Methods for class CharacterSetUTF8				*/
/************************************************************************/

CharacterSetUTF8::CharacterSetUTF8(const char *name, const char *enc)
   : CharacterSet(name,enc)
{
   // all characters have been defaulted to invalid, so now set the
   //   printable ASCII characters as valid
   initialize_ASCII(m_codes) ;
   // next, set the initial bytes of multi-byte characters as valid
   for (int i = 0xC0 ; i < 0xE0 ; i++)
      m_codes[i].init(2,0x80,0xBF) ;
   for (int i = 0xE0 ; i < 0xF0 ; i++)
      m_codes[i].init(3,0x80,0xBF) ;
   for (int i = 0xF0 ; i <= 0xF4 ; i++)
      m_codes[i].init(4,0x80,0xBF) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetUTF8::nextCodePoint(const unsigned char *s,
				    wchar_t &codepoint,
				    EscapeState &) const
{
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   uint32_t cp = s[0] ;
   // clear high bits of the first byte, which indicate the number of
   //    following bytes
   for (int i = 7 ; i > 0 && (cp & (1 << i)) != 0 ; i--)
      cp &= ~(1 << i) ;
   // collect the low six bits of each of the remaining bytes into the
   //   code point
   for (int i = 1 ; i < len ; i++)
      {
      cp <<= 6 ;
      cp |= (s[i] & 0x3F) ;
      }
   if (cp > 0x10FFFF)
      {
      codepoint = 0 ;			// not a Unicode codepoint
      return 0 ;			//   even though valid UTF8 encoding
      }
   else if ((cp == '\t' || iswprint(cp) || (cp == '\n' && newlineOK())) &&
       cp != 0xFEFF) // BOM, for which iswprint() returns 1
      {
      codepoint = (wchar_t)cp ;
      return len ;
      }
   else
      {
      codepoint = 0 ;
      return 0 ;
      }
}

//----------------------------------------------------------------------

bool CharacterSetUTF8::isAlphaNum(wchar_t codepoint) const
{
   return Unicode_isAlphaNum(codepoint) ;
}

//----------------------------------------------------------------------

bool CharacterSetUTF8::romanizable(const unsigned char *buf, size_t buflen) const
{
   return romanizable_Unicode(this,buf,buflen) ;
}

/************************************************************************/
/*	Methods for class CharacterSetUTF8				*/
/************************************************************************/

CharacterSetUTF8Ext::CharacterSetUTF8Ext(const char *name, const char *enc)
   : CharacterSetUTF8(name,enc)
{
   // all characters have been defaulted to invalid, so now set the
   //   printable ASCII characters as valid
   initialize_ASCII(m_codes) ;
   // next, set the initial bytes of multi-byte characters as valid
   for (int i = 0xC0 ; i < 0xE0 ; i++)
      m_codes[i].init(2,0x80,0xBF) ;
   for (int i = 0xE0 ; i < 0xF0 ; i++)
      m_codes[i].init(3,0x80,0xBF) ;
   for (int i = 0xF0 ; i < 0xF8 ; i++)
      m_codes[i].init(4,0x80,0xBF) ;
   for (int i = 0xF8 ; i < 0xFC ; i++)
      m_codes[i].init(5,0x80,0xBF) ;
   for (int i = 0xFC ; i < 0xFE ; i++)
      m_codes[i].init(6,0x80,0xBF) ;
   return ;
}

/************************************************************************/
/*	Methods for class CharacterSetUTF32BE				*/
/************************************************************************/

CharacterSetUTF32BE::CharacterSetUTF32BE(const char *name, const char *enc)
   : CharacterSetUTF8(name,enc)
{
   // only codes 0x00000000 to 0x0010FFFF are valid, so set limits accordingly
   m_codes[0].init(4,0x00,0x10) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetUTF32BE::nextCodePoint(const unsigned char *s,
				       wchar_t &codepoint,
				       EscapeState &) const
{
   if (!s)
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   wchar_t cp = 0 ;
   if (len == 4 && m_codes[s[0]].validSuccessor(s[1]))
      {
      cp = (((uint32_t)s[1]) << 16) | (s[2] << 8) | s[3] ;
      }
   else
      len = 0 ;
   if ((cp == '\t' || iswprint(cp) || (cp == '\n' && newlineOK())) &&
       cp != 0xFEFF) // BOM, for which iswprint() returns 1
      {
      codepoint = cp ;
      return len ;
      }
   else
      {
      codepoint = 0 ;
      return 0 ;
      }
}

//----------------------------------------------------------------------

bool CharacterSetUTF32BE::writeAsUTF(FILE *fp, const unsigned char *buffer,
				     size_t buflen, bool romanize,
				     OutputFormat fmt)
{
   for (size_t i = 0 ; i < buflen ; i += 4)
      {
      wchar_t codepoint = ((buffer[i] << 24) | (buffer[i+1] << 16) |
			   (buffer[i+2] << 8) | buffer[i+3]) ;
      if (!writeUTF(fp,codepoint,romanize,fmt))
	 return false ;
      }
   return true ;
}

/************************************************************************/
/*	Methods for class CharacterSetUTF32LE				*/
/************************************************************************/

CharacterSetUTF32LE::CharacterSetUTF32LE(const char *name, const char *enc)
   : CharacterSetUTF8(name,enc)
{
   // only codes 0x00000000 to 0x0010FFFF are valid, but little-endian
   //   means we can't actually limit the character ranges here and
   //   have to do so in nextCodePoint()
   for (unsigned i = 0 ; i < lengthof(m_codes) ; i++)
      m_codes[i].init(4,0x00,0xFF) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetUTF32LE::nextCodePoint(const unsigned char *s,
				       wchar_t &codepoint,
				       EscapeState &) const
{
   if (!s)
      return -1 ;
   int len = m_codes[s[0]].length() ;
   wchar_t cp = 0 ;
   if (len == 4 && m_codes[s[0]].validSuccessor(s[1]) &&
       s[2] <= 0x10 && s[3] == 0)
      {
      cp = (((uint32_t)s[2]) << 16) | (s[1] << 8) | s[0] ;
      }
   else
      len = 0 ;
   if ((cp == '\t' || iswprint(cp) || (cp == '\n' && newlineOK())) &&
       cp != 0xFEFF) // BOM, for which iswprint() returns 1
      {
      codepoint = cp ;
      return len ;
      }
   else
      {
      codepoint = 0 ;
      return 0 ;
      }
}

//----------------------------------------------------------------------

bool CharacterSetUTF32LE::writeAsUTF(FILE *fp, const unsigned char *buffer,
				     size_t buflen, bool romanize,
				     OutputFormat fmt)
{
   for (size_t i = 0 ; i < buflen ; i += 4)
      {
      wchar_t codepoint = ((buffer[i+3] << 24) | (buffer[i+2] << 16) |
			   (buffer[i+1] << 8) | buffer[i]) ;
      if (!writeUTF(fp,codepoint,romanize,fmt))
	 return false ;
      }
   return true ;
}

/************************************************************************/
/*	Methods for class CharacterSetISO2022				*/
/************************************************************************/

CharacterSetISO2022::CharacterSetISO2022(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // bytes with high bit clear are single-byte ASCII, so initialize them
   //   that way
   initialize_ASCII(m_codes) ;
   // ISO-2022 encodes multibyte characters in seven-bit bytes, with
   //   escape sequences to designate the interpretation of following
   //   bytes, so simply add in the known escape sequences to avoid
   //   breaking strings in the middle
   m_codes[0x0E].init(1,0,0) ;  // shift out
   m_codes[0x0F].init(1,0,0) ;  // shift in
   m_codes[0x8E].init(1,0,0) ;  // single shift two
   m_codes[0x8F].init(1,0,0) ;  // single shift three
   m_codes[0x1B].init(2,0x20,0x7E) ;  // simple escape sequence
   // (we ignore escape sequences with intermediate bytes, since the
   //   final byte will be picked up as a valid character anyway)
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetISO2022::isAlphaNum(wchar_t codepoint) const
{
   // unfortunately, we can't tell for sure which characters are
   //   alphanumeric without knowing the exact coding *and* performing
   //   a complete interpretation of escape sequences embedded in the
   //   text, so just assume ASCII
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   return false ;
}

/************************************************************************/
/*	Methods for class CharacterSetEUC				*/
/************************************************************************/

CharacterSetEUC::CharacterSetEUC(const char *name, const char *enc)
   : CharacterSet(name,enc)
{
   // bytes with high bit clear are single-byte ASCII, so initialize them
   //   that way
   initialize_ASCII(m_codes) ;
   // bytes 0xA1-0xFE start two-byte ISO-2022 characters
   for (unsigned i = 0xA1 ; i <= 0xFE ; i++)
      m_codes[i].init(2,0xA1,0xFE) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetEUC::nextCodePoint(const unsigned char *s,
				   wchar_t &codepoint,
				   EscapeState &) const
{
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   wchar_t cp = s[0] ;
   if (len == 2)
      {
      cp = 128 + ((s[0] - 0xA1) * 94) + (s[1] - 0xA1) ;
      }
   codepoint = cp ;
   return len ;
}

//----------------------------------------------------------------------

bool CharacterSetEUC::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   //FIXME: need tables for what non-ASCII chars are alphanumeric
   // for now, assume they all are, to avoid breaking the alphanum filter
   return codepoint >= 128 ;
}

/************************************************************************/
/*	Methods for class CharacterSetEUC_JP				*/
/************************************************************************/

CharacterSetEUC_JP::CharacterSetEUC_JP(const char *name, const char *enc)
   : CharacterSet(name,enc)
{
   // bytes with high bit clear are single-byte ASCII, so initialize them
   //   that way
   initialize_ASCII(m_codes) ;
   // byte 0x8E introduces "high half" JIS-X-0208 (kana) 
   m_codes[0x8E].init(2,0xA1,0xDF) ;
   // byte 0x8F introduces three-byte JIS-X-0212 codes
   m_codes[0x8F].init(3,0xA1,0xFE) ;
   // bytes 0xA1-0xFE start two-byte JIS-X-0208 characters
   for (unsigned i = 0xA1 ; i <= 0xFE ; i++)
      m_codes[i].init(2,0xA1,0xFE) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetEUC_JP::nextCodePoint(const unsigned char *s,
				      wchar_t &codepoint,
				      EscapeState &) const
{
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   wchar_t cp = s[0] ;
   if (len == 3)
      {
      wchar_t base = 128 + (0xDF - 0xA0) + (0xFE - 0xA0)*(0xFE - 0xA0) ;
      codepoint = base + (s[1] - 0xA1) * (0xFE - 0xA0) + (s[2] - 0xA1) ;
      }
   else if (len == 2)
      {
      if (s[0] == 0x8E)
	 {
	 // high-half JIS-X-0208 (kana)
	 codepoint = 128 + (s[1] - 0xA1) ;
	 }
      else
	 {
	 codepoint = (128 + (0xDF - 0xA0) + (s[0] - 0xA1) * (0xFE - 0xA0)
		      + (s[1] - 0xA1)) ;
	 }
      }
   codepoint = cp ;
   return len ;
}

//----------------------------------------------------------------------

bool CharacterSetEUC_JP::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   //FIXME: need tables for what non-ASCII chars are alphanumeric
   // for now, assume they all are, to avoid breaking the alphanum filter
   return codepoint >= 128 ;
}

/************************************************************************/
/*	Methods for class CharacterSetEUC_TW				*/
/************************************************************************/

CharacterSetEUC_TW::CharacterSetEUC_TW(const char *name, const char *enc)
   : CharacterSet(name,enc)
{
   // bytes with high bit clear are single-byte ASCII, so initialize them
   //   that way
   initialize_ASCII(m_codes) ;
   // bytes 0xA1-0xFE start two-byte CNS 11643 plane 1 characters
   for (unsigned i = 0xA1 ; i <= 0xFE ; i++)
      m_codes[i].init(2,0xA1,0xFE) ;
   // byte 0x8E introduces four-byte encodings; second byte indicates
   //   plane 1-16
   m_codes[0x8E].init(4,0xA1,0xFE) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetEUC_TW::nextCodePoint(const unsigned char *s,
				      wchar_t &codepoint, 
				      EscapeState &) const
{
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   wchar_t cp = s[0] ;
   if (len == 4)
      {
      if (s[1] < 0xA1 || s[1] > 0xB0)
	 {
	 codepoint = 0 ;
	 return 0 ;
	 }
      else
	 {
	 unsigned plane = s[1] - 0xA0 ;
	 unsigned plane_size = (0xFE - 0xA0) * (0xFE - 0xA0) ;
	 codepoint = (128 + plane_size + (plane * plane_size) +
		      (s[2] - 0xA1) * (0xFE - 0xA0) + (s[3] - 0xA1)) ;
	 }
      }
   else if (len == 2)
      {
      codepoint = 128 + ((s[0] - 0xA1) * (0xFE - 0xA0)) + (s[1] - 0xA1) ;
      }
   codepoint = cp ;
   return len ;
}

//----------------------------------------------------------------------

bool CharacterSetEUC_TW::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   //FIXME: need tables for what non-ASCII chars are alphanumeric
   // for now, assume they all are, to avoid breaking the alphanum filter
   return codepoint >= 128 ;
}

/************************************************************************/
/*	Methods for class CharacterSetShiftJIS				*/
/************************************************************************/

CharacterSetShiftJIS::CharacterSetShiftJIS(const char *name, const char *enc)
   : CharacterSet(name,enc)
{
   // bytes with high bit clear are single-byte ASCII, so initialize them
   //   that way
   initialize_ASCII(m_codes) ;
   // bytes 0x81 through 0x9F and 0xE0 through 0xEF introduce two-byte chars
   for (size_t i = 0x81 ; i <= 0x9F ; i++)
      m_codes[i].init(2,0x40,0xFC) ;
   for (size_t i = 0xE0 ; i <= 0xEF ; i++)
      m_codes[i].init(2,0x40,0xFC) ;
   // bytes 0xA1 through 0xDF are single-byte half-width katakana characters
   for (size_t i = 0xA1 ; i <= 0xDF ; i++)
      m_codes[i].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetShiftJIS::nextCodePoint(const unsigned char *s,
					wchar_t &codepoint,
					EscapeState &) const
{
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   wchar_t cp = s[0] ;
   if (len == 2)
      {
      wchar_t page = (s[0] < 0xA0) ? (s[0] - 0x81) : (s[0]-0xE0 + (0xA0-0x81));
      codepoint = 256 + page * (0xFC - 0x40 + 1) + (s[1] - 0x40) ;
      }
   codepoint = cp ;
   return len ;
}

//----------------------------------------------------------------------

bool CharacterSetShiftJIS::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   //FIXME: need tables for what non-ASCII chars are alphanumeric
   // for now, assume they all are, to avoid breaking the alphanum filter
   return codepoint >= 128 ;
}

/************************************************************************/
/*	Methods for class CharacterSetAscii85				*/
/************************************************************************/

CharacterSetAscii85::CharacterSetAscii85(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   return ;
}

//----------------------------------------------------------------------

int CharacterSetAscii85::nextCodePoint(const unsigned char *s,
				       wchar_t &codepoint,
				       EscapeState &escape_state) const
{
   if (s[0] == '<' && s[1] == '~' && escape_state == ES_None)
      {
      escape_state = ES_Active ;
      s += 2 ;
      }
   else if (escape_state == ES_Active && s[0] == '~')
      {
      escape_state = ES_None ;
      s += (s[1] == '>') ? 2 : 1 ;
      }
   if (escape_state == ES_Active)
      {
      // grab up to five bytes at a time and decode them
//FIXME
      }
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   codepoint = (len > 0) ? s[0] : 0 ;
   return len ;
}

//----------------------------------------------------------------------

bool CharacterSetAscii85::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   //FIXME: need tables for what non-ASCII chars are alphanumeric
   // for now, assume they all are, to avoid breaking the alphanum filter
   return codepoint >= 128 ;
}

/************************************************************************/
/*	Methods for class CharacterSetHZ				*/
/************************************************************************/

CharacterSetHZ::CharacterSetHZ(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   return ;
}

//----------------------------------------------------------------------

int CharacterSetHZ::nextCodePoint(const unsigned char *s,
				  wchar_t &codepoint,
				  EscapeState &escape_state) const
{
   if (s[0] == '~')
      {
      if (s[1] == '~')
	 {
	 codepoint = '~' ;
	 return 2 ;
	 }
      else if (escape_state == ES_None && s[1] == '{')
	 {
	 escape_state = ES_Active ;
	 s += 2 ;
	 }
      else if (escape_state == ES_Active && s[1] == '}')
	 {
	 escape_state = ES_None ;
	 s += 2 ;
	 }
      }
   if (escape_state == ES_Active)
      {
      // grab two bytes at a time, ignoring the high bits
      uint8_t byte1 = (uint8_t)(s[0] & 0x7F) ;
      uint8_t byte2 = (uint8_t)(s[1] & 0x7F) ;
      if (byte1 >= 0x21 && byte1 <= 0x7E && byte2 >= 0x21 && byte2 <= 0x7E)
	 {
	 codepoint = 128 + (94 * (byte1 - 0x21)) + (byte2 - 0x21) ;
	 return 2 ;
	 }
      }
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   codepoint = (len > 0) ? s[0] : 0 ;
   return len ;
}

//----------------------------------------------------------------------

bool CharacterSetHZ::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   //FIXME: need tables for what non-ASCII chars are alphanumeric
   // for now, assume they all are, to avoid breaking the alphanum filter
   return codepoint >= 128 ;
}

/************************************************************************/
/*	Methods for class CharacterSetAscii85				*/
/************************************************************************/

CharacterSetUTF7::CharacterSetUTF7(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   return ;
}

//----------------------------------------------------------------------

int CharacterSetUTF7::nextCodePoint(const unsigned char *s,
				    wchar_t &codepoint,
				    EscapeState &escape_state) const
{
   if (s[0] == '+' && escape_state == ES_None)
      {
      if (s[1] == '-')
	 {
	 codepoint = '+' ;
	 return 2 ;
	 }
      escape_state = ES_Active ;
      s++ ;
      }
   else if (escape_state != ES_None && s[0] == '-')
      {
      escape_state = ES_None ;
      s++ ;
      }
   if (escape_state != ES_None)
      {
      // grab additional bytes and decode them
      // valid bytes are A-Za-0-9 as well as + and /
      // anything else terminates the Base64 sequence and reverts to
      //   single-byte ASCII encoding
//FIXME

      // we use escape_state of ES_Prefix1 and ES_Prefix2 to indicate how
      //   many bits of the first byte to skip, having already been consumed
      //   by the previous call to nextCodePoint()
      }
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   codepoint = len > 0 ? s[0] : 0 ;
   return len ;
}

//----------------------------------------------------------------------

bool CharacterSetUTF7::isAlphaNum(wchar_t codepoint) const
{
   return Unicode_isAlphaNum(codepoint) ;
}

/************************************************************************/
/*	Methods for class CharacterSetKOI7				*/
/************************************************************************/

CharacterSetKOI7::CharacterSetKOI7(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // same valid codes as ASCII, so nothing else to be done here
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetKOI7::isAlphaNum(wchar_t codepoint) const
{
   // KOI7 replaces codes 0x60 through 0x7E with Cyrillic characters, so
   //   those need to be flagged as alphanumeric
   if (codepoint >= 0x60 || codepoint <= 0x7E)
      return true ;
   else if (isascii(codepoint))
      return isalnum(codepoint) ;
   return false ;
}

/************************************************************************/
/*	Methods for class CharacterSetKOI8R				*/
/************************************************************************/

CharacterSetKOI8R::CharacterSetKOI8R(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // codes 0x80 to 0xBF are various graphical characters, except for A3/B3
   m_codes[0xA3].init(1,0,0) ;
   m_codes[0xB3].init(1,0,0) ;
   // codes 0xC0 to 0xFF are Cyrillic chars in pseudo-roman order,
   //   such that they correspond to the ASCII letters in the same
   //   position with the high bit stripped; omit 0xFF to reduce false
   //   positives on binary files
   for (unsigned i = 0xC0 ; i < 0xFF ; i++)
      m_codes[i].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetKOI8R::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   return codepoint == 0xA3 || codepoint == 0xB3 || codepoint >= 0xC0 ;
}

/************************************************************************/
/*	Methods for class CharacterSetKOI8U				*/
/************************************************************************/

CharacterSetKOI8U::CharacterSetKOI8U(const char *name, const char *enc)
   : CharacterSetKOI8R(name,enc)
{
   // KOI8-U extends KOI8-R by replacing eight graphical characters by letters
   m_codes[0xA4].init(1,0,0) ;
   m_codes[0xB4].init(1,0,0) ;
   m_codes[0xA6].init(1,0,0) ;
   m_codes[0xB6].init(1,0,0) ;
   m_codes[0xA7].init(1,0,0) ;
   m_codes[0xB7].init(1,0,0) ;
   m_codes[0xAD].init(1,0,0) ;
   m_codes[0xBD].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetKOI8U::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   wchar_t cp = codepoint & 0xEF ;
   if (cp == 0xA3 || cp == 0xA4 || cp == 0xA6 || cp == 0xA7 || cp == 0xAD)
      return true ;
   return codepoint >= 0xC0 ;
}

/************************************************************************/
/*	Methods for class CharacterSetCP437 (original IBM-PC)		*/
/************************************************************************/

CharacterSetCP437::CharacterSetCP437(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // all high-bit characters are valid in CP437, but we want to ignore the
   //   pure graphics characters at 0xB0 through 0xDF and the math symbols at
   //   0xEF through 0xFE to avoid excessive false positives
   for (unsigned i = 0x80 ; i < 0xB0 ; i++)
      m_codes[i].init(1,0,0) ;
   for (unsigned i = 0xE0 ; i < 0xEF ; i++)
      m_codes[i].init(1,0,0) ;
   m_codes[0xFF].init(1,0,0) ; //non-breaking space
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetCP437::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else if (codepoint >= 0x80 && codepoint <= 0xA5)
      return (codepoint < 0x9B || codepoint > 0x9E) ;
   else if (codepoint >= 0xE0 && codepoint <= 0xEE && codepoint != 0xEC)
      return true ; // Greek letters
   else
      return false ;
}

/************************************************************************/
/*	Methods for class CharacterSetCP737 (var of CP437 for Greek)	*/
/************************************************************************/

CharacterSetCP737::CharacterSetCP737(const char *name, const char *enc)
   : CharacterSetCP437(name,enc)
{
   // we've already marked 80-AF and E0-EE as valid; need to add EF,F0,F4,F5
   m_codes[0xEF].init(1,0,0) ;
   m_codes[0xF0].init(1,0,0) ;
   m_codes[0xF4].init(1,0,0) ;
   m_codes[0xF5].init(1,0,0) ;
   m_codes[0xFF].init(1,0,0) ; //non-breaking space
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetCP737::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else if (codepoint >= 0x80 && codepoint <= 0xBF)
      return true ;
   else if (codepoint >= 0xE0 && codepoint <= 0xF5)
      return true ;
   else
      return false ;
}

/************************************************************************/
/*	Methods for class CharacterSetCP862 (var of CP437 for Hebrew)	*/
/************************************************************************/

CharacterSetCP862::CharacterSetCP862(const char *name, const char *enc)
   : CharacterSetCP437(name,enc)
{
   // Hebrew letters are in positions 0x80 through 0x9A, which we've already
   //   marked as valid in the CP437 ctor
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetCP862::isAlphaNum(wchar_t codepoint) const
{
   return CharacterSetCP437::isAlphaNum(codepoint) ;
}

/************************************************************************/
/*	Methods for class CharacterSetCP866 (var of CP437 for Russian)	*/
/************************************************************************/

CharacterSetCP866::CharacterSetCP866(const char *name, const char *enc)
   : CharacterSetCP437(name,enc)
{
   // add codes 0xEF through 0xF7 as letters
   for (unsigned i = 0xEF ; i < 0xF8 ; i++)
      m_codes[i].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetCP866::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else if (codepoint >= 0x80 && codepoint <= 0xAF)
      return true ;
   else if (codepoint >= 0xE0 && codepoint <= 0xF7)
      return true ; // Cyrillic/Ukrainian/Belorussian letters
   else
      return false ;
}

/************************************************************************/
/*	Methods for class CharacterSetRUSCII (variant of CP866)		*/
/************************************************************************/

CharacterSetRUSCII::CharacterSetRUSCII(const char *name, const char *enc)
   : CharacterSetCP866(name,enc)
{
   // add codes 0xF8 and 0xF9 as letters; RUSSCI is identical to CP866
   //   except in codepoints 0xF2 through 0xF9
   m_codes[0xF8].init(1,0,0) ;
   m_codes[0xF9].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetRUSCII::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else if (codepoint >= 0x80 && codepoint <= 0xAF)
      return true ;
   else if (codepoint >= 0xE0 && codepoint <= 0xF9)
      return true ; // Cyrillic/Ukrainian/Belorussian letters
   else
      return false ;
}

/************************************************************************/
/*	Methods for class CharacterSetKamenicky				*/
/*	 (variant of CP437 for Czech)					*/
/************************************************************************/

bool CharacterSetKamenicky::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else if (codepoint >= 0x80 && codepoint <= 0xAB)
      return true ;
   else if (codepoint >= 0xE0 && codepoint <= 0xEE)
      return true ; // Greek letters
   else
      return false ;
}

/************************************************************************/
/*	Methods for class CharacterSetMazovia				*/
/*	 (variant of CP437 for Polish)					*/
/************************************************************************/

bool CharacterSetMazovia::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else if (codepoint >= 0x80 && codepoint <= 0x9A)
      return true ;
   else if (codepoint == 0x9E)
      return true ;
   else if (codepoint >= 0xA0 && codepoint <= 0xA7)
      return true ;
   else if (codepoint >= 0xE0 && codepoint <= 0xEE && codepoint != 0xEC)
      return true ; // Greek letters
   else
      return false ;
}

/************************************************************************/
/*	Methods for class CharacterSetMIK 				*/
/*	 (variant of CP437 for Bulgarian)				*/
/************************************************************************/

CharacterSetMIK::CharacterSetMIK(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // all high-bit characters are valid in MIK, but we want to ignore the
   //   pure graphics characters at 0xC0 through 0xDF and the math symbols at
   //   0xEF through 0xFE to avoid excessive false positives
   for (unsigned i = 0x80 ; i < 0xB0 ; i++)
      m_codes[i].init(1,0,0) ;
   for (unsigned i = 0xE0 ; i < 0xEF ; i++)
      m_codes[i].init(1,0,0) ;
   m_codes[0xFF].init(1,0,0) ; //non-breaking space
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetMIK::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else if (codepoint >= 0x80 && codepoint <= 0xBF)
      return true ;
   else if (codepoint >= 0xE0 && codepoint <= 0xEE && codepoint != 0xEC)
      return true ; // Greek letters
   else
      return false ;
}

/************************************************************************/
/*	Methods for class CharacterSetIranSystem			*/
/*	 (variant of CP437 for Persian)					*/
/************************************************************************/

CharacterSetIranSystem::CharacterSetIranSystem(const char *nm, const char *enc)
   : CharacterSetASCII(nm,enc)
{
   // all high-bit characters are valid in MIK, but we want to ignore
   //   the pure graphics characters at 0xB0 through 0xDF to avoid
   //   excessive false positives
   for (unsigned i = 0x80 ; i < 0xB0 ; i++)
      m_codes[i].init(1,0,0) ;
   for (unsigned i = 0xE0 ; i <= 0xFF ; i++)
      m_codes[i].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetIranSystem::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else if (codepoint >= 0x80 && codepoint <= 0xBF)
      return true ;
   else if (codepoint >= 0xE0 && codepoint <= 0xFE)
      return true ;
   else
      return false ;
}

/************************************************************************/
/*	Methods for class CharacterSetCP1251				*/
/************************************************************************/

CharacterSetCP1251::CharacterSetCP1251(const char *name, const char *enc)
   : CharacterSetKOI8R(name,enc)
{
   // CP1251/Windows-1251 extends KOI8-R by replacing the graphical characters
   //   at 0x80 through 0xBF by a mix of letters and punctuation marks
   for (unsigned i = 0x80 ; i < 0xC0 ; i++)
      {
      if (CP1251_alphanum[i-0x80])
	 m_codes[i].init(1,0,0) ;
      }
   m_codes[0x97].init(1,0,0) ;
   m_codes[0xFF].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetCP1251::isAlphaNum(wchar_t codepoint) const
{
   if (codepoint >= 0xC0)
      return true ;
   else if (codepoint >= 0x80)
      return CP1251_alphanum[codepoint-0x80] ;
   else if (isascii(codepoint))
      return isalnum(codepoint) ;
   else
      return false ;
}

/************************************************************************/
/*	Methods for class CharacterSetCP1252 (Windows Western)		*/
/************************************************************************/

CharacterSetCP1252::CharacterSetCP1252(const char *name, const char *enc)
   : CharacterSetLatin1(name,enc)
{
   // CP1252 is a superset of ISO-8859-1, adding printable characters to most
   //   of the C1 range (0x80-0x9F), but we'll only add the eight alphabetic
   //   characters to reduce false positives
   m_codes[0x83].init(1,0,0) ;
   m_codes[0x8A].init(1,0,0) ;
   m_codes[0x8C].init(1,0,0) ;
   m_codes[0x8E].init(1,0,0) ;
   m_codes[0x9A].init(1,0,0) ;
   m_codes[0x9C].init(1,0,0) ;
   m_codes[0x9E].init(1,0,0) ;
   m_codes[0x9F].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetCP1252::isAlphaNum(wchar_t codepoint) const
{
   if (codepoint == 0x83 || codepoint == 0x8A || codepoint == 0x8C ||
       codepoint == 0x8E || codepoint == 0x9A || codepoint == 0x9C ||
       codepoint == 0x9E || codepoint == 0x9F)
      return true ;
   else
      return CharacterSetLatin1::isAlphaNum(codepoint) ;
}

//----------------------------------------------------------------------

bool CharacterSetCP1252::writeAsUTF(FILE *fp, const unsigned char *buffer,
				    size_t buflen, bool romanize,
				    OutputFormat fmt)
{
   // override the Latin-1 version and revert to default
   return CharacterSet::writeAsUTF(fp,buffer,buflen,romanize,fmt) ;
}

/************************************************************************/
/*	Methods for class CharacterSetCP1255 (Windows Hebrew)		*/
/************************************************************************/


CharacterSetCP1255::CharacterSetCP1255(const char *name, const char *enc)
   : CharacterSetISO8859_8(name,enc)
{
   // CP1255 is a superset of ISO-8859-8, adding vowel points at 0xC0
   //   through 0xD8
   for (unsigned i = 0xC0 ; i <= 0xD8 ; i++)
      {
      if (i != 0xCA)
	 m_codes[i].init(1,0,0) ;
      }
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetCP1255::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else if (codepoint >= 0xC0 && codepoint <= 0xD8 && codepoint != 0xCA)
      return true ;
   else if (codepoint >= 0xE0 && codepoint <= 0xFA)
      return true ;
   else
      return false ;
}

/************************************************************************/
/*	Methods for class CharacterSetCP1256 (Windows Arabic)		*/
/************************************************************************/

CharacterSetCP1256::CharacterSetCP1256(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // although all high-bit characters are valid in CP-1256, we'll only mark
   //   the alphanumeric characters to avoid excessive false positives,
   //   skipping 0xFF because it is so frequent in binary files
   for (unsigned i = 0x80 ; i <= 0xFF ; i++)
      {
      if (CP1256_alphanum[i-0x80])
	 m_codes[i].init(1,0,0) ;
      }
   // add the LTR and RTL markers as valid characters
   m_codes[0xFD].init(1,0,0) ;
   m_codes[0xFE].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetCP1256::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else if (codepoint >= 0x80)
      return CP1256_alphanum[codepoint-0x80] ;
   else
      return false ;
}

/************************************************************************/
/*	Methods for class CharacterSetArmSCII8				*/
/************************************************************************/

CharacterSetArmSCII8::CharacterSetArmSCII8(const char *name, const char *enc)
   : CharacterSetLatin1(name,enc)
{
   m_codes[0xFF].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetArmSCII8::isAlphaNum(wchar_t codepoint) const
{
   // FIXME: use real alphanum table instead of the Latin-1 set
   return CharacterSetLatin1::isAlphaNum(codepoint) ;
}

//----------------------------------------------------------------------

bool CharacterSetArmSCII8::writeAsUTF(FILE *fp, const unsigned char *buffer,
				      size_t buflen, bool romanize,
				      OutputFormat fmt)
{
   // override the Latin-1 version and revert to default
   return CharacterSet::writeAsUTF(fp,buffer,buflen,romanize,fmt) ;
}

/************************************************************************/
/*	Methods for class CharacterSetMacCyrillic			*/
/************************************************************************/

CharacterSetMacCyrillic::CharacterSetMacCyrillic(const char *name, const char *enc)
   : CharacterSetLatin1(name,enc)
{
   // FIXME: adjust valid characters
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetMacCyrillic::isAlphaNum(wchar_t codepoint) const
{
   // FIXME: use real alphanum table instead of the Latin-1 set
   return CharacterSetLatin1::isAlphaNum(codepoint) ;
}

//----------------------------------------------------------------------

bool CharacterSetMacCyrillic::writeAsUTF(FILE *fp, const unsigned char *buffer,
					 size_t buflen, bool romanize,
					 OutputFormat fmt)
{
   // override the Latin-1 version and revert to default
   return CharacterSet::writeAsUTF(fp,buffer,buflen,romanize,fmt) ;
}

/************************************************************************/
/*	Methods for class CharacterSetTIS620				*/
/************************************************************************/

CharacterSetTIS620::CharacterSetTIS620(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // TIS-620 extends ASCII with Thai letters at 0xA1 to 0xDA and 0xDF to 0xFB
   // ISO-8859-11 is TIS-620 with nobreak-space at 0xA0
   // Windows CP874 is ISO-8859-11 plus a few additional punctuation marks at
   //   0x85 and 0x91 to 0x97
   for (unsigned i = 0xA0 ; i <= 0xDA ; i++)
      m_codes[i].init(1,0,0) ;
   for (unsigned i = 0xDF ; i <= 0xFB ; i++)
      m_codes[i].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetTIS620::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else if (codepoint >= 0xDB && codepoint <= 0xDE)
      return false ;
   return (codepoint >= 0xA1 && codepoint <= 0xFB) ;
}

/************************************************************************/
/*	Methods for class CharacterSetTSCII				*/
/************************************************************************/

CharacterSetTSCII::CharacterSetTSCII(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // TSCII extends ASCII with Tamil letters at practically every position in
   //   0x80 to 0xFF (ouch!)
   for (unsigned i = 0x80 ; i <= 0x91 ; i++)
      m_codes[i].init(1,0,0) ;
   for (unsigned i = 0x95 ; i <= 0xFE ; i++)
      {
      if (i != 0xAD)
	 m_codes[i].init(1,0,0) ;
      }
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetTSCII::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   if (codepoint >= 0x91 && codepoint <= 0x94)
      return false ;
   if (codepoint == 0xA0 || codepoint == 0xA9 || codepoint == 0xAD ||
       codepoint == 0xFF)
      return false ;
   return codepoint >= 0x80 ;
}

/************************************************************************/
/*	Methods for class CharacterSetISCII				*/
/************************************************************************/

CharacterSetISCII::CharacterSetISCII(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // ISCII extends ASCII with Devanagari plus some control codes at 0xA1
   //   through 0xEA and 0xEF through 0xFA
   for (unsigned i = 0xA1 ; i <= 0xEA ; i++)
      m_codes[i].init(1,0,0) ;
   for (unsigned i = 0xEF ; i <= 0xFA ; i++)
      m_codes[i].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetISCII::isAlphaNum(wchar_t codepoint) const
{
   if (codepoint >= 0xA1 && codepoint <= 0xD8)
      return true ;
   else if (codepoint >= 0xDA && codepoint <= 0xEA)
      return true ;
   else if (codepoint >= 0xF1 && codepoint<= 0xFA)
      return true ;
   else if (isascii(codepoint))
      return isalnum(codepoint) ;
   return false ;
}

//----------------------------------------------------------------------

static const wchar_t ISCII_to_Unicode_table[] =
   { 0x0950, 0x0901, 0x0902, 0x0903, 0x0905, 0x0906, 0x0907, 0x0908, //A0-A7
     0x0909, 0x090A, 0x090B, 0x090E, 0x090F, 0x0910, 0x090D, 0x0912, //A8-AF
     0x0913, 0x0914, 0x0911, 0x0915, 0x0916, 0x0917, 0x0918, 0x0919,
     0x091A, 0x091B, 0x091C, 0x091D, 0x091E, 0x091F, 0x0920, 0x0921,
     0x0922, 0x0923, 0x0924, 0x0925, 0x0926, 0x0927, 0x0928, 0x0929,
     0x092A, 0x092B, 0x092C, 0x092D, 0x092E, 0x092F, 0x095F, 0x0930,
     0x0931, 0x0932, 0x0933, 0x0934, 0x0935, 0x0936, 0x0937, 0x0938,
     0x0939, 0x25CC, 0x093E, 0x093F, 0x0940, 0x0941, 0x0942, 0x0943,
     0x0946, 0x0947, 0x0948, 0x0945, 0x094A, 0x094B, 0x094C, 0x0949,
     0x094D, 0x093C, 0x0964, 0x00EB, 0x00EC, 0x00ED, 0x00EE, '/',
     0x00F0, 0x0966, 0x0967, 0x0968, 0x0969, 0x096A, 0x096B, 0x096C,
     0x096D, 0x096E, 0x096F, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF
   } ;

bool CharacterSetISCII::writeAsUTF(FILE *fp, const unsigned char *buffer,
				   size_t buflen, bool romanize,
				   OutputFormat fmt)
{
   for (size_t i = 0 ; i < buflen ; i++)
      {
      wchar_t codepoint = buffer[i] ;
      if (codepoint >= 0xA0)
	 codepoint = ISCII_to_Unicode_table[codepoint-0xA0] ;
      if (!writeUTF(fp,codepoint,romanize,fmt))
	 return false ;
      }
   return true ;
}

/************************************************************************/
/*	Methods for class CharacterSetVISCII				*/
/************************************************************************/

CharacterSetVISCII::CharacterSetVISCII(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // VISCII extends ASCII with Vietnamese letters at every position in
   //   0x80 to 0xFF (ouch!) and on six control codes (double ouch!)
   for (unsigned i = 0x80 ; i <= 0xFF ; i++)
      m_codes[i].init(1,0,0) ;
   m_codes[0x02].init(1,0,0) ;
   m_codes[0x05].init(1,0,0) ;
   m_codes[0x06].init(1,0,0) ;
   m_codes[0x14].init(1,0,0) ;
   m_codes[0x19].init(1,0,0) ;
   m_codes[0x1E].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetVISCII::isAlphaNum(wchar_t codepoint) const
{
   if (codepoint == 0x02 || codepoint == 0x05 || codepoint == 0x06 ||
       codepoint == 0x14 || codepoint == 0x19 || codepoint == 0x1E)
      return true ;
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   return codepoint >= 0x80 ;
}

/************************************************************************/
/*	Methods for class CharacterSetGB2312				*/
/************************************************************************/

CharacterSetGB2312::CharacterSetGB2312(const char *name, const char *enc)
   : CharacterSet(name,enc)
{
   // bytes with high bit clear are single-byte ASCII, so initialize them
   //   that way
   initialize_ASCII(m_codes) ;
   // bytes 0xA1 through 0xF7 introduce two-byte characters
   for (unsigned i = 0xA1 ; i <= 0xF7 ; i++)
      m_codes[i].init(2,0xA1,0xFE) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetGB2312::nextCodePoint(const unsigned char *s,
				      wchar_t &codepoint,
				      EscapeState &) const
{
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   wchar_t cp = s[0] ;
   if (len > 1)
      {
      codepoint = 128 + ((s[0] - 0xA1) * (0xFE - 0xA0)) + (s[1] - 0xA1) ;
      }
   codepoint = cp ;
   return len ;
}

//----------------------------------------------------------------------

bool CharacterSetGB2312::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   //FIXME: need tables for what non-ASCII chars are alphanumeric
   // for now, assume they all are, to avoid breaking the alphanum filter
   return codepoint >= 128 ;
}

/************************************************************************/
/*	Methods for class CharacterSetGBK				*/
/************************************************************************/

CharacterSetGBKlevel1::CharacterSetGBKlevel1(const char *name, const char *enc)
   : CharacterSet(name,enc)
{
   // bytes with high bit clear are single-byte ASCII, so initialize them
   //   that way
   initialize_ASCII(m_codes) ;
   // bytes 0xA1 through 0xA9 introduce two-byte characters
   for (unsigned i = 0xA1 ; i <= 0xA9 ; i++)
      m_codes[i].init(2,0xA1,0xFE) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetGBKlevel1::nextCodePoint(const unsigned char *s,
					 wchar_t &codepoint,
					 EscapeState &) const
{
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   wchar_t cp = s[0] ;
   if (len > 1)
      {
      if (s[1] == 0x7F)
	 {
	 codepoint = 0 ;
	 return 0 ;
	 }
      // codepoint computation is based on full range of GBK so that we
      //   don't need separate versions for the different levels
      codepoint = 128 + (s[0] - 0x81) * 192 + (s[1] - 0x40) ;
      }
   codepoint = cp ;
   return len ;
}

//----------------------------------------------------------------------

bool CharacterSetGBKlevel1::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   //FIXME: need tables for what non-ASCII chars are alphanumeric
   // for now, assume they all are, to avoid breaking the alphanum filter
   return codepoint >= 128 ;
}

/************************************************************************/
/*	Methods for class CharacterSetGBKlevel2				*/
/************************************************************************/

CharacterSetGBKlevel2::CharacterSetGBKlevel2(const char *name, const char *enc)
   : CharacterSetGBKlevel1(name,enc)
{
   // level 2: bytes 0xB0 through 0xF7
   for (unsigned i = 0xB0 ; i <= 0xF7 ; i++)
      m_codes[i].init(2,0xA1,0xFE) ;
   return ;
}

/************************************************************************/
/*	Methods for class CharacterSetGBKlevel3				*/
/************************************************************************/

CharacterSetGBKlevel3::CharacterSetGBKlevel3(const char *name, const char *enc)
   : CharacterSetGBKlevel2(name,enc)
{
   // level 3: bytes 0x81 through 0xA0
   for (unsigned i = 0x81 ; i <= 0xA0 ; i++)
      m_codes[i].init(2,0x40,0xFE) ;
   return ;
}

/************************************************************************/
/*	Methods for class CharacterSetGBK				*/
/************************************************************************/

CharacterSetGBK::CharacterSetGBK(const char *name, const char *enc)
   : CharacterSetGBKlevel3(name,enc)
{
   // level 4: extends range for bytes 0xAA through 0xFE
   for (unsigned i = 0xAA ; i <= 0xFE ; i++)
      m_codes[i].init(2,0x40,0xFE) ;
   // level 5: extends range for bytes 0xA8 and 0xA9
   m_codes[0xA8].init(2,0x40,0xFE) ;
   m_codes[0xA9].init(2,0x40,0xFE) ;
   return ;
}

/************************************************************************/
/*	Methods for class CharacterSetGB18030				*/
/************************************************************************/

CharacterSetGB18030::CharacterSetGB18030(const char *name, const char *enc)
   : CharacterSet(name,enc)
{
   // bytes with high bit clear are single-byte ASCII, so initialize them
   //   that way
   initialize_ASCII(m_codes) ;
   // compared to GBK, GB18030 extends the range for bytes 0x81-0xFE
   //   down to 0x30 to allow digits as the second byte in four-byte
   //   characters (two pairs of firstbyte+digit), but we'll treat
   //   them as two-byte units here to avoid complicating the code
   for (unsigned i = 0x81 ; i <= 0xFE ; i++)
      m_codes[i].init(2,0x30,0xFE) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetGB18030::nextCodePoint(const unsigned char *s,
				       wchar_t &codepoint, 
				       EscapeState &) const
{
   if (!validSuccessors(s))
      {
      codepoint = 0 ;
      return 0 ;
      }
   int len = m_codes[s[0]].length() ;
   if (len == 2 && s[1] >= 0x30 && s[1] <= 0x39)
      {
      // it's the first half of a four-byte character, so check if the
      //   rest is valid
      if (s[2] >= 0x81 && s[2] <= 0xFE && s[3] >= 0x30 && s[3] <= 0x39)
	 {
	 len = 4 ;
	 unsigned code1 = (s[0] - 0x81) * 10 + (s[1] - 0x30) ;
	 unsigned code2 = (s[2] - 0x81) * 10 + (s[3] - 0x30) ;
	 unsigned twobyte_codes = (0xFE - 0x81) * 192 ;
	 codepoint = 128 + twobyte_codes + code1 * 10 * (0xFE - 0x80) + code2 ;
	 }
      else
	 {
	 codepoint = 0 ;
	 return 0 ;
	 }
      }
   else if (len == 2)
      {
      if (s[1] < 0x40)
	 {
	 codepoint = 0 ;
	 return 0 ;
	 }
      else
	 {
	 codepoint = 128 + (s[0] - 0x81) * 192 + (s[1] - 0x40) ;
	 }
      }
   else
      codepoint = len ? s[0] : 0 ;
   return len ;
}

//----------------------------------------------------------------------

bool CharacterSetGB18030::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   //FIXME: need tables for what non-ASCII chars are alphanumeric
   // for now, assume they all are, to avoid breaking the alphanum filter
   return codepoint >= 128 ;
}

/************************************************************************/
/*	Methods for class CharacterSetBig5				*/
/************************************************************************/

CharacterSetBig5::CharacterSetBig5(const char *name, const char *enc)
   : CharacterSet(name,enc)
{
   // bytes with high bit clear are single-byte ASCII, so initialize
   //   them that way
   initialize_ASCII(m_codes) ;
   // bytes 0xA1 through 0xF9 are the first of two bytes
   for (unsigned i = 0xA1 ; i <= 0xF9 ; i++)
      m_codes[i].init(2,0x40,0xFE) ;
   return ;
}

//----------------------------------------------------------------------

int CharacterSetBig5::nextCodePoint(const unsigned char *s,
				    wchar_t &codepoint,
				    EscapeState &) const
{
   if (!s)
      return -1 ;
   int len = m_codes[s[0]].length() ;
   wchar_t cp = s[0] ;
   if (len == 2)
      {
      if (!m_codes[s[0]].validSuccessor(s[1]) ||
	  (s[1] > 0x7E && s[1] < 0xA1))
	 {
	 cp = 0 ;
	 }
      else
	 {
	 unsigned byte1 = s[0] - 0x81 ;
	 unsigned byte2 = s[1] - 0x40 ;
	 if (byte2 > (0x7E - 0x40))
	    byte2 -= (0xA1 - 0x7E - 1) ;
	 cp = 128 + (byte1 * 156 + byte2) ;
	 }
      }
   codepoint = cp ;
   return len ;
}

//----------------------------------------------------------------------

/*
Wikipedia says:
0x81/40  (128) to 0xa0/fe user-defined
0xa1/40 (5120) to 0xa3/bf graphics
0xa3/c0 () to 0xa3/fe reserved
0xa4/40 (5588) to 0xc6/7e frequently used
0xc6/a1 to 0xc8/fe user-defined
0xc9/40 to 0xf9/d5 less-frequently used
0xf9/d6 to 0xfe/fe user-defined
*/

bool CharacterSetBig5::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   if (codepoint < 5588)
      return false ;
   if (codepoint < 128 + 126 * 156)
      return true ;
   return false ; //FIXME: need full list of non-ASCII alphanum codepoints
}

/************************************************************************/
/*	Methods for class CharacterSetBig5Ext				*/
/************************************************************************/

CharacterSetBig5Ext::CharacterSetBig5Ext(const char *name, const char *enc)
   : CharacterSetBig5(name,enc)
{
   // bytes with high bit clear are single-byte ASCII, so initialize
   //   them that way
   initialize_ASCII(m_codes) ;
   // bytes 0x81 through 0xFE are the first of two bytes
   for (unsigned i = 0x81 ; i <= 0xFE ; i++)
      m_codes[i].init(2,0x40,0xFE) ;
   return ;
}

//----------------------------------------------------------------------

// using the nextCodePoint() and encodingSize() from CharacterSetBig5

//----------------------------------------------------------------------

bool CharacterSetBig5Ext::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   if (codepoint < 5588)
      return false ;
   if (codepoint < 128 + 126 * 156)
      return true ;
   return false ; //FIXME: need full list of non-ASCII alphanum codepoints
}

/************************************************************************/
/*	Methods for class CharacterISO6937				*/
/************************************************************************/

CharacterSetISO6937::CharacterSetISO6937(const char *name, const char *enc)
   : CharacterSet(name,enc)
{
   // bytes with high bit clear are single-byte ASCII, so initialize
   //   them that way
   initialize_ASCII(m_codes) ;
   // bytes 0xA0 through 0xBF are mostly punctuation and symbols
   for (unsigned i = 0xA0 ; i <= 0xBF ; i++)
      {
      if (i != 0xA4 && i != 0xA6)
	 m_codes[i].init(1,0,0) ;
      }
   // bytes 0xC1 through 0xCF (except 0xC9 and 0xCC) are the first of two bytes
   for (unsigned i = 0xC1 ; i <= 0xCF ; i++)
      {
      if (i != 0xC9 && i != 0xCC)
	 m_codes[i].init(2,0x41,0x7A) ;
      }
   // btyes 0xD0 through 0xDF are mostly symbols
   // btyes 0xE0 through 0xFE are mostly special letter forms
   for (unsigned i = 0xD0 ; i <= 0xFE ; i++)
      {
      if ((i < 0xD8 && i > 0xDC) && i != 0xE5)
	 m_codes[i].init(1,0,0) ;
      }
   return ;
}

//----------------------------------------------------------------------

int CharacterSetISO6937::nextCodePoint(const unsigned char *s,
				       wchar_t &codepoint,
				       EscapeState &) const
{
   if (!s)
      return -1 ;
   int len = m_codes[s[0]].length() ;
   wchar_t cp = s[0] ;
   if (len == 2)
      {
      if (m_codes[s[0]].validSuccessor(s[1]))
	 {
	 cp = 256 + ((s[0] - 0xC1) * 64) + (s[1] - 0x41) ;
	 }
      else
	 cp = 0 ;
      }
   codepoint = cp ;
   return len ;
}

//----------------------------------------------------------------------

bool CharacterSetISO6937::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   if (codepoint > 255)
      return true ;			// all double-byte chars are alphabetic
   else if (codepoint >= 0xE0)
      {
      return (codepoint != 0xFF && codepoint != 0xE3 && codepoint != 0xE5 &&
	      codepoint != 0xEB) ;
      }
   return false ;
}

/************************************************************************/
/*	Methods for class CharacterGEOSTD8				*/
/************************************************************************/

CharacterSetGEOSTD8::CharacterSetGEOSTD8(const char *name, const char *enc)
   : CharacterSetASCII(name,enc)
{
   // bytes 0xC0 through 0xE5 are Georgian characters
   for (unsigned i = 0xC0 ; i <= 0xE5 ; i++)
      {
      m_codes[i].init(1,0,0) ;
      }
   // byte 0xFD is a Georgian punctuation mark (== U2116)
   m_codes[0xFD].init(1,0,0) ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetGEOSTD8::isAlphaNum(wchar_t codepoint) const
{
   if (isascii(codepoint))
      return isalnum(codepoint) ;
   else
      return (codepoint >= 0xC0 && codepoint <= 0xE5) ;
}

/************************************************************************/
/*	Methods for class CharacterSet				        */
/************************************************************************/

CharacterSet::CharacterSet(const char *name, const char *encoding)
{
   if (!encoding || !*encoding)
      encoding = default_encoding ;
   m_encname = FrDupString(name) ;
   m_encoding = FrDupString(encoding) ;
   m_desired = 0 ;
   m_wordlist = 0 ;
   m_iconv = (iconv_t)-1 ;
   m_iconv_available = true ;
   m_newline_OK = false ;
   return ;
}

//----------------------------------------------------------------------

CharacterSet::~CharacterSet()
{
   FrFree(m_encname) ;
   m_encname = 0 ;
   FrFree(m_encoding) ;
   m_encoding = 0 ;
   delete m_desired ;
   m_desired = 0 ;
   delete m_wordlist ;
   m_wordlist = 0 ;
#ifndef NO_ICONV
   if (m_iconv != (iconv_t)-1) iconv_close(m_iconv) ;
#endif /* NO_ICONV */
   return ;
}

//----------------------------------------------------------------------

const char *CharacterSet::normalizedEncodingName(const char *encoding)
{
   for (int i = 0 ; sets[i].name ; i++)
      {
      if (strcasecmp(sets[i].name,encoding) == 0)
	 {
	 return sets[i].name ;
	 }
      }
   size_t enc_len = strlen(encoding) ;
   for (int i = 0 ; sets[i].name ; i++)
      {
      size_t name_len = strlen(sets[i].shortname) ;
      if (strncasecmp(sets[i].shortname,encoding,name_len) == 0 &&
	  name_len >= enc_len &&
	  // case-sensitive for single-char short name
	  (sets[i].shortname[1] || sets[i].shortname[0] == encoding[0]))
	 {
	 return sets[i].name ;
	 }
      }
   // no matching encoding
   return 0 ;
}

//----------------------------------------------------------------------

CharacterSet *CharacterSet::makeCharSet(const char *encoding)
{
   if (strcasecmp(encoding,"HELP") == 0 || strcasecmp(encoding,"list") == 0)
      {
      int len = 30 ;
      cout << "The known character sets are:" ;
      for (int i = 0 ; sets[i].name ; i++)
	 {
	 if (len > 65)
	    {
	    cout << endl ;
	    len = 0 ;
	    }
	 else
	    cout << ' ' ;
	 cout << sets[i].name ;
	 len += strlen(sets[i].name) + 1 ;
	 if (strlen(sets[i].shortname) == 1)
	    {
	    cout << " (" << sets[i].shortname << ")" ;
	    len += 4 ;
	    }
	 }
      cout << endl << endl ;
      return 0 ;
      }
   for (int i = 0 ; sets[i].name ; i++)
      {
      if (strcasecmp(sets[i].name,encoding) == 0)
	 {
	 return sets[i].creator->makeSet(encoding,sets[i].enc_class) ;
	 }
      }
   size_t enc_len = strlen(encoding) ;
   for (int i = 0 ; sets[i].name ; i++)
      {
      size_t name_len = strlen(sets[i].shortname) ;
      if (strncasecmp(sets[i].shortname,encoding,name_len)== 0 &&
	  name_len >= enc_len &&
	  // case-sensitive for single-char short name
	  (sets[i].shortname[1] || sets[i].shortname[0] == encoding[0]))
	 {
	 return sets[i].creator->makeSet(encoding,sets[i].enc_class) ;
	 }
      }
   cout << "Unrecognized character set '" << encoding
	<< "', use -eLIST for a list of known sets." << endl ;
   return 0 ;
}

//----------------------------------------------------------------------

CharacterSet **CharacterSet::makeCharSets(const char *encoding)
{
   if (encoding && strcasecmp(encoding,"auto") == 0)
      {
      CharacterSet **charsets = FrNewC(CharacterSet*,1) ;
      return charsets ;
      }
   if (!encoding || !*encoding)
      return 0 ;
   char *enc_list = FrDupString(encoding) ;
   if (!enc_list)
      return 0 ;
   char *comma ;
   // count the number of encodings requested, which is one more than the
   //   number of commas
   unsigned count = 1 ;
   for (encoding = enc_list ; *encoding ; encoding++)
      {
      if (*encoding == ',')
	 count++ ;
      }
   CharacterSet **charsets = FrNewC(CharacterSet*,count+1) ;
   if (!charsets)
      {
      FrFree(enc_list) ;
      return 0 ;
      }
   encoding = enc_list ;
   count = 0 ;
   while ((comma = strchr((char*)encoding,',')) != 0)
      {
      *comma = '\0' ;
      CharacterSet *cs = makeCharSet(encoding) ;
      if (cs)
	 charsets[count++] = cs ;
      encoding = comma + 1 ;
      }
   if (*encoding)
      {
      charsets[count] = makeCharSet(encoding) ;
      }
   FrFree(enc_list) ;
   return charsets ;
}

//----------------------------------------------------------------------

void CharacterSet::allowNewlines()
{
   // figure out what length to use, so that we can handle the case
   //   where all characters consume multiple bytes
   int len = m_codes['\t'].length() ;
   if (len == 0)
      len = m_codes[' '].length() ;
   if (len == 0)
      len = 1 ;
   m_codes['\r'].init(len,0,0) ;
   m_codes['\n'].init(len,0,0) ;
   m_newline_OK = true ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSet::setLanguage(const char *language)
{
   if (language && *language)
      {
      m_desired = new CodePoints(this,language) ;
      }
   return true ;
}

//----------------------------------------------------------------------

bool CharacterSet::setLanguageFromFile(const char *language_file)
{
   if (language_file && *language_file)
      {
      m_desired = new CodePoints(this,language_file,true) ;
      }
   return true ;
}

//----------------------------------------------------------------------

bool CharacterSet::setLanguageChars(const char *language_chars)
{
   if (language_chars && *language_chars)
      {
      m_desired = new CodePoints(this,language_chars,false) ;
      return m_desired != 0 ;
      }
   return true ;
}

//----------------------------------------------------------------------

bool CharacterSet::loadWordList(const char *wordlist_file, bool verbose)
{
   if (wordlist_file && *wordlist_file)
      {
      m_wordlist = new NybbleTrie(wordlist_file,verbose) ;
      }
   return true ;
}

//----------------------------------------------------------------------

bool CharacterSet::validSuccessors(const unsigned char *s) const
{
   if (!s)
      return false ;
   int len = m_codes[s[0]].length() ;
   if (len == 0)
      return false ;
   for (int i = 1 ; i < len ; i++)
      {
      if (!m_codes[s[0]].validSuccessor(s[i]))
	 return false ;
      }
   return true ;
}

//----------------------------------------------------------------------

NybbleTriePointer *CharacterSet::dictionary() const
{
   return m_wordlist ? new NybbleTriePointer(m_wordlist) : 0 ;
}

//----------------------------------------------------------------------

bool CharacterSet::initializeIconv()
{
   if (!m_iconv_available)
      return false ;
   if (m_iconv == (iconv_t)-1)
      {
#ifndef NO_ICONV
      // iconv() is not thread-safe
      FrCRITSECT_ENTER(iconv_mutex) ;
      m_iconv = iconv_open("utf8//TRANSLIT",encodingName()) ;
      if (m_iconv == (iconv_t)-1)
	 m_iconv = iconv_open("utf8//TRANSLIT",encoding()) ;
      FrCRITSECT_LEAVE(iconv_mutex) ;
      if (m_iconv == (iconv_t)-1)
#endif /* !NO_ICONV */
	 {
	 m_iconv_available = false ;
	 return false ;
	 }
      }
#ifndef NO_ICONV
   if (m_iconv != (iconv_t)-1)
      {
      // reset conversion state
      // iconv() is not thread-safe
      FrCRITSECT_ENTER(iconv_mutex) ;
      iconv(m_iconv,0,0,0,0) ;
      FrCRITSECT_LEAVE(iconv_mutex) ;
      }
#endif /* !NO_ICONV */
   return true ;
}

//----------------------------------------------------------------------

bool CharacterSet::writeUTF(FILE *fp, wchar_t codepoint, bool romanize,
			    OutputFormat fmt) const
{
   if (romanize)
      {
      wchar_t romanized1 ;
      wchar_t romanized2 ;
      unsigned cpoints = romanize_codepoint(codepoint,romanized1,romanized2) ;
      bool success = writeUTF(fp,romanized1,false,fmt) ;
      if (cpoints == 2 && !writeUTF(fp,romanized2,false,fmt))
	 success = false ;
      return success ;
      }
   bool byteswap = false ;
   char buffer[8] ;
   int bytes = 0 ;
   if (fmt == OF_UTF16BE || fmt == OF_UTF16LE)
      {
      if (codepoint > 0xFFFF)
	 {
	 // write two surrogate code points to represent the value
	 //  that doesn't fit into 16 bits
	 return (writeUTF(fp,0xD800 + ((codepoint >> 10) & 0x3FF),false,fmt) &&
		 writeUTF(fp,0xDC00 + (codepoint & 0x3FF),false,fmt)) ;
	 }
      uint8_t hi = (uint8_t)(codepoint >> 8) ;
      uint8_t lo = (uint8_t)(codepoint & 0xFF) ;
      if (fmt == OF_UTF16BE)
	 {
	 return (fputc(hi,fp) != EOF && fputc(lo,fp) != EOF) ;
	 }
      else
	 {
	 return (fputc(lo,fp) != EOF && fputc(hi,fp) != EOF) ;
	 }
      }
   else
      {
      bytes = Fr_Unicode_to_UTF8((FrChar16)codepoint,buffer,byteswap) ;
      if (bytes > 0)
	 return (int)fwrite(buffer,sizeof(char),bytes,fp) == bytes ;
      else
	 return false ;
      }
}

//----------------------------------------------------------------------

uint32_t get_UTF8(char *&string)
{
   uint32_t codepoint ;
   uint8_t leadchar = *((uint8_t*)string) ;
   if ((leadchar & 0x80) == 0)
      {
      codepoint = *string++ ;
      }
   else
      {
      unsigned extra = 0 ;
      string++ ;
      if ((leadchar & 0xE0) == 0xC0)
	 {
	 codepoint = leadchar & 0x1F ;
	 extra = 1 ;
	 }
      else if ((leadchar & 0xF0) == 0xE0)
	 {
	 codepoint = leadchar & 0x0F ;
	 extra = 2 ;
	 }
      else if ((leadchar & 0xF8) == 0xF0)
	 {
	 codepoint = leadchar & 0x07 ;
	 extra = 3 ;
	 }
      else if ((leadchar & 0xFC) == 0xF8)
	 {
	 codepoint = leadchar & 0x03 ;
	 extra = 4 ;
	 }
      else if ((leadchar & 0xFE) == 0xFC)
	 {
	 codepoint = leadchar & 0x01 ;
	 extra = 5 ;
	 }
      else
	 {
	 return 0xFFFD ;  // replacement for unknown char
	 }
      for (unsigned i = 0 ; i < extra ; i++)
	 {
	 uint8_t ch = *((uint8_t*)string) ;
	 string++ ;
	 if ((ch & 0xC0) != 0x80)
	    {
	    return 0xFFFD ;  // invalid encoding!
	    }
	 codepoint = (codepoint << 6) | (ch & 0x3F) ;
	 }
      }
   return codepoint ;
}

//----------------------------------------------------------------------

static bool write_string(CharacterSet *cset, FILE *fp, char *bufstart,
			 const char *bufend, bool romanize, OutputFormat fmt)
{
   bool success = true ;
   while (bufstart < bufend)
      {
      uint32_t codepoint = get_UTF8(bufstart) ;
      if (!cset->writeUTF(fp,codepoint,romanize,fmt))
	 success = false ;
      }
   return success ;
}
		    
//----------------------------------------------------------------------

bool CharacterSet::convertToUTF8(const char *inbuf, size_t inlen,
				 char *outbuf, size_t outlen, bool romanize)
{
   if (initializeIconv())
      {
      char *inbuffer = (char*)inbuf ;
      char *convertbuf = outbuf ;
      size_t convertlen = outlen ;
      // iconv() is not thread-safe
      FrCRITSECT_ENTER(iconv_mutex) ;
      size_t count = iconv(m_iconv,&inbuffer,&inlen,&convertbuf,&convertlen) ;
      FrCRITSECT_LEAVE(iconv_mutex) ;
      bool success = count != (size_t)-1 ;
      if (!convertbuf)
	 convertbuf = outbuf ;
      *convertbuf = '\0' ;
      if (success && romanize)
	 {
	 FrLocalAlloc(char,tmpbuf,6 * FrMAX_LINE, 6 * outlen) ;
	 char *tmpptr = tmpbuf ;
	 bool byteswap = false ;
	 char *inptr = outbuf ;
	 while (inptr < convertbuf)
	    {
	    uint32_t codepoint = get_UTF8(inptr) ;
	    wchar_t romanized1, romanized2 ;
	    unsigned cpoints = romanize_codepoint(codepoint,romanized1,romanized2) ;
	    unsigned bytes = Fr_Unicode_to_UTF8((FrChar16)romanized1,tmpptr,byteswap) ;
	    tmpptr += bytes ;
	    if (cpoints == 2)
	       {
	       bytes = Fr_Unicode_to_UTF8((FrChar16)romanized2,tmpptr,byteswap) ;
	       tmpptr += bytes ;
	       }
	    }
	 unsigned rom_len = tmpptr - tmpbuf ;
	 if (rom_len >= outlen)
	    rom_len = outlen - 1 ;
	 memcpy(outbuf,tmpbuf,rom_len) ;
	 outbuf[rom_len] = '\0' ;
	 FrLocalFree(tmpbuf) ;
	 outlen -= rom_len ;
	 }
      if (count == (size_t)-1 && errno == EILSEQ && outlen > 0)
	 {
	 // we have unconvertible bytes, so add them to the buffer as-is
	 memcpy(convertbuf,inbuffer,convertlen < inlen ? convertlen : inlen) ;
	 convertbuf[convertlen-1] = '\0' ;
	 }
      return success ;
      }
   return false ;
}

//----------------------------------------------------------------------

bool CharacterSet::writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize,
			      OutputFormat fmt)
{
   if (filterNUL())
      {
      for (size_t i = 0 ; i < buflen ; i++)
	 {
	 if (buffer[i] && !writeUTF(fp,buffer[i],romanize,fmt))
	    return false ;
	 }
      return true ;
      }
#ifndef NO_ICONV
   else if (initializeIconv())
      {
      size_t convlen = 6 * buflen ;
      char *bufferptr = (char*)buffer ;
      size_t in_len = buflen ;
      FrLocalAlloc(char,convertbuf,6*FrMAX_LINE+2,convlen+2) ;
      char *convertptr = convertbuf ;
      // iconv() is not thread-safe
      FrCRITSECT_ENTER(iconv_mutex) ;
      size_t count = iconv(m_iconv,&bufferptr,&in_len,&convertptr,&convlen) ;
      FrCRITSECT_LEAVE(iconv_mutex) ;
      *convertptr = '\0' ;
      // output the successfully converted bytes
      bool success = false ;
      if (romanize)
	 {
	 success = write_string(this,fp,convertbuf,convertptr,true,fmt) ;
	 }
      if (!success)
	 {
	 success = write_string(this,fp,convertbuf,convertptr,false,fmt) ;
	 }
      FrLocalFree(convertbuf) ;
      if (count == (size_t)-1 && errno == EILSEQ && success)
	 {
	 // we have unconvertible bytes, so dump them as-is
	 success = fwrite(bufferptr,sizeof(char),in_len,fp) == in_len ;
	 }
      return success ;
      }
#endif /* !NO_ICONV */
   else
      {
      if (fp == stdout)
	 {
	 // strip out NUL bytes, as those cause problems with output
	 for (size_t i = 0 ; i < buflen ; i++)
	    {
	    if (buffer[i] && !fputc(buffer[i],fp))
	       return false ;
	    }
	 return true ;
	 }
      else
	 {
	 // the trivial conversion: write the buffer as-is
	 return fwrite(buffer,sizeof(char),buflen,fp) == buflen ;
	 }
      }
}

//----------------------------------------------------------------------

bool CharacterSet::romanizable(const unsigned char *buffer, size_t buflen) const
{
#ifndef NO_ICONV
   if (m_iconv != (iconv_t)-1)
      {
      size_t convlen = 6 * buflen ;
      char *bufferptr = (char*)buffer ;
      size_t in_len = buflen ;
      FrLocalAlloc(char,convertbuf,6*FrMAX_LINE+1,convlen+1) ;
      char *convertptr = convertbuf ;
      // iconv() is not thread-safe
      FrCRITSECT_ENTER(iconv_mutex) ;
      (void)iconv(m_iconv,&bufferptr,&in_len,&convertptr,&convlen) ;
      FrCRITSECT_LEAVE(iconv_mutex) ;
      size_t out_len = (convertptr - convertbuf) ;
      size_t pos = 0 ;
      bool can_romanize = false ;
      while (pos < out_len)
	 {
	 wchar_t codepoint ;
	 int len = get_UTF8_codepoint(convertbuf+pos,codepoint) ;
	 if (len == 0)
	    break ;
	 if (romanizable_codepoint(codepoint))
	    {
	    can_romanize = true ;
	    break ;
	    }
	 pos += len ;
	 }
      FrLocalFree(convertbuf) ;
      return can_romanize ;
      }
#endif
   return false ;
}

//----------------------------------------------------------------------

int CharacterSet::consumeNewlines(const unsigned char *s, size_t buflen) const
{
   (void)s ; (void)buflen ;
   return 0 ;
}

/************************************************************************/
/*	Methods for class CharacterSetCache			        */
/************************************************************************/

CharacterSetCache::CharacterSetCache()
{
   m_size = 0 ;
   m_alloc = 0 ;
   m_charsets = 0 ;
   return ;
}

//----------------------------------------------------------------------

CharacterSetCache::~CharacterSetCache()
{
   for (size_t i = 0 ; i < m_alloc ; i++)
      {
      delete m_charsets[i] ;
      }
   FrFree(m_charsets) ;
   m_charsets = 0 ;
   m_size = 0 ;
   m_alloc = 0 ;
   return ;
}

//----------------------------------------------------------------------

bool CharacterSetCache::insert(CharacterSet **array, size_t &size,
			       CharacterSet *set)
{
//FIXME: do something more efficient than a strict linear search
   for (size_t i = 0 ; i < size ; i++)
      {
      if (set->encodingName() && array[i]->encodingName() &&
	  strcasecmp(set->encodingName(),array[i]->encodingName()) == 0)
	 return false ; // already cached
      }
   if (array[size] == 0)
      {
      array[size++] = set ;
      return true ;
      }
   return false ; // could not insert
}

//----------------------------------------------------------------------

bool CharacterSetCache::insertNew(CharacterSet **array, size_t &size,
				  CharacterSet *set)
{
   if (array[size] == 0)
      {
      array[size++] = set ;
      return true ;
      }
   return false ; // could not insert
}

//----------------------------------------------------------------------

CharacterSet *CharacterSetCache::locateNormalized(const char *encoding) const
{
//FIXME: do something more efficient than a strict linear search
   for (size_t i = 0 ; i < cacheSize() ; i++)
      {
      if (m_charsets[i] &&
	  strcasecmp(m_charsets[i]->encodingName(),encoding) == 0)
	 {
	 return m_charsets[i] ;
	 }
      }
   // if we get here, there was no match
   return 0 ;
}

//----------------------------------------------------------------------

CharacterSet *CharacterSetCache::locate(const char *encoding) const
{
   const char *norm = CharacterSet::normalizedEncodingName(encoding) ;
   if (norm)
      encoding = norm ;
//FIXME: do something more efficient than a strict linear search
   for (size_t i = 0 ; i < cacheSize() ; i++)
      {
      if (m_charsets[i] &&
	  strcasecmp(m_charsets[i]->encodingName(),encoding) == 0)
	 return m_charsets[i] ;
      }
   // if we get here, there was no match
   return 0 ;
}

//----------------------------------------------------------------------

bool CharacterSetCache::expand()
{
   if (cacheSize() >= cacheAllocated())
      {
      size_t new_alloc = cacheAllocated()<500 ? 500 : 2 * cacheAllocated() ;
      CharacterSet **new_charsets = FrNewC(CharacterSet*,new_alloc) ;
      if (!new_charsets)
	 return false ;
      size_t new_size = 0 ;
      for (size_t i = 0 ; i < m_alloc ; i++)
	 {
	 if (m_charsets[i])
	    {
	    if (!insert(new_charsets,new_size,m_charsets[i]))
	       delete m_charsets[i] ; // can't happen, but clean up if it does
	    }
	 }
      FrFree(m_charsets) ;
      m_charsets = new_charsets ;
      m_size = new_size ;
      m_alloc = new_alloc ;
      }
   return true ;
}

//----------------------------------------------------------------------

const CharacterSet *CharacterSetCache::getCharSet(const char *encoding)
{
   if (!encoding)
      return 0 ;
   const char *norm = CharacterSet::normalizedEncodingName(encoding) ;
   if (norm)
      encoding = norm ;
   CharacterSet *set = locateNormalized(encoding) ;
   if (set)
      return set ;
   // if we get here, the requested character set was not already cached,
   //   so create it and add it to the cache
   cache_critsect.acquire() ;
   set = CharacterSet::makeCharSet(encoding) ;
   expand() ;
   if (set)
      insert(m_charsets,m_size,set) ;
   cache_critsect.release() ;
   return set ;
}

//----------------------------------------------------------------------

CharacterSet **CharacterSetCache::getCharSets(const char *encoding_list)
{
   //FIXME
(void)encoding_list ;
   return 0 ;
}

//----------------------------------------------------------------------

void CharacterSetCache::freeCharSets(CharacterSet **sets)
{
   if (!sets)
      return ;
   // since we're caching the actual character sets, just free the array
   delete [] sets ;
   return ;
}

//----------------------------------------------------------------------

CharacterSetCache* CharacterSetCache::instance()
{
   if (!s_cache)
      s_cache = new CharacterSetCache() ;
   return s_cache ;
}

//----------------------------------------------------------------------

void CharacterSetCache::deallocate()
{
   delete s_cache ;
   s_cache = 0 ;
   return ;
}

/* end of file charset.C */
