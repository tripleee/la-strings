/************************************************************************/
/*                                                                      */
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     la-strings.C						*/
/*  Version:  1.24							*/
/*  LastEdit: 26aug2013							*/
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

#include <cstdlib>
#include <errno.h>
#include <iostream>
#include <locale.h>
#include <string.h>
#include "charset.h"
#include "extract.h"
#include "langident/langid.h"
#include "FramepaC.h"
#include "la-strings.h"

using namespace std ;

/************************************************************************/
/*	Manifest Constants						*/
/************************************************************************/

#define DEFAULT_MAX_LANGS 2

/************************************************************************/
/*	Types for this module						*/
/************************************************************************/

/************************************************************************/
/*	Global variables for this module				*/
/************************************************************************/

static double bigram_weight = DEFAULT_BIGRAM_WEIGHT ;

/************************************************************************/
/*	Helper functions						*/
/************************************************************************/

static const char *get_arg(int &argc, const char **&argv)
{
   if (argv[1][2])
      return argv[1]+2 ;
   else
      {
      argc-- ;
      argv++ ;
      return argv[1] ;
      }
}

//----------------------------------------------------------------------

static char *dupstr(const char *s)
{
   if (!s)
      return 0 ;
   unsigned len = strlen(s) ;
   char *dup = (char*)malloc(len+1) ;
   if (dup)
      memcpy(dup,s,len) ;
   return dup ;
}

//----------------------------------------------------------------------

static void parse_langident(const char *arg, bool &identify_language,
			    const char *&langident_file,
			    bool &use_friendly_name,
			    bool &smooth_language_score)
{
   arg += 2 ; // skip -i
   if (*arg == '@')
      {
      smooth_language_score = false ;
      arg++ ;
      }
   if (*arg == '+')
      {
      use_friendly_name = true ;
      arg++ ;
      }
   identify_language = true ;
   langident_file = arg ;
   return ;
}

//----------------------------------------------------------------------

static void parse_restriction(const char *restriction,
			      ExtractParameters &params,
			      bool verbose)
{
   if (restriction && *restriction)
      {
      uint64_t start_offset = 0 ;
      uint64_t end_offset = (uint64_t)~0 ; 
      char *end ;
      uint64_t value = strtoul(restriction,&end,0) ;
      if (end != restriction)
	 {
	 start_offset = value ;
	 restriction = end ;
	 }
      if (*restriction == ',')
	 {
	 restriction++ ;
	 value = strtoul(restriction,&end,0) ;
	 if (end != restriction && value > start_offset)
	    end_offset = value ;
	 }
      if (verbose)
	 cerr << "**** Restricting scanning to bytes " << start_offset
	      << " through " << end_offset << endl ;
      params.setRange(start_offset,end_offset) ;
      }
   return ;
}

//----------------------------------------------------------------------

static void parse_UTF_output(const char *arg, OutputFormat &output_format,
			     bool &romanize_output)
{
   output_format = OF_UTF8 ;
   while (*arg)
      {
      switch (*arg)
	 {
	 case '8':	output_format = OF_UTF8 ;	break ;
	 case 'r':	romanize_output = true ;	break ;
#ifdef NO_ICONV
	 case 'b':
	 case 'l':
	    cerr << "UTF-16 output not available (iconv not linked)" << endl ;	break ;
#else
	 case 'b':	output_format = OF_UTF16BE ;	break ;
	 case 'l':	output_format = OF_UTF16LE ;	break ;
#endif /* NO_ICONV */
	 default:
	    cerr << "Unknown option '" << *arg << "' to -u switch" << endl ;
	 }
      arg++ ;
      }
   return ;
}

//----------------------------------------------------------------------

static void parse_fuzzy(const char *fuzz_spec, ExtractParameters &params)
{
   // format of fuzzy-match spec:
   //    [@]maxgap,mindesired,minalpha
   //  @ means include newlines and carriage returns as valid text chars
   //  maxgap is maximum number of chars outside desired range to skip
   //  mindesired is minimum proportion (0.0-1.0) of chars from given language
   //  minalpha is minimum proportion (0.0-1.0) of alpha/numeric chars
   if (fuzz_spec && *fuzz_spec)
      {
      while (*fuzz_spec && isspace(*fuzz_spec))
	 fuzz_spec++ ;
      if (*fuzz_spec == '@')
	 {
	 params.allowNewlines() ;
	 fuzz_spec++ ;
	 }
      char *end ;
      unsigned long gap = strtol(fuzz_spec,&end,0) ;
      if (end != fuzz_spec)
	 {
	 fuzz_spec = end ;
	 params.setGap(gap) ;
	 }
      if (*fuzz_spec == ',')
	 {
	 fuzz_spec++ ;
	 double minimum = strtod(fuzz_spec,&end) ;
	 if (end != fuzz_spec)
	    {
	    fuzz_spec = end ;
	    params.setDesired(minimum) ;
	    }
	 if (*fuzz_spec == ',')
	    {
	    fuzz_spec++ ;
	    minimum = strtod(fuzz_spec,&end) ;
	    if (end != fuzz_spec)
	       {
	       fuzz_spec = end ;
	       params.setAlpha(minimum) ;
	       }
	    }
	 }
      }
   return ;
}

