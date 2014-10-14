/************************************************************************/
/*                                                                      */
/*	LangIdent: long n-gram-based language identification		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     icuconv.C  character-set conversion using libicu		*/
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

// g++ -o icuconv -Wall icuconv.C -licuuc

#include <cstdio>
#include <unicode/ucnv.h>

#define BUFFERSIZE 65536

UConverter *converter ;

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

// based on example at http://userguide.icu-project.org/conversion/converters
static void convert_to(FILE *infp, FILE *outfp)
{
   UErrorCode err = U_ZERO_ERROR ;
   while (!feof(infp))
      {
      char inbuf[BUFFERSIZE] ;
      // get next chunk of input file
      unsigned buflen = fread(inbuf,1,sizeof(inbuf),infp) ;
      UBool flush = !feof(infp) ;
      const char *sourceptr = inbuf ;
      const char *sourcelimit = inbuf + buflen ;
      do {
         UChar outbuf[2*BUFFERSIZE] ;
	 UChar *outptr = outbuf ;
	 const UChar *outlimit = outbuf + BUFFERSIZE ;

	 err = U_ZERO_ERROR ;
	 ucnv_toUnicode(converter, &outptr, outlimit,
			&sourceptr, sourcelimit, NULL, flush,
			&err) ;
	 write_as_UTF8(outfp, outbuf, outptr - outbuf) ;
         } while (err == U_BUFFER_OVERFLOW_ERROR) ;
      if (U_FAILURE(err))
	 {
	 //
	 break ;
	 }
      }
   if (U_FAILURE(err))
      {
      //
      }
   return ;
}

//----------------------------------------------------------------------

static void convert_from(FILE *infp, FILE *outfp)
{
   UErrorCode err = U_ZERO_ERROR ;
   while (!feof(infp))
      {
      UChar inbuf[BUFFERSIZE] ;
      // get next chunk of input file
      unsigned buflen = fread(inbuf,sizeof(UChar),BUFFERSIZE,infp) ;
      UBool flush = !feof(infp) ;
      const UChar *sourceptr = inbuf ;
      const UChar *sourcelimit = inbuf + buflen ;
      do {
         char outbuf[4*BUFFERSIZE] ;
	 char *outptr = outbuf ;
	 char *outlimit = outbuf + sizeof(outbuf) ;

	 err = U_ZERO_ERROR ;
	 ucnv_fromUnicode(converter, &outptr, outlimit,
			  &sourceptr, sourcelimit, NULL, flush,
			  &err) ;
	 fwrite(outbuf, 1, outptr - outbuf, outfp) ;
         } while (err == U_BUFFER_OVERFLOW_ERROR) ;
      if (U_FAILURE(err))
	 {
	 //
	 break ;
	 }
      }
   if (U_FAILURE(err))
      {
      //
      }
   return ;
}

//----------------------------------------------------------------------

static void convert(FILE *infp, FILE *outfp, bool from)
{
   if (from)
      convert_from(infp, outfp) ;
   else
      convert_to(infp, outfp) ;
   return ;
}

//----------------------------------------------------------------------

int main(int argc, char **argv)
{
   if (argc < 3)
      {
      fprintf(stderr,"Usage: %s {f|t} encoding [file ...]\n",argv[0]) ;
      fprintf(stderr,"  convert text between 'encoding' and UTF-8\n") ;
      fprintf(stderr,"  first arg specifies whether to convert (f)rom or (t)o 'encoding'\n") ;
      return 1 ;
      }
   bool convert_from_Unicode = (argv[1][0] == 't') ;
   const char *encoding = argv[2] ;
   argv += 2 ;
   argc -= 2 ;
   UErrorCode err = U_ZERO_ERROR ;
   if (encoding[0] == '-' && encoding[1] == '\0')
      {
      int32_t count = ucnv_countAvailable() ;
      fprintf(stderr,"%d converters available:\n", count) ;
      for (int32_t i = 0 ; i < count ; i++)
	 {
	 fprintf(stderr," %s", ucnv_getAvailableName(i)) ;
	 if (i % 5 == 4)
	    fprintf(stderr,"\n") ;
	 }
      return 0 ;
      }
   converter = ucnv_open(encoding, &err) ;
   if (!converter || U_FAILURE(err))
      {
      fprintf(stderr,"Unable to open ICU converter for %s\n",encoding) ;
      return 2 ;
      }
   if (argc > 1)
      {
      // convert each named file to standard output
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
	       convert(fp,stdout,convert_from_Unicode) ;
	       fclose(fp) ;
	       }
	    else
	       fprintf(stderr,"Unable to open file %s\n",filename) ;
	    }
	 }
      }
   else
      {
      // convert standard input to standard output
      convert(stdin,stdout,convert_from_Unicode) ;
      }
   ucnv_close(converter) ;
   return 0 ;
}

// end of file icuconv.C //
