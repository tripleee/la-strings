/************************************************************************/
/*                                                                      */
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     charset.h							*/
/*  Version:  1.22							*/
/*  LastEdit: 01jul2013							*/
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

#ifndef __CHARSET_H_INCLUDED
#define __CHARSET_H_INCLUDED

#include <stdint.h>
#include <wchar.h>
#include "language.h"
#include "langident/trie.h"

#ifdef NO_ICONV
typedef void *iconv_t ;
#else
# include <iconv.h>
#endif

using namespace std ;

/************************************************************************/
/************************************************************************/

// forward declaration, don't need to know details in this header
class CodePoints ;
class NybbleTrie ;
class NybbleTriePointer ;

//----------------------------------------------------------------------

enum EscapeState
  {
     ES_None, ES_Active, ES_Prefix1, ES_Prefix2, ES_Prefix3,
     ES_Suffix1, ES_Suffix2, ES_Suffix3
  } ;

//----------------------------------------------------------------------

class CharacterCode
   {
   private:
      uint8_t	    m_length ;		// total bytes for char (0 = invalid)
      unsigned char m_range_begin ;	// lowest value of successor bytes
      unsigned char m_range_end ;	// highest value of successor bytes
   public:
      CharacterCode()
	 { m_length = 0 ; m_range_begin = '\xFF' ; m_range_end = '\0' ; }
      CharacterCode(uint8_t len, unsigned char begin, unsigned char end)
	 { init(len,begin,end) ; }
      ~CharacterCode() {}
      void init(uint8_t len, unsigned char begin, unsigned char end)
         { m_length = len ; m_range_begin = begin ; m_range_end = end ; }

      // accessors
      unsigned int length() const { return m_length ; }
      bool validSuccessor(unsigned char c) const
         { return c >= m_range_begin && c <= m_range_end ; }
   } ;

//----------------------------------------------------------------------

class CharacterSet
   {
   protected:
      CharacterCode m_codes[256] ;
      char	   *m_encoding ;
      char	   *m_encname ;
      size_t	    m_numpoints ;
      CodePoints   *m_desired ;
      NybbleTrie   *m_wordlist ;  //FIXME?
      iconv_t	    m_iconv ;
      bool	    m_iconv_available ;
      bool	    m_newline_OK ;

   public:
      // constructors and destructors
      CharacterSet(const char *encname = 0, const char *encoding = 0) ;
      virtual ~CharacterSet() ;
      static CharacterSet *makeCharSet(const char *encoding) ;
      static CharacterSet **makeCharSets(const char *encoding_list) ;

      // configuration
      void allowNewlines() ;
      bool setLanguage(const char *language) ;
      bool setLanguageChars(const char *language_chars) ;
      bool setLanguageFromFile(const char *language_file) ;
      bool loadWordList(const char *wordlist_file, bool verbose) ;

      // accessors
      virtual size_t encodingSize() const = 0 ;
      virtual int nextCodePoint(const unsigned char *s,
				wchar_t &codepoint,
				EscapeState &in_escape_sequence) const = 0 ;
      virtual bool isAlphaNum(wchar_t codepoint) const = 0 ;
      virtual bool filterNUL() const { return false ; }
      virtual bool bigEndian() const { return false ; }
      virtual double detectionReliability() const { return 1.0 ; }
      virtual unsigned alignment() const { return 1 ; }
      virtual bool romanizable(const unsigned char *s, size_t buflen) const ;
      virtual int consumeNewlines(const unsigned char *s, size_t buflen) const ;
      bool newlineOK() const { return m_newline_OK ; }
      bool validSuccessors(const unsigned char *s) const ;
      bool desiredCodePoint(wchar_t codepoint) const
         { return m_desired ? m_desired->validPoint(codepoint) : true ; }
      const char *encodingName() const { return m_encname ; }
      const char *encoding() const { return m_encoding ; }
      NybbleTriePointer *dictionary() const ;
      static const char *normalizedEncodingName(const char *encoding) ;

      // output
      bool writeUTF(FILE *fp, wchar_t codepoint, bool romanize = false,
		    OutputFormat fmt = OF_UTF8) const ;
      bool convertToUTF8(const char *inbuffer, size_t inlen,
			 char *outbuffer, size_t outlen, bool romanize = false) ;
      virtual bool initializeIconv() ;
      virtual bool writeAsUTF(FILE *fp, const unsigned char *buffer,
			      size_t buflen, bool romanize = false,
			      OutputFormat fmt = OF_UTF8) ;
   } ;

//----------------------------------------------------------------------

class CharacterSetCache
   {
   private:
      static CharacterSetCache *s_cache ;
   protected:
      size_t   m_size ;
      size_t   m_alloc ;
      CharacterSet **m_charsets ;
   protected:
      static bool insert(CharacterSet **array, size_t &size, CharacterSet *set) ;
      static bool insertNew(CharacterSet **array, size_t &size, CharacterSet *set) ;
      CharacterSet *locateNormalized(const char *encoding) const ;
      CharacterSet *locate(const char *encoding) const ;
      bool expand() ;
   public:
      CharacterSetCache() ;
      ~CharacterSetCache() ;

      const CharacterSet *getCharSet(const char *encoding) ;
      CharacterSet **getCharSets(const char *encoding_list) ;
      void freeCharSets(CharacterSet **sets) ;

      // accessors
      size_t cacheSize() const { return m_size ; }
      size_t cacheAllocated() const { return m_alloc ; }

      // manipulating the single instance
      static CharacterSetCache *instance() ;
      static void deallocate() ;
   } ;

//----------------------------------------------------------------------

#endif /* !__CHARSET_H_INCLUDED */

/* end of file charset.h */
