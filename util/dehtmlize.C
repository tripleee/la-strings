/************************************************************************/
/*                                                                      */
/*	LangIdent: long n-gram-based language identification		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     dehtmlize.C  convert &#NNN; to UTF-8			*/
/*  Version:  1.24							*/
/*  LastEdit: 06aug2014 						*/
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

#include <cstdlib>
#include <cstdio>

enum State
   {
      ST_Normal,
      ST_Ampersand,
      ST_Numeric

   } ;

//----------------------------------------------------------------------

static void write_utf8(unsigned long codepoint, FILE *outfp)
{
   if (codepoint < 0x80)
      fputc((char)codepoint,outfp) ;
   else if (codepoint < 0x800)
      {
      fputc((char)(0xC0 | ((codepoint >> 6) & 0x1F)), outfp) ;
      fputc((char)(0x80 | (codepoint & 0x3F)), outfp) ;
      }
   else if (codepoint < 0x10000)
      {
      fputc((char)(0xE0 | ((codepoint >> 12) & 0x0F)), outfp) ;
      fputc((char)(0x80 | ((codepoint >> 6) & 0x3F)), outfp) ;
      fputc((char)(0x80 | (codepoint & 0x3F)), outfp) ;
      }
   else if (codepoint < 0x10FFFF)
      {
      fputc((char)(0xF0 | ((codepoint >> 18) & 0x07)), outfp) ;
      fputc((char)(0x80 | ((codepoint >> 12) & 0x3F)), outfp) ;
      fputc((char)(0x80 | ((codepoint >> 6) & 0x3F)), outfp) ;
      fputc((char)(0x80 | (codepoint & 0x3F)), outfp) ;
      }
   else
      fputc('?',stdout) ;
   return ;
}

//----------------------------------------------------------------------

static void usage(const char *argv0)
{
   fprintf(stderr,"Usage: %s <in >out\n",argv0) ;
   fprintf(stderr,"  convert &#nnn; entities to actual UTF-8 characters\n") ;
   return ;
}

//----------------------------------------------------------------------

int main(int argc, char **argv)
{
   if (argc > 1)
      {
      usage(argv[0]) ;
      return 1;
      }
   State state = ST_Normal ;
   unsigned long codepoint = 0 ;
   while (!feof(stdin))
      {
      int ch = fgetc(stdin) ;
      if (ch == EOF)
	 break ;
      switch (state)
	 {
	 case ST_Normal:
	    if (ch == '&')
	       state = ST_Ampersand ;
	    else
	       fputc(ch,stdout) ;
	    break ;
	 case ST_Ampersand:
	    if (ch == '#')
	       {
	       state = ST_Numeric ;
	       codepoint = 0 ;
	       }
	    else
	       {
	       fputc('&',stdout) ;
	       fputc(ch,stdout) ;
	       state = ST_Normal ;
	       }
	    break ;
	 case ST_Numeric:
	    if (ch >= '0' && ch <= '9')
	       codepoint = 10 * codepoint + (ch - '0') ;
	    else
	       {
	       write_utf8(codepoint,stdout) ;
	       if (ch != ';')
		  fputc(ch,stdout) ;
	       state = ST_Normal ;
	       }
	    break ;
	 default:
	    return 1 ;
	 }
      }
   return 0 ;
}
