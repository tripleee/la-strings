/************************************************************************/
/*                                                                      */
/*	LangIdent: long n-gram-based language identification		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     interleave.C  interleave lines of multiple text files	*/
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINE 262144

const char version[] = "interleave.c version 1.24" ;

/*=======================================================================*/

static void get_line(FILE *in, char *line, int maxline, int strip_sentnum,
		     int strip_tagonly)
{
   /* skip leading whitespace */
   char *start ;
   char *end ;
   if (!fgets(line,maxline,in))
      return ;
   line[maxline-1] = '\0' ;		/* ensure proper string termination */
   /* strip leading whitespace */
   start = line ;
   while (*start && (unsigned)*start < 0x80 && isspace(*start))
      start++ ;
   if (start != line)
      strcpy(line,start) ;
   end = strchr(line,'\0') ;
   while (end > line && (((unsigned char*)end)[-1] <= ' '))
      *--end = '\0' ;
   if (strip_tagonly && line[0] == '<' && strchr(line,'>')+1 == end)
      {
      /* blank the line if it consists of a single SGML/HTML/XML tag */
      *line = '\0' ;
      }
   else if (strip_sentnum && strncmp(line,"<s id=",6) == 0)
      {
      /* strip off the sentence ID if requested to do so */
      char *start = strchr(line+6,'>') ;
      if (start)
	 strcpy(line,start+1) ;
      }
   return ;
}

/*=======================================================================*/

static void write_line(FILE *out, const char *line, const char *blank_tag)
{
   if (line[0] == '\0' && blank_tag)
      fputs(blank_tag,out) ;
   else
      fputs(line,out) ;
   fputc('\n',out) ;
   return ;
}

/*=======================================================================*/

static int have_more_data(FILE **fp, int count)
{
   size_t i ;
   for (i = 1 ; i < count ; i++)
      {
      if (!feof(fp[i]))
	 return 1 ;
      }
   // none of the files have data remaining
   return 0 ;
}

/*=======================================================================*/

static int length_ratio_OK(const char *line1, const char *line2,
			   double ratio, int allow_blank_lines)
{
   if (!line1 || !line2)
      return 0 ;
   if (!allow_blank_lines)
      {
      if (!*line1 && !*line2)
	 return 1 ;
      else if (!*line1 || !*line2)
	 return 0 ;
      }
   int len1 ;
   int len2 ;
   int longer ;
   int shorter ;
   if (ratio < 1.0)
      return 1 ;
   len1 = strlen(line1) ;
   len2 = strlen(line2) ;
   if (len1 < allow_blank_lines)
      len1 = allow_blank_lines ;
   if (len2 < allow_blank_lines)
      len2 = allow_blank_lines ;
   if (len1 > len2)
      {
      longer = len1 ;
      shorter = len2 ;
      }
   else
      {
      longer = len2 ;
      shorter = len1 ;
      }
   return (longer <= ratio * shorter) ;
}

/*=======================================================================*/

static void usage(const char *argv0)
{
   fprintf(stderr,"Usage: %s [options] file1 file2 ... >interleaved\n",argv0) ;
   fprintf(stderr,"\tinterleave the contents of the named files line-by-line\n") ;
   fprintf(stderr,"Options:\n") ;
   fprintf(stderr,"  -bS\tinsert string 'S' in place of blank input lines\n") ;
   fprintf(stderr,"  -gN\tinterleave groups of N lines (default=1)\n") ;
   fprintf(stderr,"  -n\tdon't insert a blank line between sets of input lines\n") ;
   fprintf(stderr,"  -rX\tskip pairs where one line is less than X * length of other\n") ;
   fprintf(stderr,"\t\t(not available with multiple lines per group)\n") ;
   fprintf(stderr,"  -s\tstrip SGML sentence-IDs\n") ;
   fprintf(stderr,"  -S\tstrip pairs where either line consists of an SGML tag\n") ;
   fprintf(stderr,"  -u\tallow unequal input file lengths (default = stop on\n") ;
   fprintf(stderr,"\t\treaching end of shortest file)\n") ;
   fprintf(stderr,"  -v\trun verbosely\n") ;
   return ;
}

/*=======================================================================*/