//----------------------------------------------------------------------

static void set_weight(char type, double weight)
{
   switch (type)
      {
      case 'b':
	 bigram_weight = (weight >= 0.0 ? weight : DEFAULT_BIGRAM_WEIGHT) ;
	 break ;
      case 's':
	 set_stopgram_penalty(weight) ;
	 break ;
      default:
	 cerr << "Unknown weight type '" << type << "' in -W argument"
	      << endl ;
	 break ;
      }
   return ;
}

//----------------------------------------------------------------------

static void parse_weights(const char *wtspec)
{
   if (!wtspec)
      return ;
   while (*wtspec)
      {
      while (*wtspec == ',')
	 wtspec++ ;		     // allow empty specs to simplify scripts
      char type = *wtspec++ ;
      char *spec_end ;
      double value = strtod(wtspec,&spec_end) ;
      if (spec_end && spec_end > wtspec)
	 {
	 set_weight(type,value) ;
	 wtspec = spec_end ;
	 if (*wtspec == ',')
	    wtspec++ ;
	 else
	    break ;
	 }
      else
	 break ;
      }
   return ;
}

//----------------------------------------------------------------------

static void usage(const char *argv0, const char *bad_arg)
{
   if (bad_arg)
      cerr << "Unrecognized argument " << bad_arg << endl << endl ;
   cerr << "LangAware-Strings v" LASTRINGS_VERSION "  Copyright 2010-2013 Ralf Brown/CMU -- GNU GPLv3" << endl ;
   cerr << "Usage: " << argv0 << " [options] file ..." << endl ;
   cerr << "Extract text strings from the given files and print them to standard output.\n"
      "Search options:\n"
      "  -eSET   search for strings in character encoding SET (default ASCII)\n"
      "  -e=DB   automatically identify character encodings using database DB\n"
      "          (overrides default use of -i database when used with -i)\n"
      "  -nN     require strings to contain at least N characters\n"
      "  -i[F]   identify language of string; if F specified, use it instead of\n"
      "          the default language identification database\n"
      "  -i+[F]  identify languages, using friendly name if available\n"
      "  -i@     identify languages without smoothing language scores\n"
      "  -lLANG  assume text is in language LANG (default no restriction on letters)\n"
      "  -Lfile  set desired language characters from 'file'\n"
      "  -L=..   set desired language characters to ranges in comma-separated list\n"
      "  -wfile  load list of desired words from 'file'\n"
      "  -WSPEC  set internal scoring weights according to SPEC:\n"
      "          b0.1,s1.5  would set bigram weights to 0.1 and stopgram weights\n"
      "                     to 1.5\n"
      "  -Fg,d,a filtering: max gap, min desired%, min alphanumeric%\n"
      "  -rS,E   restrict scan to bytes S through E of the file\n"
      "Output options:\n"
      "  -C      print counts of strings extracted, by language\n"
      "  -E      print detected encoding before each string\n"
      "  -f      print filename before each string\n"
      "  -I#     print up to # language guesses (with -i)\n"
      "  -M      force Microsoft-style CRLF newlines\n"
      "  -o      print file offset of string in octal\n"
      "  -O[dir] output strings to file '[dir/]{infile}.strings'\n"
      "  -s      show confidence score for each string\n"
      "  -S[X]   don't show strings with confidence < X (default 10.0)\n"
      "  -tX     print file offset in radix X (o=8,d=10,x=16)\n"
      "  -u[bl]  convert extracted text to UTF8/[UTF16BE/UTF16LE] if possible\n"
      "  -ur     output romanized version of extracted text if possible\n"
      "  -v      run verbosely\n"
      "Compatibility options: -a and -T are ignored\n"
      "Use -eLIST to display the known character encodings (for compatibility with\n"
      "GNU strings, 's', 'S', 'b', 'B', 'l', 'L', 'e', and 'u' are also accepted).\n"
	<< endl ;
   exit(1) ;
}

/************************************************************************/
/*	Main Program							*/
/************************************************************************/

static unsigned count_names(const char *namelist)
{
   if (!namelist || !*namelist)
      return 0 ;
   unsigned count = 1 ;
   while (namelist)
      {
      const char *comma = strchr(namelist,',') ;
      if (comma)
	 {
	 count++ ;
	 namelist = comma + 1 ;
	 }
      else
	 break ;
      }
   return count ;
}

