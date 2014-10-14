/************************************************************************/
/*                                                                      */
/*	LangIdent: long n-gram-based language identification		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     icuconv.C  transliteration/romanization using libicu	*/
/*  Version:  1.24							*/
/*  LastEdit: 04aug2014 						*/
/*                                                                      */
/*  (c) Copyright 2012,2014 Ralf Brown/Carnegie Mellon University	*/
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

// g++ -Wall -o icutrans icutrans.C -licuuc -licutu

#include <cstdio>
#include <cstring>
#include <iostream>
#include <unicode/translit.h>

using namespace std ;

#define MAX_LINE 65536

//----------------------------------------------------------------------

static void usage(const char *argv0)
{
   cerr << "Usage: " << argv0 << "xlit [file ...]" << endl ;
   cerr << "   apply transliteration 'xlit' to input; write to stdout" << endl ;
   cerr << "   (transliteration '=' lists available transliterations)" << endl ;
   return ;
}

//----------------------------------------------------------------------

static void write_as_UTF8(FILE *outfp, const UChar *buf, unsigned buflen)
{
   while (buflen > 0)
      {
      UChar codepoint = *buf++ ;
      buflen-- ;
      if (codepoint < 0x80)
	 {
	 // encoded as single byte
	 fputc(codepoint,outfp) ;
	 }
      else if (codepoint < 0x800)
	 {
	 // encoded in two bytes
	 fputc(0xC0 | ((codepoint & 0x07C0) >> 6),outfp) ;
	 fputc(0x80 | (codepoint & 0x003F), outfp) ;
	 }
      else if (codepoint < 0xD800 || codepoint >= 0xE000)
	 {
	 // encoded in three bytes
	 fputc(0xE0 | ((codepoint & 0xF000) >> 12),outfp) ;
	 fputc(0x80 | ((codepoint & 0x0FC0) >> 6),outfp) ;
	 fputc(0x80 | (codepoint & 0x003F), outfp) ;
	 }
      else
	 {
	 // surrogate, would need a second code point, so just ignore
	 }
      }
   return ;
}

//----------------------------------------------------------------------

static void transliterate(const char *transliteration, FILE *infp, FILE *outfp)
{
   UnicodeString ID(transliteration,"utf-8") ;
   UErrorCode err = U_ZERO_ERROR ;
   Transliterator *xlit = Transliterator::createInstance(ID,UTRANS_FORWARD,
							 err) ;
   if (!xlit || U_FAILURE(err))
      {
      cerr << "Unable to instantiate transliterator " << transliteration
	   << endl ;
      return ;
      }
   while (!feof(infp))
      {
      char buf[MAX_LINE] ;
      if (!fgets(buf,sizeof(buf),infp))
	 break ;
      buf[sizeof(buf)-1] = '\0' ;
      size_t len = strlen(buf) ;
      if (len > 0 && buf[len-1] == '\n')
	 buf[len-1] = '\0' ;
      UnicodeString s(buf,"utf-8") ;
      if (!s.isBogus())
	 {
	 xlit->transliterate(s) ;
	 for (int32_t i = 0 ; i < s.length() ; i++)
	    {
	    UChar c = s[i] ;
	    write_as_UTF8(outfp,&c,1) ;
	    }
	 fputc('\n',outfp) ;
	 }
      }
   delete xlit ;
   return ;
}

//----------------------------------------------------------------------

int main(int argc, char **argv)
{
   if (argc < 2)
      {
      usage(argv[0]) ;
      return 1 ;
      }
   const char *transliteration = argv[1] ;
   argv++ ;
   argc-- ;
   if (*transliteration == '=')
      {
      UErrorCode err = U_ZERO_ERROR ;
      StringEnumeration *IDs = Transliterator::getAvailableIDs(err) ;
      if (IDs)
	 {
	 int32_t count = IDs->count(err) ;
	 for (int32_t i = 0 ; i < count ; i++)
	    {
	    int32_t len = 0 ;
	    cerr << "\t" << IDs->next(&len,err) << endl ;
	    }
	 }
      delete IDs ;
      return 0 ;
      }

   if (argc > 1)
      {
      // transliterate each named file to standard output
      while (argc > 1)
	 {
	 const char *filename = argv[1] ;
	 argv++ ;
	 argc-- ;
	 if (!filename)
	    break ;
	 if (*filename)
	    {
	    FILE *fp = fopen(filename,"rb") ;
	    if (fp)
	       {
	       transliterate(transliteration,fp,stdout) ;
	       fclose(fp) ;
	       }
	    else
	       fprintf(stderr,"Unable to open file %s\n",filename) ;
	    }
	 }
      }
   else
      {
      transliterate(transliteration,stdin,stdout) ;
      }
   return 0 ;
}

// end of file icutrans.C //