int main(int argc, char *argv[])
{
   FILE **fp ;
   char **lines ;
   int block_size = 1 ;
   int strip_sentnum = 0 ;
   int strip_tagonly = 0 ;
   int insert_blank_line = 1 ;
   int done = 0 ;
   int verbose = 0 ;
   int must_be_same_length = 1 ;
   unsigned int skipped = 0 ;
   const char *argv0 = argv[0] ;
   const char *blank_tag = "" ;
   double ratio = 0.0 ;
   size_t i ;
   while (argc > 1 && argv[1][0] == '-')
      {
      switch (argv[1][1])
	 {
	 case 'b':
	    blank_tag = argv[1]+2 ;
	    break ;
	 case 'g':
	    block_size = atoi(argv[1]+2) ;
	    break ;
	 case 'n':
	    insert_blank_line = 0 ;
	    break ;
	 case 'r':
	    ratio = strtod(argv[1]+2,0) ;
	    break ;
	 case 'S':
	    strip_tagonly = 1 ;
	    break ;
	 case 's':
	    strip_sentnum = 1 ;
	    break ;
	 case 'u':
	    must_be_same_length = 0 ;
	    break ;
	 case 'v':
	    verbose = 1 ;
	    break ;
	 default:
	    usage(argv0) ;
	    return 1 ;
	 }
      argv++ ;
      argc-- ;
      }
   if (argc < 2)
      {
      usage(argv0) ;
      return 1 ;
      }
   fp = (FILE**)malloc(argc*sizeof(FILE*)) ;
   lines = (char**)malloc(argc*sizeof(char*)) ;
   if (!fp || !lines)
      {
      fprintf(stderr,"out of memory!\n") ;
      return 2 ;
      }
   for (i = 1 ; i < argc ; i++)
      {
      fp[i] = fopen(argv[i],"r") ;
      if (!fp[i])
	 {
	 fprintf(stderr,"unable to open file '%s' for read\n",argv[i]) ;
	 return 3 ;
	 }
      lines[i] = (char*)malloc(MAXLINE) ;
      if (!lines[i])
	 {
	 fprintf(stderr,"out of memory!\n") ;
	 return 2 ;
	 }
      }
   while (!done && have_more_data(fp,argc))
      {
      int have_more_data = 1 ;
      for (i = 1 ; i < argc ; i++)
	 {
	 int linecount ;
	 lines[i][0] = '\0' ;
	 if (feof(fp[i]))
	    {
	    if (must_be_same_length)
	       {
	       fprintf(stderr,"warning: file%u (%s) is too short\n",(unsigned)i,argv[i]) ;
	       done = 1 ;
	       break ;
	       }
	    else
	       continue ;
	    }
	 if (block_size > 1 || (!must_be_same_length && ratio == 0.0))
	    {
	    for (linecount = 0 ;
		 linecount < block_size && !feof(fp[i]) ;
		 linecount++)
	       {
	       get_line(fp[i],lines[i],MAXLINE,strip_sentnum,strip_tagonly) ;
	       if (!feof(fp[i]))
		   write_line(stdout,lines[i],blank_tag) ;
	       }
	    }
	 else
	    {
	    get_line(fp[i],lines[i],MAXLINE,strip_sentnum,strip_tagonly) ;
	    }
	 if (!feof(fp[i]))
	    have_more_data = 1 ;
	 }
      if (!have_more_data)
	 done = 1 ;
      if (done)
	 break ;
      if (block_size > 1 || !must_be_same_length)
	 {
	 if (insert_blank_line)
	    fputc('\n',stdout) ;
	 continue ;
	 }
      if (argc > 3 ||
	  length_ratio_OK(lines[1],lines[2],ratio,strlen(blank_tag)))
	 {
	 for (i = 1 ; i < argc ; i++)
	    write_line(stdout,lines[i],blank_tag) ;
	 if (insert_blank_line)
	    fputc('\n',stdout) ;
	 }
      else
	 {
	 if (verbose)
	    {
	    fprintf(stderr,"skipped '%s'\n",lines[1]) ;
	    fprintf(stderr,"     => '%s'\n",lines[2]) ;
	    }
	 skipped++ ;
	 }
      }
   if (skipped > 0)
      fprintf(stderr,"skipped %u pairs due to bad length ratio\n",skipped) ;
   // clean up
   for (i = 1 ; i < argc ; i++)
      {
      fclose(fp[i]) ;
      }
   free(fp) ;
   return 0 ;
}