//----------------------------------------------------------------------

static double parse_min_score(const char *arg)
{
   double min_score = 10.0 ;
   if (arg && *arg)
      {
      double score = strtod(arg,0) ;
      if (score >= 0.0)
	 min_score = score ;
      }
   return min_score ;
}

//----------------------------------------------------------------------

int main(int argc, const char **argv)
{
   const char *argv0 = argv[0] ;
   const char *encoding = 0 ;
   const char *language = "" ;
   const char *restriction = "" ;
   const char *fuzzy = "" ;
   const char *language_file = "" ;
   const char *lang_ident_file = "" ;
   const char *charset_ident_file = 0 ;
   const char *outdir = 0 ;
   char *wordlist_file = 0 ;
   int min_length = MIN_STRING_LENGTH ;
   int max_langs = DEFAULT_MAX_LANGS ;
   OutputFormat output_format = OF_Native ;
   bool verbose = false ;
   bool show_conf = false ;
   bool show_enc = false ;
   bool print_filename = false ;
   bool count_by_language = false ;
   bool want_help = false ;
   bool identify_language = false ;
   bool smooth_language_scores = true ;
   bool use_friendly_name = false ;
   bool end_of_args = false ;
   bool romanize_output = false ;
   bool force_CRLF = false ;
   bool show_script = false ;
   char print_location = ' ' ;
   double min_score = -1.0 ;
   while (argc > 1 && argv[1][0] == '-')
      {
      if (argv[1][1] == '-')
	 {
	 // no more flag arguments
	 end_of_args = true ;
	 argv++ ;
	 argc-- ;
	 break ;
	 }
      switch (argv[1][1])
	 {
	 case '0': case '1': case '2': case '3': case '4':
	 case '5': case '6': case '7': case '8': case '9':
	    min_length = atoi(argv[1]+1) ;			break ;
	 case 'a': /* GNU compatibility */			break ;
	 case 'A': show_script = true ;				break ;
	 case 'C': count_by_language = true ;			break ;
	 case 'e': encoding = get_arg(argc,argv) ;		break ;
	 case 'E': show_enc = true ;				break ;
	 case 'f': print_filename = true ;			break ;
	 case 'F': fuzzy = get_arg(argc,argv) ;			break ;
	 case 'h': want_help = true ;				break ;
	 case 'i': parse_langident(argv[1],identify_language,
				   lang_ident_file,
				   use_friendly_name,
				   smooth_language_scores) ;	break ;
	 case 'I': max_langs = atoi(get_arg(argc,argv)) ;	break ;
	 case 'l': language = get_arg(argc,argv) ;		break ;
	 case 'L': language_file = get_arg(argc,argv) ;		break ;
	 case 'M': force_CRLF = true ;				break ;
	 case 'n': min_length = atoi(get_arg(argc,argv)) ; 	break ;
	 case 'o': print_location = 'o' ;			break ;
	 case 'O': outdir = argv[1]+2 ;		 		break ;
	 case 'r': restriction = get_arg(argc,argv) ;		break ;
	 case 's': show_conf = true ;				break ;
	 case 'S': min_score = parse_min_score(argv[1]+2) ;	break ;
	 case 't': print_location = *get_arg(argc,argv) ;	break ;
	 case 'T': (void)get_arg(argc,argv) ;			break ;
	 case 'u': parse_UTF_output(argv[1]+2,output_format,
				    romanize_output) ;		break ;
	 case 'v': verbose = true ;				break ;
	 case 'w': wordlist_file = dupstr(get_arg(argc,argv)) ;	break ;
	 case 'W': parse_weights(get_arg(argc,argv)) ;		break ;
	 default: usage(argv0,argv[1]) ;			break ;
	 }
      argc-- ;
      argv++ ;
      }
   if ((argc < 2 && !end_of_args) || want_help)
      usage(argv0,0) ;
   ExtractParameters filters ;
   if (min_length > 1)
      filters.setLength(min_length) ;
   if (max_langs < 0)
      max_langs = 0 ;
   parse_restriction(restriction,filters,verbose) ;
   parse_fuzzy(fuzzy,filters) ;
   if (identify_language && language[0] == '\0')
      {
      // if we've been asked to identify the langauge of each string, but
      //   no search language has been specified, request a pre-search
      //   language classification
      language = "auto" ;
      if (!encoding || encoding[0] == '=')
	 {
	 if (encoding && encoding[0] == '=')
	    {
	    charset_ident_file = encoding + 1 ;
	    }
	 encoding = "auto" ;
	 // reduce false positives by filtering out the very lowest-scoring
	 //   strings
	 if (min_score < 0.0)
	    min_score = 7.0 ;
	 }
      }
   if (!encoding)
      encoding = "ASCII" ;
   else if (encoding[0] == '=')
      {
      cerr << "Automatic character-set detection requires language identification (-i)"
	   << endl ;
      encoding = "ASCII" ;
      }
   // set an appropriate locale that permits iswprint() to return the
   //   right thing for Unicode characters
   if (!setlocale(LC_CTYPE, "UTF-8") && !setlocale(LC_CTYPE, "UTF8") &&
       !setlocale(LC_CTYPE, "en_US.UTF-8") && !setlocale(LC_CTYPE, "en_US.utf8") &&
       !setlocale(LC_CTYPE, "en_GB.utf8"))
      {
      // none of our tries is supported, so get default locale
      //   information for character typing from the environment
      //   variables LC_ALL or LC_CTYPE
      setlocale(LC_CTYPE, "") ;
      }
   CharacterSet **charsets = CharacterSet::makeCharSets(encoding) ;
   unsigned num_charsets = 0 ;
   if (charsets)
      {
      for (unsigned i = 0 ; charsets[i] ; i++)
	 num_charsets++ ;
      }
   if (num_charsets > 0 || identify_language)
      {
      filters.setMaxLanguages(max_langs) ;
      filters.wantConfidence(show_conf) ;
      filters.wantFilename(print_filename) ;
      filters.wantEncoding(show_enc) ;
      filters.wantScript(show_script) ;
      filters.wantSeparateOutputs(outdir != 0) ;
      filters.setOutputDirectory(outdir) ;
      filters.wantLocation(print_location) ;
      filters.wantCRLF(force_CRLF) ;
      if (min_score < 0.0) min_score = 0.0 ;
      filters.setMinimumScore(min_score) ;
      filters.identifyLanguage(identify_language && 
			       (max_langs > 0 || count_by_language)) ;
      filters.wantSmoothedScores(smooth_language_scores) ;
      filters.countLanguages(count_by_language) ;
      filters.wantFriendlyName(use_friendly_name) ;
      filters.outputFormat(output_format) ;
      filters.romanizeOutput(romanize_output) ;
      LanguageIdentifier *language_identifier = 0 ;
      if (identify_language)
	 {
	 language_identifier
	    = load_language_database(lang_ident_file, charset_ident_file,
				     false, verbose) ;
	 if (!language_identifier || language_identifier->numLanguages() == 0)
	    {
	    cerr << "Unable to open language identification database " 
		 << lang_ident_file << endl ;
	    unload_language_database(language_identifier) ;
	    language_identifier = 0 ;
	    identify_language = 0 ;
	    filters.identifyLanguage(false) ;
	    }
	 else if (!language_identifier->good())
	    {
	    cerr << "Error opening language identification database "
		 << lang_ident_file << endl ;
	    if (errno == EINVAL)
	       cerr << " (invalid signature or unsupported version)" << endl ;
	    unload_language_database(language_identifier) ;
	    language_identifier = 0 ;
	    identify_language = 0 ;
	    filters.identifyLanguage(false) ;
	    }
	 else
	    {
	    language_identifier->useFriendlyName(use_friendly_name) ;
	    language_identifier->setBigramWeight(bigram_weight) ;
	    filters.setLanguageIdentifier(language_identifier) ;
	    filters.setCharSets() ;
	    }
	 }
      unsigned list_count = count_names(wordlist_file) ;
      if (list_count != 0 && list_count != num_charsets)
	 {
	 cerr << "You must specify as many word lists as encodings."
	      << endl ;
	 wordlist_file = 0 ;
	 }
      for (unsigned i = 0 ; i < num_charsets ; i++)
	 {
	 char *next = wordlist_file ? strchr(wordlist_file,',') : 0 ;
	 if (next)
	    *next = '\0' ;
	 charsets[i]->loadWordList(wordlist_file,verbose) ;
	 wordlist_file = next ;
	 if (filters.newlinesAllowed())
	    charsets[i]->allowNewlines() ;
	 if (language_file && *language_file)
	    {
	    if (*language_file == '=')
	       charsets[i]->setLanguageChars(language_file+1) ;
	    else
	       charsets[i]->setLanguageFromFile(language_file) ;
	    }
	 else if (language && strcasecmp(language,"auto") != 0)
	    (void)charsets[i]->setLanguage(language) ;
	 }
      extract_text(charsets,&filters,verbose,argc,argv) ;
      for (unsigned i = 0 ; i < num_charsets ; i++)
	 {
	 delete charsets[i] ;
	 }
      if (filters.countLanguages() && language_identifier)
	 language_identifier->writeStatistics(stdout) ;
      unload_language_database(language_identifier) ;
      }
   else
      {
      cerr << "Error: unknown character set.  No text extracted." << endl ;
      }
   FrFree(charsets) ;
   free_language_scores() ;
   CharacterSetCache::deallocate() ;
   //FrMemoryStats() ;
   return 0 ;
}

// end of la-strings.C //
