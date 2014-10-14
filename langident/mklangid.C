/****************************** -*- C++ -*- *****************************/
/*                                                                      */
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     mklangid.C						*/
/*  Version:  1.24							*/
/*  LastEdit: 11aug2014							*/
/*                                                                      */
/*  (c) Copyright 2010,2011,2012,2013,2014				*/
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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <iomanip>
#include "langid.h"
#include "trie.h"
#include "mtrie.h"
#include "ptrie.h"
#include "FramepaC.h"
#ifndef NO_ICONV
# include <iconv.h>
#endif

using namespace std ;

/************************************************************************/
/*	Configuration and Manifest Constants				*/
/************************************************************************/

#define VERSION "1.24"

#define MAX_NGRAMS 5000

//#define DEBUG		// generate torrents of output in verbose mode

// since we always count at least trigrams, we don't support lengths less
//   than three
#define ABSOLUTE_MIN_LENGTH 3
#define DEFAULT_MAX_LENGTH  8
#define ABSOLUTE_MAX_LENGTH 500

#define MAX_OVERSAMPLE 2.5
#define MAX_INCREMENT 10

// if n-gram plus an affix appears at least this many times the count
//   of times the bare n-gram appears, ignore the bare n-gram
#define AFFIX_RATIO 0.90
#define MINLEN_AFFIX_RATIO 0.995        // be strict for minimum-length ngrams
// limit how low the user can set the affix ratio
#define MIN_AFFIX_RATIO 0.4

// assume that each character in a listed n-gram implies N characters
//   in the total training data when TotalCount: is not present; this discount
//   factor reduces false positive detections from mguesser ngram lists
#define ASSUMED_NGRAM_DENSITY 8

// how much to discount faked ngrams inserted via the UTF8: directive in
//   a frequency list file
#define FAKED_NGRAM_DISCOUNT 10

#define DEFAULT_SIMILARITY_THRESHOLD 0.50

#define BUFFER_SIZE 65536U

// factor times minimum representable prob at which to cut off stopgrams
#define STOPGRAM_CUTOFF 2

// weighting of stopgrams based on amount of training data
#define TRAINSIZE_NO_WEIGHT 15000
#define TRAINSIZE_FULL_WEIGHT 2000000

// the amount by which to increase the weight of an n-gram which is unique
//   to a particular model among closely-related languages
#define UNIQUE_BOOST 1.0

// precompute smoothing before scaling to reduce quantization error when
//   packing the trie
#define SMOOTHING_POWER 0.14

/************************************************************************/
/************************************************************************/

#ifndef lengthof
#  define lengthof(x) (sizeof(x)/sizeof((x)[0]))
#endif /* lengthof */

#if BUFFER_SIZE < 2*FrMAX_LINE
#  undef BUFFER_SIZE
#  define BUFRER_SIZE (2*FrMAX_LINE)
#endif

/************************************************************************/
/*	Types for this module						*/
/************************************************************************/

struct NgramEnumerationData
   {
   public:
      NybbleTrie *m_oldngrams ;
      NybbleTrie *m_ngrams ;
      uint32_t   *m_frequencies ;
      bool       &m_have_max_length ;
      bool	  m_inserted_ngram ;
      unsigned    m_min_length ;
      unsigned    m_max_length ;
      unsigned	  m_desired_length ;
      unsigned    m_topK ;
      unsigned    m_count ;
      unsigned	  m_alignment ;
      uint32_t    m_min_freq ;
   public:
      NgramEnumerationData(bool &have_max_length)
	 : m_have_max_length(have_max_length), m_inserted_ngram(false), m_alignment(1)
	 {}
      ~NgramEnumerationData() {}

   } ;

//----------------------------------------------------------------------

class StopGramInfo
   {
   private:
      NybbleTrie 	   *m_trie ;
      NybbleTrie	   *m_currngrams ;
      NybbleTrie	   *m_ngramweights ;
      const PackedTrieFreq *m_freqbase ;
      const LanguageScores *m_weights ;
      const FrBitVector    *m_selected ;
      unsigned		    m_activelang ;
   public:
      StopGramInfo(NybbleTrie *t, NybbleTrie *t2, NybbleTrie *t3,
		   const PackedTrieFreq *base,
		   const LanguageScores *wt, const FrBitVector *sel,
		   unsigned lang)
	 { m_trie = t ; m_currngrams = t2 ; m_ngramweights = t3 ;
	   m_freqbase = base ; m_weights = wt ; m_selected = sel ;
	   m_activelang = lang ; }
      ~StopGramInfo() {}

      // accessors
      NybbleTrie *trie() const { return m_trie ; }
      NybbleTrie *currLangTrie() const { return m_currngrams ; }
      NybbleTrie *weightTrie() const { return m_ngramweights ; }
      unsigned activeLanguage() const { return m_activelang ; }
      bool selected(size_t langid) const
	 { return m_selected ? m_selected->getBit(langid) : false ; }
      const PackedTrieFreq *freqBaseAddress() const { return m_freqbase ; }
      double weight(size_t langid) const
	 { return m_weights ? m_weights->score(langid) : 1.0 ; }
   } ;

//----------------------------------------------------------------------

class StopGramWeight
   {
   private:
      const NybbleTrie *m_weighttrie ;
      uint64_t          m_totalbytes ;
      bool	        m_scaled ;
   public:
      StopGramWeight(const NybbleTrie *wt, uint64_t tb, bool sc)
	 { m_weighttrie = wt ; m_totalbytes = tb ; m_scaled = sc ; }
      ~StopGramWeight() {}

      // accessors
      bool scaled() const { return m_scaled ; }
      uint64_t totalBytes() const { return m_totalbytes ; }
      const NybbleTrie *weights() const { return m_weighttrie ; }
      uint32_t weight(const uint8_t *key, unsigned keylen) const ;

      // modifiers

   } ;

//----------------------------------------------------------------------

enum BigramExtension
   {
      BigramExt_None,
      BigramExt_ASCIILittleEndian,
      BigramExt_ASCIIBigEndian,
      BigramExt_UTF8LittleEndian,
      BigramExt_UTF8BigEndian
   } ;

/************************************************************************/
/*	Global variables						*/
/************************************************************************/

static bool verbose = false ;
static bool store_similarities = false ;
static bool do_dump_trie = false ;
static bool crubadan_format = false ;
static BigramExtension bigram_extension = BigramExt_None ;
static bool convert_Latin1 = false ;
static unsigned topK = MAX_NGRAMS ;
static unsigned minimum_length = ABSOLUTE_MIN_LENGTH ;
static unsigned maximum_length = DEFAULT_MAX_LENGTH ;
static unsigned alignment = 1 ;
static const char *vocabulary_file = 0 ;
static double max_oversample = MAX_OVERSAMPLE ;
static double affix_ratio = AFFIX_RATIO ;
static double discount_factor = 1.0 ;
static LanguageIdentifier *language_identifier = 0 ;
static bool skip_numbers = false ;
static bool subsample_input = false ;
static uint64_t byte_limit = ~0 ;
static double unique_boost = UNIQUE_BOOST ;
static double smoothing_power = SMOOTHING_POWER ;
static double log_smoothing_power = 1.0 ;

// storage for input from training file
#ifndef NO_ICONV
static iconv_t  conversion = (iconv_t)-1 ;
static unsigned char original_buffer[BUFFER_SIZE] ;
static unsigned original_buffer_len = 0 ;
#endif /* !NO_ICONV */
static unsigned char translit_buffer[2*BUFFER_SIZE] ;
static unsigned translit_buffer_len = 0 ;
static unsigned translit_buffer_ptr = 0 ;
static FrList *buffered_lines = 0 ;

static unsigned char buffered_char = '\0' ;

/************************************************************************/
/*	Helper functions						*/
/************************************************************************/

// in trigram.C
void insert_frequency(uint32_t newelt, uint32_t *heap, size_t heaplen) ;
uint32_t adjusted_threshold(const uint32_t *frequencies) ;

//----------------------------------------------------------------------

static void usage(const char *argv0, const char *bad_arg)
{
   if (bad_arg)
      cerr << "Unrecognized argument " << bad_arg << endl << endl ;
   cerr << "MKLANGID version " VERSION "  Copyright 2011,2012 Ralf Brown/CMU -- GNU GPLv3" << endl ;
   cerr << "Usage: " << argv0 << " [=DBFILE] {options} file ... [{options} file ...]"
	<< endl ;
   cerr << "  Specify =DBFILE to use language database DBFILE instead of the\n" ;
   cerr << "  default " DEFAULT_LANGID_DATABASE << "; with ==DBFILE, the database\n" ;
   cerr << "  will not be updated (use -w to store results)" << endl ; ;
   cerr << "Options:" << endl ;
   cerr << "   -h       show this usage summary" << endl ;
   cerr << "   -l LANG  specify language of following files (use ISO-639 two-letter code)" << endl ;
   cerr << "   -r REG   specify regional variant of the language (use ISO two-letter" << endl ;
   cerr << "            country codes, e.g. for locale 'en_US', use -l en -r US" << endl ;
   cerr << "            [optional])" << endl ;
   cerr << "   -e ENC   specify the character encoding, e.g. iso8859-1, utf-8, etc." << endl ;
   cerr << "   -s SRC   specify the source of the training data (optional)" << endl ;
   cerr << "   -W SCR   specify writing system (script) of the training data (optional)" << endl ;
   cerr << "   -kK      collect top K n-grams by frequency (default " << topK << ")\n" ;
   cerr << "   -mN      require n-grams to consist of at least N bytes (min 3)\n" ;
   cerr << "   -MN      limit n-grams to at most N bytes (default " << DEFAULT_MAX_LENGTH << ", max " << ABSOLUTE_MAX_LENGTH << ")\n" ;
   cerr << "   -i       ignore blanks when processing files\n" ;
   cerr << "   -n       skip ngrams containing newlines in following files\n" ;
   cerr << "   -nn      skip ngrams starting with digits as well\n" ;
   cerr << "   -LN      limit training to first N bytes of input\n" ;
   cerr << "   -L@N     limit training to N bytes uniformly sampled from input\n" ;
   cerr << "   -b       omit bigram table from model for following files\n" ;
   cerr << "   -ON      set maximum oversampling factor to N (default " << max_oversample << ")" << endl ;
   cerr << "   -aX      set affix ratio; remove 'ABC' if c(ABCD) >= X * c(ABC)" << endl ;
   cerr << "   -dX      set probability discount factor for X" << endl ;
   cerr << "   -R SPEC  compute stop-grams relative to related language(s) listed in SPEC" << endl ;
   cerr << "   -B BOOST increase smoothed scores of n-grams unique to model by BOOST*" << endl ;
   cerr << "   -S SMTH  set smoothing power to SMTH (negative for logarithmic)" << endl;
   cerr << "   -1       convert Latin-1 input to UTF-8" << endl ;
   cerr << "   -2b      pad input bytes to 16 bits (big-endian)" << endl ;
   cerr << "   -2l      pad input bytes to 16 bits (little-endian)" << endl ;
   cerr << "   -2-      don't pad input bytes to 16 bits" << endl ;
   cerr << "   -8b      convert UTF8 input to UTF-16 (big-endian)" << endl ;
   cerr << "   -8l      convert UTF8 input to UTF-16 (little-endian)" << endl ;
   cerr << "   -8-      don't convert UTF8" << endl ;
   cerr << "   -AN      alignment: only start ngram at multiple of N (1,2,4)" << endl ;
   cerr << "   -f       following files are frequency lists (count then string)" << endl ;
   cerr << "   -fc      following files are frequency lists (count/string, word delim)" << endl ;
   cerr << "   -ft      following files are frequency lists (string/tab/count)" << endl ;
   cerr << "   -v       run verbosely" << endl ;
   cerr << "   -wFILE   write resulting vocabulary list to FILE in plain text" << endl ;
   cerr << "   -D       dump computed multi-trie to standard output" << endl ;
   cerr << "Notes:" << endl ;
   cerr << "\tThe -1 -b -f -i -n -nn -R -w flags reset after each group of files." << endl;
   cerr << "\t-2 and -8 are mutually exclusive -- the last one specified is used." << endl ;
   exit(1) ;
}

//----------------------------------------------------------------------

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

static void print_quoted_char(FILE *fp, uint8_t ch)
{
   switch (ch)
      {
      case '\0': fputs("\\0",fp) ;		break ;
      case '\f': fputs("\\f",fp) ;		break ;
      case '\n': fputs("\\n",fp) ;		break ;
      case '\r': fputs("\\r",fp) ;		break ;
      case '\t': fputs("\\t",fp) ;		break ;
      case ' ': fputs("\\ ",fp) ;		break ;
      case '\\': fputs("\\\\",fp) ;		break ;
      default:	 fputc(ch,fp) ;			break ;
      }
   return ;
}

/************************************************************************/
/*	Methods for class StopGramWeight				*/
/************************************************************************/

uint32_t StopGramWeight::weight(const uint8_t *key, unsigned keylen) const
{
   uint32_t raw = weights()->find(key,keylen) ;
   if (!scaled() && raw > 0)
      {
      double percent = raw / (double)TRIE_SCALE_FACTOR ;
      double proportion = percent / 100.0 ;
      uint32_t unscaled = (uint32_t)(proportion * totalBytes() + 0.5) ;
      return unscaled ;
      }
   else
      return raw ;
}

/************************************************************************/
/*	Data input functions						*/
/************************************************************************/

static FILE *open_sampled_input_file(const char *filename,
				     bool &piped,
				     size_t max_bytes)
{
   // load in the entire input file
   FILE *fp = FrOpenMaybeCompressedInfile(filename,piped) ;
   FrList *lines = 0 ;
   size_t numlines = 0 ;
   size_t total_bytes = 0 ;
   while (!feof(fp))
      {
      char line[FrMAX_LINE] ;
      line[0] = '\0' ;
      if (!fgets(line,sizeof(line),fp))
	 break ;
      line[FrMAX_LINE-1] = '\0' ;
      FrString *l = new FrString(line) ;
      total_bytes += l->stringLength() ;
      pushlist(l,lines) ;
      numlines++ ;
      }
   // reset the EOF condition
   fseek(fp,0,SEEK_SET) ;
   clearerr(fp) ;
   // subsample the lines we read to be just a little more than the
   //   desired number of bytes
   double interval = max_bytes / (double)total_bytes ;
   if (interval > 0.5)			// adjustment for high sampling rates
      interval += (interval-0.5)/6.0 ;
   size_t sampled = 0 ;
   if (interval >= 0.98)
      {
      buffered_lines = listreverse(lines) ;
      lines = 0 ;
      sampled = total_bytes ;
      }
   else
      {
      double avgline = total_bytes / (double)numlines ;
      double count = interval / 2.0 ;
      buffered_lines = 0 ;
      while (lines)
	 {
	 FrString *line = (FrString*)poplist(lines) ;
	 if (!line) continue ; //FIXME
	 size_t len = line->stringLength() ;
	 double increment = interval * len / avgline ;
	 if (((size_t)(count + increment) > (size_t)count) ||
	     interval >= 1.0)
	    {
	    pushlist(line,buffered_lines) ;
	    sampled += len ;
	    }
	 else
	    {
	    free_object(line) ;
	    }
	 count += increment ;
	 }
      }
//   if (verbose)
      {
      cout << "  Sampled " << sampled << " bytes from input file" ;
      if (sampled < byte_limit && max_bytes < total_bytes)
	 cout << " (requested " << max_bytes << ", filesize=" << total_bytes
	      << ")" ;
      cout << endl ;
      }
   // if we subsampled but didn't get enough bytes, try again with a higher
   //   limit
   if (sampled < byte_limit && total_bytes >= max_bytes)
      {
      FrCloseMaybeCompressedInfile(fp,piped) ;
      free_object(buffered_lines) ;
      buffered_lines = 0 ;
      max_bytes *= (max_bytes / (double)sampled * 1.01) ;
      return open_sampled_input_file(filename,piped,max_bytes) ;
      }
   return fp ;
}

//----------------------------------------------------------------------

static FILE *open_input_file(const char *filename, bool &piped)
{
   if (subsample_input && byte_limit != ~0U
       && alignment == 1) // can't currently sample UTF16
      {
      return open_sampled_input_file(filename,piped,byte_limit) ;
      }
   else
      return FrOpenMaybeCompressedInfile(filename,piped) ;
}

//----------------------------------------------------------------------

void close_input_file(FILE *fp, bool piped)
{
   // discard any remnants of buffered data from subsampling
   free_object(buffered_lines) ;
   buffered_lines = 0 ;
   // discard any remnants of transliteration
   translit_buffer_ptr = 0 ;
   translit_buffer_len = 0 ;
   // close the input file
   FrCloseMaybeCompressedInfile(fp,piped) ;
   return ;
}

//----------------------------------------------------------------------

static int read_input(FILE *fp, unsigned char *buf, size_t buflen)
{
   if (buffered_lines)
      {
      int count = 0 ;
      while ((unsigned long)count < buflen && buffered_lines)
	 {
	 size_t len = ((FrString*)buffered_lines->first())->stringLength() ;
	 if (count + len > buflen)
	    break ;
	 FrString *line = (FrString*)poplist(buffered_lines) ;
	 memcpy(buf,line->stringValue(),len) ;
	 buf += len ;
	 count += len ;
	 free_object(line) ;
	 }
      return count ;
      }
   else
      return fread(buf,1,buflen,fp) ;
}

//----------------------------------------------------------------------

static int fill_buffer(FILE *fp)
{
#ifndef NO_ICONV
   if (conversion != (iconv_t)-1)
      {
      // try to fill the input buffer
      int remainder = sizeof(original_buffer) - original_buffer_len ;
      int read_count = read_input(fp,original_buffer+original_buffer_len,
				  remainder) ;
      original_buffer_len += read_count ;
      // convert as much as possible
      char *origbuf = (char*)original_buffer ;
      size_t orig_len = original_buffer_len ;
      char *translitbuf = (char*)translit_buffer ;
      size_t translit_len = sizeof(translit_buffer) ;
      errno = 0 ;
      size_t count = iconv(conversion,&origbuf,&orig_len,&translitbuf,
			   &translit_len) ;
      if (count != (size_t)-1)
	 errno = 0 ;
      if (errno == EILSEQ)
	 {
	 // bad input, try to skip ahead
	 if (translit_len > 0)
	    {
	    *translitbuf++ = *origbuf++ ;
	    orig_len-- ;
	    translit_len-- ;
	    }
	 }
      else if (errno != EINVAL && errno != E2BIG)
	 {
	 // only need to deal with the error if it is not an
	 //   incomplete multibyte sequence or running out of room in
	 //   the output buffer; in this case, we just copy as much as
	 //   we can to the output buffer
	 while (orig_len > 0 && translit_len > 0)
	    {
	    *translitbuf++ = *origbuf++ ;
	    orig_len-- ;
	    translit_len-- ;
	    }
	 // reset the conversion state
	 iconv(conversion,0,0,0,0) ;
	 }
      else if (read_count == 0) // at end of file?
	 {
	 // anything still left in original_buffer is unconvertible
	 //  bytes, so just copy them over
	 translit_buffer_len = original_buffer_len ;
	 if (original_buffer_len)
	    {
	    memcpy(translit_buffer,original_buffer,original_buffer_len) ;
	    original_buffer_len = 0 ;
	    }
	 translit_buffer_ptr = 0 ;
	 return translit_buffer_len ;
	 }
      if (orig_len > 0)
	 {
	 // we have data remaining in the input buffer, so
	 //  copy it to the beginning for the next refill
	 memcpy(original_buffer,origbuf,orig_len) ;
	 }
      original_buffer_len = orig_len ;
      translit_buffer_ptr = 0 ;
      translit_buffer_len = (translitbuf - (char*)translit_buffer) ;
      }
   else
#endif /* NO_ICONV */
      {
      translit_buffer_ptr = 0 ;
      translit_buffer_len
	 = read_input(fp,translit_buffer,sizeof(translit_buffer)) ;
      }
   return translit_buffer_len ;
}

//----------------------------------------------------------------------

static bool more_data(FILE *fp)
{
   return (translit_buffer_len > translit_buffer_ptr) || !feof(fp) ;
}

//----------------------------------------------------------------------

#if 0
static int peek_at_buffer(FILE *fp)
{
   if (translit_buffer_ptr >= translit_buffer_len)
      {
      if (fill_buffer(fp) <= 0)
	 return EOF ;
      }
   return translit_buffer[translit_buffer_ptr] ; // don't advance pointer
}
#endif

//----------------------------------------------------------------------

static int get_from_buffer(FILE *fp)
{
   if (translit_buffer_ptr >= translit_buffer_len)
      {
      if (fill_buffer(fp) <= 0)
	 return EOF ;
      }
   return translit_buffer[translit_buffer_ptr++] ; // advance pointer
}

//----------------------------------------------------------------------

static unsigned get_codepoint(FILE *fp)
{
   int byte = get_from_buffer(fp) ;
   if (byte == EOF)
      {
      return byte ;
      }
   // ensure no sign-extension
   unsigned codepoint = byte & 0xFF ;
   if ((byte & 0x80) != 0 &&
       !(bigram_extension == BigramExt_ASCIILittleEndian ||
	 bigram_extension == BigramExt_ASCIIBigEndian))
      {
      // figure out how many bytes make up the code point, and put the
      //   highest-order bits stored in the first byte into the
      //   codepoint variable
      unsigned extra = 0 ;
      if ((byte & 0xE0) == 0xC0)
	 {
	 codepoint = (byte & 0x1F) ;
	 extra = 1 ;
	 }
      else if ((byte & 0xF0) == 0xE0)
	 {
	 codepoint = (byte & 0x0F) ;
	 extra = 2 ;
	 }
      else if ((byte & 0xF8) == 0xF0)
	 {
	 codepoint = (byte & 0x07) ;
	 extra = 3 ;
	 }
      else if ((byte & 0xFC) == 0xF8)
	 {
	 codepoint = (byte & 0x03) ;
	 extra = 4 ;
	 }
      else if ((byte & 0xFE) == 0xFC)
	 {
	 codepoint = (byte & 0x01) ;
	 extra = 5 ;
	 }
      // read in the additional bytes specified by the high bits of the
      //   codepoint's first byte
      for (size_t i = 1 ; i <= extra ; i++)
	 {
	 byte = get_from_buffer(fp) ;
	 if (byte == EOF)
	    return byte ;
	 else if ((byte & 0xC0) != 0x80)
	    break ;			// invalid UTF8
	 // each extra byte gives us six more bits of the codepoint
	 codepoint = (codepoint << 6) | (byte & 0x3F) ;
	 }
      }
   return codepoint ;
}

//----------------------------------------------------------------------

#if 0
static int peek_byte(FILE *fp, uint64_t byte_count, bool ignore_whitespace = false)
{
   if (convert_Latin1)
      {
      if (buffered_char >= 0x80)
	 return buffered_char ;
      if (ignore_whitespace)
	 {
	 while (peek_at_buffer(fp) == ' ')
	    {
	    (void)get_from_buffer(fp) ;
	    }
	 }
      return peek_at_buffer(fp) ;
      }
   else if (bigram_extension == BigramExt_None)
      {
      if (ignore_whitespace)
	 {
	 while (peek_at_buffer(fp) == ' ')
	    {
	    (void)get_from_buffer(fp) ;
	    }
	 }
      return peek_at_buffer(fp) ;
      }
   else if ((byte_count & 1) != 0)
      return buffered_char ;
   else
      return peek_at_buffer(fp) ;
}
#endif

//----------------------------------------------------------------------

static int get_byte(FILE *fp, uint64_t &byte_count,
		    bool ignore_whitespace = false)
{
   if (byte_count >= byte_limit)
      {
      translit_buffer_ptr = translit_buffer_len ;
      return EOF ;
      }
   if (convert_Latin1)
      {
      int cp ;
      if (buffered_char >= 0x80)
	 {
	 cp = buffered_char ;
	 buffered_char = 0 ;
	 }
      else
	 {
	 do {
	    cp = get_from_buffer(fp) ;
	    } while (ignore_whitespace && cp == ' ') ;
	 if (cp != EOF)
	    {
	    if ((cp & 0xFF) >= 0x80)
	       {
	       buffered_char = (unsigned char)(0x80 | (cp & 0x3F)) ;
	       cp = 0xC0 | ((cp & 0xFF) >> 6) ;
	       }
	    }
	 }
      if (cp != EOF)
	 byte_count++ ;
      return cp ;
      }
   if (bigram_extension == BigramExt_None)
      {
      int cp ;
      do {
         cp = get_from_buffer(fp) ;
         } while (ignore_whitespace && cp == ' ') ;
      byte_count++ ;
      return cp ;
      }
   if ((byte_count & 1) != 0)
      {
      byte_count++ ;
      return buffered_char ;
      }
   int cp = get_codepoint(fp) ;
   if (cp == EOF)
      return cp ;
   byte_count++ ;
   if (bigram_extension == BigramExt_ASCIILittleEndian ||
       bigram_extension == BigramExt_UTF8LittleEndian)
      {
      buffered_char = (unsigned char)((cp >> 8) & 0xFF) ;
      return (cp & 0xFF) ;
      }
   else
      {
      buffered_char = (unsigned char)(cp & 0xFF) ;
      return (cp >> 8) & 0xFF ;
      }
}

/************************************************************************/
/************************************************************************/

static bool initialize_transliteration(const char *from, const char *to)
{
#ifndef NO_ICONV
   conversion = (from && to) ? iconv_open(to,from) : (iconv_t)-1 ;
   if (conversion == (iconv_t)-1)
      return !(from && to) ;
   // reset conversion state
   iconv(conversion,0,0,0,0) ;
#endif /* !NO_ICONV */
   return true ;
}

//----------------------------------------------------------------------

static bool shutdown_transliteration()
{
#ifndef NO_ICONV
   if (conversion != (iconv_t)-1)
      iconv_close(conversion) ;
#endif /* !NO_ICONV */
   return true ;
}

//----------------------------------------------------------------------

static unsigned set_oversampling(unsigned topK, unsigned abs_min_len,
				 unsigned min_len, bool aligned)
{
   if (abs_min_len < min_len)
      {
      double base = aligned ? 2.0 : 1.0 ;
      double oversample
	 = ::pow(2,base+(min_len-abs_min_len)/5.0) ;
      if (oversample > max_oversample)
	 oversample = max_oversample ;
      return (unsigned)(topK * oversample) ;
      }
   else
      return topK ;
}

//----------------------------------------------------------------------

static uint64_t count_trigrams(const char **filelist, unsigned num_files,
			       TrigramCounts &counts, bool skip_newlines,
			       bool ignore_whitespace, bool aligned,
			       BigramCounts **bigrams)
{
   cout << "Counting trigrams" << endl ;
   uint64_t total_bytes = 0 ;
   for (size_t i = 0 ; i < num_files ; i++)
      {
      const char *filename = filelist[i] ;
      if (filename && *filename)
	 {
	 bool piped ;
	 FILE *fp = open_input_file(filename,piped) ;
	 if (!fp)
	    {
	    cerr << "Error opening '" << filename << "' for reading" << endl ;
	    }
	 else
	    {
	    cout << "  Processing " << filename << endl ;
	    // prime the trigrams with the first two bytes
	    uint8_t c1 = get_byte(fp,total_bytes,ignore_whitespace) ;
	    uint8_t c2 = get_byte(fp,total_bytes,ignore_whitespace) ;
	    unsigned offset = 0 ;
	    // now process the rest of the file, incrementing the count
	    //   for every trigram encountered
	    while (more_data(fp))
	       {
	       int c3 = get_byte(fp,total_bytes,ignore_whitespace) ;
	       if (c3 == EOF)
		  break ;
	       if (offset % alignment == 0)
		  counts.incr(c1,c2,(uint8_t)c3) ;
	       c1 = c2 ;
	       c2 = (uint8_t)c3 ;
	       offset++ ;
	       }
	    close_input_file(fp,piped) ;
	    }
	 }
      }
   if (bigrams)
      (*bigrams) = new BigramCounts(counts) ;
   if (bigram_extension == BigramExt_ASCIILittleEndian ||
       bigram_extension == BigramExt_UTF8LittleEndian)
      {
      // don't bother with trigrams starting with a NUL, as those will
      //  split a 16-bit character
      for (size_t i = 0 ; i < 256 ; i++)
	 {
	 for (size_t j = 0 ; j < 256 ; j++)
	    {
	    counts.clear('\0',i,j) ;
	    }
	 }
      if (skip_newlines)
	 {
	 counts.clear(' ','\0',' ') ;
	 for (size_t i = 0 ; i < 256 ; i++)
	    {
	    counts.clear(i,'\0','\r') ;
	    counts.clear(i,'\0','\n') ;
	    counts.clear('\r','\0',i) ;
	    counts.clear('\n','\0',i) ;
	    counts.clear('\t','\0',i) ;
	    }
	 }
      if (skip_numbers)
	 {
	 for (size_t c1 = '0' ; c1 <= '9' ; c1++)
	    {
	    counts.clear('.','\0',c1) ;
	    counts.clear(',','\0',c1) ;
	    counts.clear(c1,'\0','.') ;
	    counts.clear(c1,'\0',',') ;
	    for (size_t c3 = '0' ; c3 <= '9' ; c3++)
	       {
	       counts.clear(c1,'\0',c3) ;
	       }
	    }
	 // also skip certain repeated punctuation marks which are not informative
	 counts.clear('-','\0','-') ;
	 counts.clear('=','\0','=') ;
	 counts.clear('*','\0','*') ;
	 counts.clear('.','\0','.') ;
	 counts.clear('?','\0','?') ;
	 }
      }
   else if (bigram_extension == BigramExt_ASCIIBigEndian ||
	    bigram_extension == BigramExt_UTF8BigEndian)
      {
      if (bigram_extension == BigramExt_ASCIIBigEndian)
	 {
	 // force trigrams to start with a NUL, as otherwise we will
	 //  split a 16-bit character
	 for (size_t i = 1 ; i < 256 ; i++)
	    {
	    for (size_t j = 0 ; j < 256 ; j++)
	       {
	       counts.clear(i,'\0',j) ;
	       }
	    }
	 }
      if (skip_newlines)
	 {
	 for (size_t i = 0 ; i < 256 ; i++)
	    {
	    counts.clear(i,'\0','\r') ;
	    counts.clear(i,'\0','\n') ;
	    counts.clear('\0','\r',i) ;
	    counts.clear('\0','\n',i) ;
	    counts.clear('\0','\t',i) ;
	    }
	 }
      }
   else if (bigram_extension == BigramExt_None && !aligned)
      {
      if (skip_newlines)
	 {
	 for (size_t i = 0 ; i < 256 ; i++)
	    {
	    counts.clear(' ',' ',i) ;
	    for (size_t j = 0 ; j < 256 ; j++)
	       {
	       counts.clear(i,j,'\r') ;
	       counts.clear(i,j,'\n') ;
	       counts.clear(i,'\r',j) ;
	       counts.clear(i,'\n',j) ;
	       counts.clear('\r',i,j) ;
	       counts.clear('\n',i,j) ;
	       counts.clear('\t',i,j) ;
	       if (alignment == 1)
		  {
		  counts.clear(i,j,'\0') ;
		  counts.clear(i,'\0',j) ;
		  counts.clear('\0',i,j) ;
		  }
	       }
	    }
	 }
      if (skip_numbers)
	 {
	 for (size_t c1 = '0' ; c1 <= '9' ; c1++)
	    {
	    for (size_t c2 = '0' ; c2 <= '9' ; c2++)
	       {
	       counts.clear('.',c1,c2) ;
	       counts.clear(',',c1,c2) ;
	       for (size_t c3 = 0 ; c3 <= 0xFF ; c3++)
		  {
		  counts.clear(c1,c2,c3) ;
		  if (c3 >= '0' && c3 <= '9')
		     {
		     counts.clear(c1,'.',c3) ;
		     counts.clear(c1,',',c3) ;
		     }
		  }
	       }
	    }
	 // also skip certain repeated punctuation marks which are not informative
	 counts.clear('-','-','-') ;
	 counts.clear('=','=','=') ;
	 counts.clear('*','*','*') ;
	 counts.clear('.','.','.') ;
	 counts.clear('?','?','?') ;
	 }
      }
   cout << "  Processed " << total_bytes << " bytes" << endl ;
   return total_bytes ;
}

//----------------------------------------------------------------------

static bool count_ngrams(FILE *fp, NybbleTrie *ngrams,
			 unsigned min_length, unsigned max_length,
			 bool skip_newlines, bool ignore_whitespace,
			 bool aligned)
{
   if (max_length < min_length || max_length == 0)
      return false ;
   uint8_t ngram[max_length] ;
   uint64_t byte_count = 0 ;
   // fill the ngram buffer, except for the last byte
   for (size_t i = 0 ; i+1 < max_length ; i++)
      {
      int c = get_byte(fp,byte_count,ignore_whitespace) ;
      if (c == EOF)
	 return false ;
      ngram[i] = (uint8_t)c ;
      }
   // now iterate through the file, counting the ngrams
   unsigned offset = 0 ;
   while (more_data(fp))
      {
      int c = get_byte(fp,byte_count,ignore_whitespace) ;
      if (c == EOF)
	 break ;
      // fill the last byte of the buffer
      ngram[max_length-1] = (uint8_t)c ;
      // increment n-gram counts if they are an extension of a known n-gram,
      //   but don't include newlines if told not to do so
      size_t max_len = max_length ;
      if (skip_newlines)
         {
	 if (bigram_extension == BigramExt_ASCIIBigEndian ||
	     bigram_extension == BigramExt_UTF8BigEndian)
	    {
	    for (size_t i = (min_length - 1)/2 ; i < (max_length/2) ; i++)
	       {
	       if (ngram[2*i] == '\0' &&
		   (ngram[2*i+1] == '\n' || ngram[2*i+1] == '\r' || ngram[2*i+1] == '\0'))
		  {
		  max_len = 2*i ;
		  break ;
		  }
	       }
	    }
	 else if (bigram_extension == BigramExt_ASCIILittleEndian ||
		  bigram_extension == BigramExt_UTF8LittleEndian)
	    {
	    for (size_t i = (min_length - 1)/2 ; i < (max_length/2) ; i++)
	       {
	       if (ngram[2*i+1] == '\0' &&
		   (ngram[2*i] == '\n' || ngram[2*i] == '\r' || ngram[2*i] == '\0'))
		  {
		  max_len = 2*i ;
		  break ;
		  }
	       }
	    }
	 else
	    {
	    for (size_t i = min_length - 1 ; i < max_length ; i++)
	       {
	       if (ngram[i] == '\n' || ngram[i] == '\r' ||
		   (!aligned && bigram_extension == BigramExt_None && ngram[i] == '\0'))
		  {
		  max_len = i ;
		  break ;  
		  }
	       }
            }
         }
      if (alignment == 2 && min_length > 3 && ngram[0] == '\0' && ngram[2] == '\0')
	 {
	 // check for big-endian two-character sequences we weren't
	 //   able to filter at the trigram stage
	 if (skip_newlines)
	    {
	    if (ngram[1] == ' ' && ngram[3] == ' ')
	       max_len = 0 ;
	    }
	 if (skip_numbers)
	    {
	    if (isdigit(ngram[3]) &&
		(isdigit(ngram[1]) || ngram[1] == '.' || ngram[1] == ','))
	       max_len = 0 ;
	    else if (isdigit(ngram[1]) &&
		     (ngram[3] == '.' || ngram[3] == ','))
	       max_len = 0 ;
	    }
	 if ((ngram[1] == '-' && ngram[3] == '-') ||
	     (ngram[1] == '=' && ngram[3] == '=') ||
	     (ngram[1] == '*' && ngram[3] == '*') ||
	     (ngram[1] == '.' && ngram[3] == '.') ||
	     (ngram[1] == '?' && ngram[3] == '?'))
	    max_len = 0 ;
	 }
      if (max_len >= min_length && (offset % alignment) == 0)
	 ngrams->incrementExtensions(ngram,min_length-1,max_len) ;
      // shift the buffer by one byte
      memmove(ngram,ngram+1,max_length-1) ;
      }
   return true ;
}

//----------------------------------------------------------------------

static bool remove_suffix(const NybbleTrieNode *node, const uint8_t *key, unsigned keylen,
			  void *user_data)
{
   NgramEnumerationData *enum_data = (NgramEnumerationData*)user_data ;
   unsigned alignment = enum_data->m_alignment ;
   if (keylen == enum_data->m_desired_length && keylen >= alignment + enum_data->m_min_length)
      {
      NybbleTrie *trie = enum_data->m_oldngrams ;
      NybbleTrieNode *suffix = trie->findNode(key+alignment,keylen-alignment) ;
//      if (suffix && suffix->frequency() >= affix_ratio * node->frequency())
      if (suffix && node->frequency() >= affix_ratio * suffix->frequency())
	 suffix->setFrequency(0) ;
      }
   return true ;
}

//----------------------------------------------------------------------

static bool find_max_frequency(const NybbleTrieNode *node, const uint8_t * /*key*/,
			       unsigned /*keylen*/, void *user_data)
{
   uint32_t *freqptr = (uint32_t*)user_data ;
   uint32_t freq = node->frequency() ;
   if (*freqptr == (uint32_t)~0) // parent node?
      *freqptr = 0 ;
   else if (freq > *freqptr)
      *freqptr = freq ;
   return true ;
}

//----------------------------------------------------------------------

static bool find_ngram_cutoff(const NybbleTrieNode *node,
			      const uint8_t * key, unsigned keylen,
			      void *user_data)
{
   NgramEnumerationData *enum_data = (NgramEnumerationData*)user_data ;
   if (keylen >= minimum_length ||
       (enum_data->m_max_length < minimum_length &&
	keylen >= enum_data->m_max_length))
      {
      uint32_t freq = node->frequency() ;
      if (freq > 0 && freq > enum_data->m_frequencies[0])
	 {
	 // an optimization to eliminate extraneous n-grams: if the
	 //   node has only a single child, and that child has the
	 //   same frequency, this means that every occurrence of the
	 //   current n-gram is a prefix of that child n-gram, so
	 //   don't bother counting it; generalized to single-child
	 //   nodes where the child has almost the same frequency.
	 uint32_t max_freq = (uint32_t)~0 ;
	 if (node->enumerateChildren(enum_data->m_oldngrams,(uint8_t*)key,
				     8*(keylen+1), 8*keylen, 
				     find_max_frequency, &max_freq) &&
	      (max_freq < affix_ratio * freq ||
	       (keylen == minimum_length && affix_ratio < MINLEN_AFFIX_RATIO &&
		max_freq < MINLEN_AFFIX_RATIO * freq)))
	    {
	    insert_frequency(freq,enum_data->m_frequencies,
			     enum_data->m_topK) ;
	    enum_data->m_count++ ;
	    }
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

static bool filter_ngrams(const NybbleTrieNode *node, const uint8_t *key,
			  unsigned keylen, void *user_data)
{
   NgramEnumerationData *enum_data = (NgramEnumerationData*)user_data ;
   if (keylen >= minimum_length ||
       (enum_data->m_max_length < minimum_length &&
	keylen >= enum_data->m_max_length))
      {
      uint32_t freq = node->frequency() ;
      uint32_t max_freq = (uint32_t)~0 ;
      if (freq >= enum_data->m_min_freq &&
	  node->enumerateChildren(enum_data->m_oldngrams,(uint8_t*)key,
				  8*(keylen+1), 8*keylen,
				  find_max_frequency, &max_freq) &&
	  (max_freq < affix_ratio * freq ||
	   (keylen == minimum_length && affix_ratio < MINLEN_AFFIX_RATIO &&
	    max_freq < MINLEN_AFFIX_RATIO * freq)))
	 {
//for(size_t i = 0;i<keylen;i++)print_quoted_char(stdout,key[i]);
//cout <<" "<<keylen<<"@"<<freq<<endl;
	 enum_data->m_ngrams->insert(key,keylen,freq,node->isStopgram()) ;
	 enum_data->m_inserted_ngram = true ;
	 if (keylen == enum_data->m_max_length)
	    enum_data->m_have_max_length = true ;
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

static NybbleTrie *restrict_ngrams(NybbleTrie *ngrams, unsigned top_K,
				   unsigned min_length, unsigned max_length,
				   unsigned minlen, bool &have_max_length,
				   bool show_threshold = false)
{
   if (minlen > max_length)
      minlen = max_length ;
   uint32_t top_frequencies[top_K] ;
   memset(top_frequencies,'\0',top_K*sizeof(uint32_t)) ;
   NybbleTrie *new_ngrams = new NybbleTrie ;
   NgramEnumerationData enum_data(have_max_length) ;
   enum_data.m_oldngrams = ngrams ;
   enum_data.m_ngrams = new_ngrams ;
   enum_data.m_min_length = min_length ;
   enum_data.m_max_length = max_length ;
   enum_data.m_frequencies = top_frequencies ;
   enum_data.m_topK = top_K ;
   uint8_t keybuf[max_length+1] ;
   // figure out the threshold we need to limit the total n-grams to the
   //   desired number
   enum_data.m_count = 0 ;
   enum_data.m_min_freq = 0 ;
   unsigned required = top_K / (maximum_length - max_length + 3) ;
   if (!ngrams->enumerate(keybuf,max_length,find_ngram_cutoff,&enum_data)
       || enum_data.m_count < required)
      {
      cout << "Only " << enum_data.m_count << " distinct ngrams at length "
	   << max_length << ": collect more data" << endl ;
      if (max_length < maximum_length)
	 {
	 delete new_ngrams ;
	 new_ngrams = 0 ;
	 return new_ngrams ;
	 }
      }
//   uint32_t threshold = adjusted_threshold(top_frequencies) ;
   uint32_t threshold = top_frequencies[0] ;
   if (enum_data.m_count < top_K && max_length == maximum_length)
      threshold = 1 ;
   enum_data.m_min_freq = threshold ;
   if (show_threshold)
      {
      cout << "  Enumerating ngrams of length " << minlen << " to "
	   << max_length << " occurring at least " << threshold << " times"
	   << endl ;
      }
   enum_data.m_have_max_length = false ;
   if (!ngrams->enumerate(keybuf,max_length,filter_ngrams,&enum_data) ||
       !enum_data.m_inserted_ngram)
      {
      delete new_ngrams ;
      new_ngrams = 0 ;
      }
   FramepaC_gc() ;
   return new_ngrams ;
}

//----------------------------------------------------------------------

static NybbleTrie *count_ngrams(const char **filelist, unsigned num_files,
				NybbleTrie *ngrams,
				unsigned min_length, unsigned max_length,
				bool &have_max_length,
				bool skip_newlines, bool ignore_whitespace,
				bool aligned)
{
   cout << "Counting n-grams up to length " << max_length << endl ;
   for (size_t i = 0 ; i < num_files ; i++)
      {
      const char *filename = filelist[i] ;
      if (filename && *filename)
	 {
	 bool piped ;
	 FILE *fp = open_input_file(filename,piped) ;
	 if (fp)
	    {
	    cout << "  Processing " << filename << endl ;
	    count_ngrams(fp,ngrams,min_length,max_length,skip_newlines,
			 ignore_whitespace,aligned) ;
	    close_input_file(fp,piped) ;
	    }
	 // no need to squawk if problem opening file, as we will normally
	 //   have already reported the problem while processing trigrams
	 }
      }
   unsigned minlen = minimum_length ;
   if (minlen > max_length)
      minlen = max_length ;
   unsigned top_K = set_oversampling(topK,min_length,minimum_length,aligned) ;
   uint32_t top_frequencies[top_K] ;
   memset(top_frequencies,'\0',top_K*sizeof(uint32_t)) ;
   NybbleTrie *new_ngrams = new NybbleTrie ;
   NgramEnumerationData enum_data(have_max_length) ;
   enum_data.m_oldngrams = ngrams ;
   enum_data.m_ngrams = new_ngrams ;
   enum_data.m_min_length = min_length ;
   enum_data.m_max_length = max_length ;
   enum_data.m_frequencies = top_frequencies ;
   enum_data.m_topK = top_K ;
   enum_data.m_alignment = alignment ;
   uint8_t keybuf[max_length+1] ;
   // find the threshold at which to cut off ngrams to limit to topK
   if (verbose)
      {
      cout << "  Determining threshold for ngrams of length " << minlen
	   << " to " << max_length << endl ;
      }
   // remove any suffixes of an n-gram which have nearly the same
   //   frequency as the n-gram containing them (but don't remove
   //   minimum-length n-grams)
   for (size_t len = min_length + 2 ; len <= max_length ; len++)
      {
      enum_data.m_desired_length = len ;
      (void)ngrams->enumerate(keybuf,len,remove_suffix,&enum_data) ;
      }
   // figure out the threshold we need to limit the total n-grams to the
   //   desired number
   enum_data.m_count = 0 ;
   enum_data.m_min_freq = 1 ;
   unsigned required = top_K / (maximum_length - max_length + 3) ;
   enum_data.m_frequencies[0] = 0 ;
   if (!ngrams->enumerate(keybuf,max_length,find_ngram_cutoff,&enum_data)
       || enum_data.m_count < required)
      {
      cout << "Only " << enum_data.m_count << " distinct ngrams at length "
	   << max_length << ": collect more data" << endl ;
      if (max_length < maximum_length)
	 {
	 delete new_ngrams ;
	 new_ngrams = 0 ;
	 return new_ngrams ;
	 }
      }
   uint32_t threshold = adjusted_threshold(top_frequencies) ;
   if (enum_data.m_count < top_K /*&& max_length == maximum_length*/)
      threshold = 1 ;
   enum_data.m_min_freq = threshold ;
   cout << "  Enumerating ngrams of length " << minlen << " to "
	<< max_length << " occurring at least " << threshold << " times"
	<< endl ;
   enum_data.m_have_max_length = false ;
   if (!ngrams->enumerate(keybuf,max_length,filter_ngrams,&enum_data) ||
       !enum_data.m_inserted_ngram)
      {
      delete new_ngrams ;
      new_ngrams = 0 ;
      }
   FramepaC_gc() ;
   return new_ngrams ;
}

//----------------------------------------------------------------------

static void accumulate_stop_grams(FILE *fp, NybbleTrie *stop_grams,
				  NybbleTriePointer **states,
				  bool ignore_whitespace)
{
   unsigned maxkey = stop_grams->longestKey() ;
   uint64_t byte_count = 0 ;
   while (more_data(fp))
      {
      int keybyte = get_byte(fp,byte_count,ignore_whitespace) ;
      if (keybyte == EOF)
	 break ;
      delete states[maxkey] ;
      states[maxkey] = 0 ;
      states[0] = new NybbleTriePointer(stop_grams) ;
      for (size_t i = maxkey ; i > 0 ; i--)
	 {
	 if (!states[i-1])
	    continue ;
	 if (states[i-1]->extendKey((uint8_t)(keybyte&0xFF)))
	    {
	    // check whether we're at a leaf node; if so, increment its frequency
	    NybbleTrieNode *node = states[i-1]->node() ;
	    if (node && node->leaf())
	       {
	       node->incrFrequency() ;
	       }
	    states[i] = states[i-1] ;
	    }
	 else
	    {
	    delete states[i-1] ;
	    states[i] = 0 ;
	    }
	 states[i-1] = 0 ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

static void accumulate_stop_grams(FILE *fp, NybbleTrie *stop_grams,
				  bool ignore_whitespace)
{
   if (!fp || !stop_grams)
      return ;
   unsigned maxkey = stop_grams->longestKey() ;
   NybbleTriePointer **states = FrNewC(NybbleTriePointer*,maxkey+2) ;
   if (states)
      {
      accumulate_stop_grams(fp,stop_grams,states,ignore_whitespace) ;
      for (size_t i = 0 ; i <= maxkey ; i++)
	 {
	 delete states[i] ;
	 }
      FrFree(states) ;
      }
   return ;
}

//----------------------------------------------------------------------

static bool add_stop_gram(const NybbleTrieNode *node,
			  const uint8_t *key, unsigned keylen,
			  void *user_data)
{
   if (keylen > 2 && node && node->leaf() && 
       (node->frequency() == 0 || node->isStopgram()))
      {
      NybbleTrie *ngrams = (NybbleTrie*)user_data ;
      StopGramWeight *weights = (StopGramWeight*)ngrams->userData() ;
      uint32_t weight = weights->weight(key,keylen) ;
      ngrams->insert(key,keylen,weight,true) ;
      }
   return true ;
}

//----------------------------------------------------------------------

static bool boost_unique_ngram(const NybbleTrieNode *node,
			       const uint8_t *key, unsigned keylen,
			       void *user_data)
{
   NybbleTrie *stop_grams = (NybbleTrie*)user_data ;
   if (node && node->leaf() && node->frequency() > 0 && !node->isStopgram())
      {
      NybbleTrieNode *sgnode = stop_grams->findNode(key,keylen) ;
      if (!sgnode || !sgnode->leaf())
	 {
	 uint32_t freq = node->frequency() ;
	 // boost the weight of this node
	 double boost = scale_frequency(unique_boost,smoothing_power,
					log_smoothing_power) ;
	 uint32_t boosted = (uint32_t)(freq * boost + 0.9) ;
	 if (boosted < node->frequency()) // did we roll over?
	    boosted = 0xFFFFFFFF ;
	 NybbleTrieNode *n = (NybbleTrieNode*)node ;
	 n->setFrequency(boosted) ;
	 }
      else if (0)
	 {
	 StopGramWeight *weights = (StopGramWeight*)stop_grams->userData() ;
	 uint32_t freq = node->frequency() ;
	 uint32_t sgfreq
	    = scaled_frequency(sgnode->frequency(),weights->totalBytes(),
			       smoothing_power,log_smoothing_power) ;
	 if (freq > 2 * sgfreq)
	    {
	    cout << "alternate" << endl ;
	    }
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

static bool add_stop_grams(const char **filelist, unsigned num_files,
			   NybbleTrie *ngrams, NybbleTrie *stop_grams,
			   const NybbleTrie *ngram_weights, bool scaled,
			   uint64_t total_bytes, bool ignore_whitespace)
{
   if (!stop_grams || stop_grams->size() <= 100)
      return true ;
   cout << "Computing Stop-Grams" << endl ;
   // accumulate counts for all the ngrams in the stop-gram list
   for (size_t i = 0 ; i < num_files ; i++)
      {
      const char *filename = filelist[i] ;
      if (filename && *filename)
	 {
	 bool piped ;
	 FILE *fp = open_input_file(filename,piped) ;
	 if (fp)
	    {
	    cout << "  Processing " << filename << endl ;
	    accumulate_stop_grams(fp,stop_grams,ignore_whitespace) ;
	    close_input_file(fp,piped) ;
	    }
	 // no need to squawk if problem opening file, as we will normally
	 //   have already reported the problem while processing trigrams
	 }
      }
   StopGramWeight stop_gram_weight(ngram_weights,total_bytes,scaled) ;
   ngrams->setUserData(&stop_gram_weight) ;
   stop_grams->setUserData(&stop_gram_weight) ;
   // 'stop_grams' contains the union of all n-grams in the models for
   //   closely-related languages.  Any n-grams in the current language's
   //   model which don't appear in any of the other models get a boost,
   //   because they are particularly strong evidence that the text is in
   //   the given language
   unsigned longkey = stop_grams->longestKey() ;
   if (ngrams->longestKey() > longkey)
      longkey = ngrams->longestKey() ;
   uint8_t ngram_buf[longkey+1] ;
   if (unique_boost > 1.0)
      {
      ngrams->enumerate(ngram_buf,ngrams->longestKey(),boost_unique_ngram,
			stop_grams) ;
      }
   // scan the stop-gram list and add any with a zero count to the main
   //   n-gram list, flagged as stop-grams
   stop_grams->enumerate(ngram_buf,stop_grams->longestKey(),
			 add_stop_gram,ngrams) ;
   return true ;
}

//----------------------------------------------------------------------

static void merge_bigrams(NybbleTrie *ngrams, const BigramCounts *bigrams,
			  bool scaled, uint64_t total_bytes)
{
   if (!ngrams || !bigrams)
      return ;
   uint8_t keybuf[3] ;
   uint32_t min_count = 2 ;
   for (unsigned c1 = 0 ; c1 < 256 ; c1++)
      {
      keybuf[0] = (uint8_t)c1 ;
      for (unsigned c2 = 0 ; c2 < 256 ; c2++)
	 {
	 uint32_t count = bigrams->count(c1,c2) ;
	 if (count < min_count)
	    continue ;
	 keybuf[1] = (uint8_t)c2 ;
	 if (scaled)
	    count = scaled_frequency(count,total_bytes,smoothing_power,
				     log_smoothing_power) ;
	 ngrams->insert(keybuf,2,count,false) ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

static bool add_ngram(const NybbleTrieNode *node, const uint8_t *key,
		      unsigned keylen, void *user_data)
{
   MultiTrie *trie = (MultiTrie *)user_data ;
   if (trie && node)
      {
      uint32_t freq = node->frequency() ;
      trie->insert(key,keylen,trie->currentLanguage(),freq,
		   node->isStopgram()) ;
      }
   return true ;
}

//----------------------------------------------------------------------

static void add_ngrams(const NybbleTrie *ngrams, uint64_t total_bytes,
		       const LanguageID &opts, const char *filename)
{
   if (ngrams)
      {
      uint32_t num_langs = language_identifier->numLanguages() ;
      // add the new language ID to the global database
      uint32_t langID = language_identifier->addLanguage(opts,total_bytes) ;
      if (langID < num_langs)
	 {
	 char *spec = language_identifier->languageDescriptor(langID) ;
	 cerr << "Duplicate language specification " << spec
	      << " encountered in " << filename
	      << ",\n  ignoring data to avoid database errors." << endl ;
	 FrFree(spec) ;
	 }
      MultiTrie *trie = language_identifier->unpackedTrie() ;
      if (trie)
	 {
	 trie->setLanguage(langID) ;
	 uint8_t keybuf[10000] ;
	 ngrams->enumerate(keybuf,sizeof(keybuf),add_ngram,trie) ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

static int compare_langcode(const LanguageID *id1, const LanguageID *id2)
{
   if (!id1)
      return id2 ? +1 : 0 ;
   else if (!id2)
      return -1 ;
   const char *l1 = id1->language() ;
   const char *l2 = id2->language() ;
   if (!l1)
      return l2 ? +1 : 0 ;
   else if (!l2)
      return -1 ;
   return strcmp(l1,l2) ;
}

//----------------------------------------------------------------------

static int compare_codepair(const LanguageID *id1, const LanguageID *id2)
{
   if (!id1)
      return id2 ? +1 : 0 ;
   else if (!id2)
      return -1 ;
   const char *l1 = id1->language() ;
   const char *l2 = id2->language() ;
   if (!l1)
      return l2 ? +1 : 0 ;
   else if (!l2)
      return -1 ;
   int cmp = strcmp(l1,l2) ;
   if (cmp)
      return cmp ;
   const char *enc1 = id1->encoding() ;
   const char *enc2 = id2->encoding() ;
   if (!enc1)
      return enc2 ? +1 : 0 ;
   else if (!enc2)
      return -1 ;
   return strcmp(enc1,enc2) ;
}

//----------------------------------------------------------------------

static unsigned count_languages(const LanguageIdentifier *id,
				int (*cmp)(const LanguageID*,const LanguageID*))
{
   unsigned count = 0 ;
   if (id)
      {
      size_t num_langs = id->numLanguages() ;
      LanguageID **langcodes = FrNewC(LanguageID *,num_langs) ;
      for (size_t i = 0 ; i < num_langs ; i++)
	 {
	 langcodes[i] = (LanguageID*)id->languageInfo(i) ;
	 }
      FrQuickSortPtr(langcodes,num_langs,cmp) ;
      count = 1 ;
      for (size_t i = 1 ; i < num_langs ; i++)
	 {
	 if (cmp(langcodes[i-1],langcodes[i]) != 0)
	    count++ ;
	 }
      FrFree(langcodes) ;
      }
   return count ;
}

//----------------------------------------------------------------------

static bool save_database(const char *database_file)
{
   if (!database_file || !*database_file)
      database_file = DEFAULT_LANGID_DATABASE ;
   if (language_identifier->numLanguages() > 0)
      {
      if (do_dump_trie)
	 {
	 fprintf(stdout,"=======================\n") ;
	 language_identifier->dump(stdout,verbose) ;
	 }
      unsigned num_languages
	 = count_languages(language_identifier,compare_langcode) ;
      unsigned num_pairs
	 = count_languages(language_identifier,compare_codepair) ;
      cout << "Database contains " << language_identifier->numLanguages()
	   << " models, " << num_languages
	   << " distinct language codes,\n\tand " << num_pairs
	   << " language/encoding pairs" << endl ;
      cout << "Saving database" << endl ;
      return language_identifier->write(database_file) ;
      }
   return false ;
}

//----------------------------------------------------------------------

static bool dump_ngrams(const NybbleTrieNode *node, const uint8_t *key,
			unsigned keylen, void *user_data)
{
   FILE *fp = (FILE*)user_data ;
   if (node->leaf())
      {
      uint32_t freq = node->frequency() ;
      if (node->isStopgram() && freq > 0)
	 fputc('-',fp) ;
      fprintf(fp,"%u\t",freq) ;
      for (size_t i = 0 ; i < keylen ; i++)
	 {
	 print_quoted_char(fp,key[i]) ;
	 }
      fprintf(fp,"\n") ;
      }
   return true ;
}

//----------------------------------------------------------------------

// this global variable makes dump_vocabulary() non-reentrant
static uint64_t dump_total_bytes ;

static bool dump_ngrams_scaled(const NybbleTrieNode *node,
			       const uint8_t *key,
			       unsigned keylen, void *user_data)
{
   FILE *fp = (FILE*)user_data ;
   if (node->leaf())
      {
      uint32_t freq = node->frequency() ;
      if (node->isStopgram() && freq > 0)
	 fputc('-',fp) ;
#if 0
      uint32_t mantissa ;
      uint32_t exponent ;
      PackedTrieFreq::quantize(freq,mantissa,exponent) ;
      mantissa >>= PACKED_TRIE_FREQ_MAN_SHIFT ;
      exponent *= PTRIE_EXPONENT_SCALE ;
      exponent = PACKED_TRIE_FREQ_MAN_SHIFT - exponent ;
      if (exponent > 0 && mantissa > 0)
	 fprintf(fp,"%u%c\t",mantissa,exponent + 'a' - 1) ;
      else
	 fprintf(fp,"%u\t",mantissa) ;
#else
      double unscaled = (unscale_frequency(freq,smoothing_power) * dump_total_bytes / 100.0) + 0.99 ;
      fprintf(fp,"%u\t",(unsigned)unscaled) ;
#endif
      for (size_t i = 0 ; i < keylen ; i++)
	 {
	 print_quoted_char(fp,key[i]) ;
	 }
      fprintf(fp,"\n") ;
      }
   return true ;
}

//----------------------------------------------------------------------

static void dump_vocabulary(const NybbleTrie *ngrams, bool scaled,
			    const char *vocab_file, unsigned max_length,
			    uint64_t total_bytes, const LanguageID &opts)
{
   FILE *fp = fopen(vocab_file,"w") ;
   if (fp)
      {
      if (total_bytes)
	 fprintf(fp,"TotalCount: %llu\n", (unsigned long long)total_bytes) ;
      fprintf(fp,"Lang: %s",opts.language()) ;
      if (opts.friendlyName() != opts.language())
	 fprintf(fp,"=%s",opts.friendlyName()) ;
      fprintf(fp,"\nScript: %s\nRegion: %s\nEncoding: %s\nSource: %s\n",
	      opts.script(),opts.region(),opts.encoding(),opts.source()) ;
      if (alignment > 1)
	 fprintf(fp,"Alignment: %d\n",alignment) ;
      if (discount_factor > 1.0)
	 fprintf(fp,"Discount: %g\n",discount_factor) ;
      if (ngrams->ignoringWhiteSpace())
	 fprintf(fp,"IgnoreBlanks: yes\n") ;
      if (opts.coverageFactor() > 0.0 && opts.coverageFactor() != 1.0)
	 fprintf(fp,"Coverage: %g\n",opts.coverageFactor()) ;
      if (opts.countedCoverage() > 0.0 && opts.countedCoverage() != 1.0)
	 fprintf(fp,"WeightedCoverage: %g\n",opts.countedCoverage()) ;
      if (opts.freqCoverage() > 0.0)
	 fprintf(fp,"FreqCoverage: %g\n",opts.freqCoverage()) ;
      if (opts.matchFactor() > 0.0)
	 fprintf(fp,"MatchFactor: %g\n",opts.matchFactor()) ;
//      if (scaled)
//	 fprintf(fp,"Scaled: yes\n") ;
      uint8_t keybuf[max_length+1] ;
      if (scaled)
	 {
	 dump_total_bytes = total_bytes ;
	 (void)ngrams->enumerate(keybuf,max_length,dump_ngrams_scaled,fp) ;
	 }
      else
	 (void)ngrams->enumerate(keybuf,max_length,dump_ngrams,fp) ;
      fclose(fp) ;
      }
   else
      {
      cerr << "Unable to open '" << vocab_file << "' to write vocabulary"
	   << endl ;
      }
   return ;
}

//----------------------------------------------------------------------

static int UCS2_to_UTF8(unsigned long codepoint, char *buf)
{
   if (codepoint < 0x80)
      {
      // encode as single byte
      *buf = (char)codepoint ;
      return 1 ;
      }
   else if (codepoint > 0x10FFFF)
      return -1 ;
   if (codepoint < 0x800)
      {
      // encode in two bytes
      buf[0] = (unsigned char)(0xC0 | ((codepoint & 0x07C0) >> 6)) ;
      buf[1] = (unsigned char)(0x80 | (codepoint & 0x003F)) ;
      return 2 ;
      }
   else if (codepoint < 0x010000)
      {
      // encode as three bytes
      buf[0] = (unsigned char)(0xE0 | ((codepoint & 0xF000) >> 12)) ;
      buf[1] = (unsigned char)(0x80 | ((codepoint & 0x0FC0) >> 6)) ;
      buf[2] = (unsigned char)(0x80 | (codepoint & 0x003F)) ;
      return 3 ;
      }
   else
      {
      // encode as four bytes
      buf[0] = (unsigned char)(0xF0 | ((codepoint & 0x1C0000) >> 18)) ;
      buf[1] = (unsigned char)(0x80 | ((codepoint & 0x03F000) >> 12)) ;
      buf[2] = (unsigned char)(0x80 | ((codepoint & 0x000FC0) >> 6)) ;
      buf[3] = (unsigned char)(0x80 | (codepoint & 0x00003F)) ;
      return 4 ;
      }
}


//----------------------------------------------------------------------

static const char *add_UTF8_range(const char *range_spec,
				  NybbleTrie *ngrams,
				  uint64_t &total_bytes)
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
	    char utf8[6] ;
	    int bytes = UCS2_to_UTF8(i,utf8) ;
	    if (bytes > 0)
	       {
               ngrams->insert((uint8_t*)utf8,bytes,1,false) ;
	       total_bytes += (bytes * FAKED_NGRAM_DISCOUNT) ;
	       }
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

static void add_UTF8_codepoints(NybbleTrie *ngrams,
				const char *cp_list, uint64_t &total_bytes)
{
   while (*cp_list)
      {
      cp_list = add_UTF8_range(cp_list,ngrams,total_bytes) ;
      }
   return ;
}

//----------------------------------------------------------------------

static bool collect_language_ngrams_packed(const PackedTrieNode *node,
					   const uint8_t *key,
					   unsigned keylen, void *user_data)
{
   StopGramInfo *stop_gram_info = (StopGramInfo*)user_data ;
   if (stop_gram_info && node && node->leaf())
      {
      const PackedTrieFreq *freq
	 = node->frequencies(stop_gram_info->freqBaseAddress()) ;
      // the following is the smallest value which can be represented by
      //   the bitfields in PackedTrieFreq; anything less than that will
      //   round to zero unless smoothed
      unsigned max_exp = PTRIE_EXPONENT_SCALE * (PACKED_TRIE_FREQ_EXPONENT >> PACKED_TRIE_FREQ_EXP_SHIFT) ;
      double minweight = PACKED_TRIE_FREQ_MANTISSA >> max_exp ;
      // since extra stopgrams have a cost both in storage and runtime,
      //   we want to omit any that have little possible impact on the
      //   overall score, so bump up the above absolute minimum threshold
      minweight *= STOPGRAM_CUTOFF ;
      for ( ; ; freq++)
	 {
	 unsigned langid = freq->languageID() ;
	 if (stop_gram_info->activeLanguage() == langid)
	    {
	    uint32_t wt = freq->scaledScore() ;
	    stop_gram_info->currLangTrie()->insert(key,keylen,wt,false) ;
	    }
	 if (stop_gram_info->selected(langid) &&
	     !freq->isStopgram() && freq->percentage() > 0.0)
	    {
	    double wt = stop_gram_info->weight(langid) * freq->scaledScore() ;
	    uint32_t weight = (uint32_t)(wt + 0.5) ;
	    if (weight >= minweight)
	       {
	       // if the weight is large enough to be bothered with,
	       //   add the ngram to the presence trie and its computed
	       //   weight to the weight trie
	       stop_gram_info->trie()->insert(key,keylen,0,false) ;
	       stop_gram_info->weightTrie()->insertMax(key,keylen,weight,
						       false) ;
	       }
	    }
	 if (freq->isLast())
	    break ;
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

static NybbleTrie *load_stop_grams_similarity(unsigned langid,
					      LanguageScores *weights,
					      const char *thresh,
					      unsigned maxkey,
					      PackedMultiTrie *ptrie,
					      const PackedTrieFreq *freq_base,
					      NybbleTrie *&curr_ngrams,
					      NybbleTrie *&ngram_weights)
{
   curr_ngrams = 0 ;
   ngram_weights = 0 ;
   if (!weights)
      {
      cout << "Unable to compute cross-language similarities, will not "
	      "compute stop-grams." << endl ;
      return 0;
      }
   char *endptr = 0 ;
   double threshold = strtod(thresh,&endptr) ;
   if (threshold <= 0.0 || threshold > 1.0)
      threshold = DEFAULT_SIMILARITY_THRESHOLD ;
   const LanguageID *curr = language_identifier->languageInfo(langid) ;
   FrBitVector selected(weights->numLanguages()) ;
   bool above_threshold = false ;
   for (size_t langnum = 0 ; langnum < weights->numLanguages() ; langnum++)
      {
      if (langnum == langid || weights->score(langnum) < threshold)
	 continue ;
      const LanguageID *other = language_identifier->languageInfo(langnum) ;
      if (other)
	 {
	 // check that the other model isn't the same language,
	 //   region, AND encoding
	 if (curr->language() && other->language() &&
	     strcmp(curr->language(),other->language()) == 0 &&
	     curr->region() && other->region() &&
	     strcmp(curr->region(),other->region()) == 0 &&
	     curr->encoding() && other->encoding() &&
	     strcmp(curr->encoding(),other->encoding()) == 0)
	    continue ;
	 }
      above_threshold = true ;
      selected.setBit(langnum) ;
      if (/*verbose &&*/ other)
	 {
	 cout << "  similarity to "<< other->language() << "_"
	      << other->region() << "-" << other->encoding() << " is "
	      << weights->score(langnum) << endl ;
	 }
      }
   NybbleTrie *stop_grams = new NybbleTrie ;
above_threshold=true;
   if (above_threshold)
      {
      // further discount the now-selected other languages by the absolute
      //   amount of training data in the primary language (since less data
      //   means a greater chance that the n-gram is not seen purely due
      //   to data sparsity)
      uint64_t train = curr->trainingBytes() ;
      if (train < TRAINSIZE_FULL_WEIGHT)
	 {
	 train = (train > TRAINSIZE_NO_WEIGHT) ? train - TRAINSIZE_NO_WEIGHT : 0 ;
	 double scalefactor = (TRAINSIZE_FULL_WEIGHT - TRAINSIZE_NO_WEIGHT) ;
	 double scale = ::pow(train / scalefactor,0.7) ;
	 weights->scaleScores(scale) ;
	 }
      // because the values stored in the tries have already been
      //   adjusted to smooth them, we need to apply the same
      //   adjustment to the inter-model weights to avoid overly
      //   reducing the weights of stopgrams
      for (size_t i = 0 ; i < weights->numLanguages() ; i++)
	 {
	 if (selected.getBit(i))
	    {
	    double sc = weights->score(i) ;
	    sc = ::pow(sc,smoothing_power) ;
#if 0 //!!!
	    if (i != langid)
	       {
	       // adjust the strength of the stop-grams for each model
	       //   pair by the coverage fractions of the two models
	       double cover = curr->coverageFactor() ;
	       if (cover > 0.0)
		  sc /= cover ;
	       // assume that the coverages are independent of each
	       //   other, which lets us simply multiple the two
	       //   coverage fractions
	       const LanguageID *other = language_identifier->languageInfo(i) ;
	       cover = other->coverageFactor() ;
cerr<<"adj="<<cover<<endl;
	       if (cover > 0.0)
		  sc /= cover ;
	       }
#endif
	    weights->setScore(i,sc) ;
	    }
	 }
      uint8_t key[1000] ;
      if (maxkey > sizeof(key))
      maxkey = sizeof(key) ;
      curr_ngrams = new NybbleTrie ;
      ngram_weights = new NybbleTrie ;
      StopGramInfo stop_gram_info(stop_grams,curr_ngrams,ngram_weights,
				  freq_base,weights,&selected,langid) ;
      ptrie->enumerate(key,maxkey,collect_language_ngrams_packed,
		       &stop_gram_info) ;
      }
   delete weights ;
   return stop_grams ;
}

//----------------------------------------------------------------------

static NybbleTrie *load_stop_grams(const LanguageID *lang_info,
				   const char *languages,
				   NybbleTrie *&curr_ngrams,
				   NybbleTrie *&ngram_weights,
				   uint64_t &training_bytes)
{
   curr_ngrams = 0 ;
   ngram_weights = 0 ;
   training_bytes = 0 ;
   if (!languages)
      return 0 ;
   PackedMultiTrie *ptrie = language_identifier->trie() ;
   if (!ptrie)
      {
      return 0 ;
      }
   unsigned langid = language_identifier->languageNumber(lang_info) ;
   training_bytes = language_identifier->trainingBytes(langid) ;
   unsigned maxkey = ptrie->longestKey() ;
   const PackedTrieFreq *freq_base = ptrie->frequencyBaseAddress() ;
   cout << "Computing similarities relative to "
	<< lang_info->language() << "_" << lang_info->region()
	<< "-" << lang_info->encoding() << endl ;
   LanguageScores *weights = language_identifier->similarity(langid) ;
   if (languages && *languages == '@')
      return load_stop_grams_similarity(langid, weights, languages + 1,
					maxkey,ptrie,freq_base,curr_ngrams,
					ngram_weights) ;
   NybbleTrie *stop_grams = 0 ;
   char *descriptions = FrDupString(languages) ;
   char *desc = descriptions ;
   while (desc && *desc)
      {
      char *desc_end = strchr(desc,',') ;
      if (desc_end)
	 *desc_end++ = '\0' ;
      else
	 desc_end = strchr(desc,'\0') ;
      unsigned langnum = language_identifier->languageNumber(desc) ;
      if (langnum == (unsigned)~0)
	 {
	 cerr << "Warning: no match for language descriptor " << desc << endl ;
	 }
      else
	 {
	 if (!stop_grams)
	    stop_grams = new NybbleTrie ;
	 uint8_t key[1000] ;
	 if (maxkey > sizeof(key))
	    maxkey = sizeof(key) ;
	 FrBitVector selected(language_identifier->numLanguages()) ;
	 selected.setBit(langnum) ;
	 delete curr_ngrams ;
	 curr_ngrams = new NybbleTrie ;
	 delete ngram_weights ;
	 ngram_weights = new NybbleTrie ;
	 StopGramInfo stop_gram_info(stop_grams,curr_ngrams,ngram_weights,
				     freq_base,weights,&selected,langid) ;
	 ptrie->enumerate(key,maxkey,collect_language_ngrams_packed,
			  &stop_gram_info) ;
	 }
      desc = desc_end ;
      }
   delete weights ;
   FrFree(descriptions) ;
   return stop_grams ;
}

//----------------------------------------------------------------------

static void coverage_matches(const uint8_t *buf, unsigned buflen, unsigned *cover,
			     double *freqtotal, double &matchcount,
			     const NybbleTrie *ngrams, bool scaled)
{
   NybbleTriePointer ptr(ngrams) ;
   for (size_t i = 0 ; i < buflen ; i++)
      {
      if (!ptr.extendKey(buf[i]))
	 return ;
      const NybbleTrieNode *n = ptr.node() ;
      if (n && n->leaf())
	 {
	 matchcount += 1.0 ;
//	 matchcount += ::pow(i,0.75) ;
	 for (size_t j = 0 ; j <= i ; j++)
	    {
	    cover[j]++ ;
	    double freq ;
	    if (scaled)
	       freq = unscale_frequency(n->frequency(),smoothing_power) ;
	    else
	       freq = (n->frequency() / (double)TRIE_SCALE_FACTOR) ;
	    freqtotal[j] += freq ;
	    }
	 }
      }
   return ;
}

//----------------------------------------------------------------------

static void compute_coverage(FILE *fp, size_t &overall_cover, size_t &counted_cover,
			     double &freq_cover, double &match_count, uint64_t &train_bytes,
			     const NybbleTrie *ngrams, bool ignore_whitespace,
			     bool scaled)
{
   if (!ngrams)
      return ;
   unsigned maxlen = ngrams->longestKey() ;
   if (maxlen > ABSOLUTE_MAX_LENGTH)
      maxlen = ABSOLUTE_MAX_LENGTH ;
   uint8_t buf[maxlen+1] ;
   unsigned cover[maxlen+1] ;
   double freqtotal[maxlen+1] ;
   unsigned buflen = 0 ;
   // initialize the statistics and prime the buffer
   for (size_t i = 0 ; i < maxlen ; i++)
      {
      if (!more_data(fp))
	 {
	 break ;
	 }
      buflen++ ;
      buf[i] = get_byte(fp,train_bytes,ignore_whitespace) ;
      cover[i] = 0 ;
      freqtotal[i] = 0.0 ;
      }
   bool hit_eod = false ;
   match_count = 0 ;
   while (buflen > 0)
      {
      // check matches against current buffer
      coverage_matches(buf,buflen,cover,freqtotal,match_count,ngrams,scaled) ;
      // update statistics
      overall_cover += (cover[0] != 0) ;
      counted_cover += cover[0] ;
      freq_cover += freqtotal[0] ;
      // update buffer
      memmove(buf,buf+1,buflen-1) ;
      if (!hit_eod && more_data(fp))
	 {
	 int b = get_byte(fp,train_bytes,ignore_whitespace) ;
	 if (b == EOF)
	    {
	    hit_eod = true ;
	    break ;
	    }
	 buf[buflen-1] = (uint8_t)b ;
	 }
      else
	 {
	 buflen-- ;
	 if (buflen == 0)
	    break ;
	 }
      // update statistics buffers
      memmove(cover,cover+1,buflen*sizeof(cover[0])) ;
      memmove(freqtotal,freqtotal+1,buflen*sizeof(freqtotal[0])) ;
      cover[buflen-1] = 0 ;
      freqtotal[buflen-1] = 0.0 ;
      }
   return ;
}

//----------------------------------------------------------------------

static void compute_coverage(LanguageID &lang_info,
			     const char **filelist, unsigned num_files,
			     const NybbleTrie *ngrams, bool ignore_whitespace,
			     bool scaled)
{
   size_t overall_coverage = 0 ;	// percentage of training bytes covered by ANY ngram
   size_t counted_coverage = 0 ;	// coverage weighted by number of ngrams covering a byte
   double match_count = 0.0 ;		// number of matching ngrams against training data
   double freq_coverage = 0.0 ;		// coverage weighted by freq of ngrams covering a bytes
   uint64_t training_bytes = 0 ;
   if (ngrams && filelist)
      {
      for (size_t i = 0 ; i < num_files ; i++)
	 {
	 const char *filename = filelist[i] ;
	 if (filename && *filename)
	    {
	    bool piped ;
	    FILE *fp = open_input_file(filename,piped) ;
	    if (fp)
	       {
	       cout << "  Computing coverage of " << filename << endl ;
	       compute_coverage(fp,overall_coverage,counted_coverage,freq_coverage,
				match_count,training_bytes,ngrams,ignore_whitespace,scaled) ;
	       close_input_file(fp,piped) ;
	       }
	    }
	 }
      }
   if (training_bytes > 0)
      {
      if (verbose)
	 {
	 cout << "    Coverage fraction " << (overall_coverage / (double)training_bytes) << endl ;
	 }
      lang_info.setCoverageFactor(overall_coverage / (double)training_bytes) ;
      lang_info.setCountedCoverage(counted_coverage / (double)training_bytes) ;
      freq_coverage = ::sqrt(freq_coverage) ; // adjust for multiple-counting of high-freq ngrams
      lang_info.setFreqCoverage(freq_coverage) ;
      lang_info.setMatchFactor(match_count / training_bytes) ;
      }
   else
      {
      lang_info.setCoverageFactor(0.0) ;
      }
   return ;
}

//----------------------------------------------------------------------

static bool guess_script(LanguageID &lang_info)
{
   if (!lang_info.script()
       || strcasecmp(lang_info.script(),"UNKNOWN") == 0
       || strcmp(lang_info.script(),"") == 0)
      {
      // try to guess the script from the encoding
      const char *enc = lang_info.encoding() ;
      if (strcasecmp(enc,"iso-8859-6") == 0)
	 {
	 lang_info.setScript("Arabic") ;
	 }
      else if (strcasecmp(enc,"ArmSCII8") == 0)
	 {
	 lang_info.setScript("Armenian") ;
	 }
      else if (strncasecmp(enc,"KOI8-",5) == 0 ||
	  strcasecmp(enc,"KOI7") == 0 ||
	  strcasecmp(enc,"CP866") == 0 ||
	  strcasecmp(enc,"RUSCII") == 0 ||
	  strcasecmp(enc,"Windows-1251") == 0 ||
	  strcasecmp(enc,"iso-8859-5") == 0 ||
	  strcasecmp(enc,"Latin-5") == 0 ||
	  strcasecmp(enc,"MacCyrillic") == 0)
	 {
	 lang_info.setScript("Cyrillic") ;
	 }
      else if (strcasecmp(enc,"ISCII") == 0)
	 {
	 lang_info.setScript("Devanagari") ;
	 }
      else if (strcasecmp(enc,"iso-8859-7") == 0 ||
	       strcasecmp(enc,"cp737") == 0)
	 {
	 lang_info.setScript("Greek") ;
	 }
      else if (strcasecmp(enc,"GB2312") == 0 ||
	       strcasecmp(enc,"GB-2312") == 0 ||
	       strcasecmp(enc,"GB18030") == 0 ||
	       strcasecmp(enc,"GBK") == 0 ||
	       strcasecmp(enc,"Big5") == 0 ||
	       strcasecmp(enc,"EUC-CN") == 0 ||
	       strcasecmp(enc,"EUC-TW") == 0)
	 {
	 lang_info.setScript("Han") ;
	 }
      else if (strcasecmp(enc,"EUC-KR") == 0)
	 {
	 lang_info.setScript("Hangul") ;
	 }
      else if (strcasecmp(enc,"CP862") == 0 ||
	       strncasecmp(enc,"iso-8859-8",10) == 0)
	 {
	 lang_info.setScript("Hebrew") ;
	 }
      else if (strcasecmp(enc,"ShiftJIS") == 0 ||
	       strcasecmp(enc,"Shift-JIS") == 0 ||
	       strcasecmp(enc,"ISO-2022") == 0 ||
	       strcasecmp(enc,"EUC-JP") == 0 ||
	       strncasecmp(enc,"EUC-JIS",7) == 0)
	 {
	 lang_info.setScript("Kanji") ;
	 }
      else if (strcasecmp(enc,"TIS620") == 0 ||
	       strcasecmp(enc,"TSCII") == 0 ||
	       strcasecmp(enc,"iso-8859-11") == 0)
	 {
	 lang_info.setScript("Thai") ;
	 }
      else if (strcasecmp(enc,"VISCII") == 0)
	 {
	 lang_info.setScript("Vietnamese") ;
	 }
      else if (strcasecmp(enc,"ASCII") == 0 ||
	       strcasecmp(enc,"CP437") == 0 ||
	  strncasecmp(enc,"ASCII-16",8) == 0 ||
	  strncasecmp(enc,"iso-8859-",9) == 0 ||
	  strncasecmp(enc,"Latin",5) == 0)
	 {
	 lang_info.setScript("Latin") ;
	 }
      else if (!lang_info.script()
	       || strcmp(lang_info.script(),"") == 0)
	 {
	 lang_info.setScript("UNKNOWN") ;
	 return false ;
	 }
      else
	 return false ;
      return true ;	// successfully made a guess
      }
   return true ;	// already have a script assigned
}

//----------------------------------------------------------------------

static bool load_frequencies(FILE *fp, NybbleTrie *ngrams,
			     uint64_t &total_bytes, bool textcat_format,
			     LanguageID &opts,
			     BigramCounts *&bigrams, bool &scaled)
{
   scaled = false ;
   if (!fp)
      return false ;
   bool have_total_bytes = false ;
   bool first_line = true ;
   bool have_bigram_counts = false ;
   double codepoint_discount = 1.0 ;
   BigramCounts *crubadan_bigrams = 0 ;
   if (crubadan_format)
      crubadan_bigrams = new BigramCounts ;
   char buffer[16384] ;
   memset(buffer,'\0',sizeof(buffer)) ; // keep Valgrind happy
   bool have_script = false ;
   bool try_guessing_script = false ;
   opts.setCoverageFactor(1.0) ;
   while (!feof(fp))
      {
      if (!fgets(buffer,sizeof(buffer),fp))
	 break ;
      
      char *endptr ;
      if (textcat_format)
	 {
	 char *tab = strchr(buffer,'\t') ;
	 if (tab)
	    {
	    *tab = '\0' ;
	    size_t len = tab - buffer ;
	    tab++ ;
	    if (len > 0)
	       {
	       // we have a valid frequency-list entry, so add it to the
	       //   trie and update the global byte count
	       unsigned long count = strtoul(tab,&endptr,0) ;
	       ngrams->increment((uint8_t*)buffer,len,count) ;
	       total_bytes += (len * count * ASSUMED_NGRAM_DENSITY) ;
	       }
	    }
	 }
      else if (buffer[0] == '#' || buffer[1] == ';')
	 {
	 // comment line
	 continue ;
	 }
      else if (first_line && strncasecmp(buffer,"TotalCount:",11) == 0)
	 {
	 total_bytes = strtoul(buffer+11,0,0) ;
	 if (total_bytes > 0)
	    have_total_bytes = true ;
	 }
      else if (strncasecmp(buffer,"Lang:",5) == 0)
	 {
	 char *arg = FrTrimWhitespace(buffer + 5) ;
	 opts.setLanguage(arg) ;
	 }
      else if (strncasecmp(buffer,"Region:",7) == 0)
	 {
	 char *arg = FrTrimWhitespace(buffer + 7) ;
	 opts.setRegion(arg) ;
	 }
      else if (strncasecmp(buffer,"Encoding:",9) == 0)
	 {
	 char *arg = FrTrimWhitespace(buffer + 9) ;
	 opts.setEncoding(arg) ;
	 try_guessing_script = !have_script ;
	 }
      else if (strncasecmp(buffer,"Source:",7) == 0)
	 {
	 char *arg = FrTrimWhitespace(buffer + 7) ;
	 opts.setSource(arg) ;
	 }
      else if (strncasecmp(buffer,"Script:",7) == 0)
	 {
	 char *arg = FrTrimWhitespace(buffer + 7) ;
	 opts.setScript(arg) ;
	 have_script = true ;
	 try_guessing_script = false ;
	 }
      else if (strncasecmp(buffer,"Scaled:",7) == 0)
	 {
	 scaled = true ;
	 }
      else if (strncasecmp(buffer,"IgnoreBlanks:",13) == 0)
	 {
	 ngrams->ignoreWhiteSpace() ;
	 }
      else if (strncasecmp(buffer,"Alignment:",10) == 0)
	 {
	 char *arg = FrTrimWhitespace(buffer + 10) ;
	 opts.setAlignment(arg) ;
	 }
      else if (strncasecmp(buffer,"BigramCounts:",13) == 0)
	 {
	 have_bigram_counts = true ;
	 break ;
	 }
      else if (strncasecmp(buffer,"Discount:",9) == 0)
	 {
	 // permit discounting of probabilities to ensure that a very small
	 //   model doesn't overwhelm one built with adequate training data
	 codepoint_discount = atof(buffer+9) ;
	 if (codepoint_discount < 1.0)
	    codepoint_discount = 1.0 ;
	 }
      else if (strncasecmp(buffer,"Coverage:",9) == 0)
	 {
	 opts.setCoverageFactor(atof(buffer+9)) ;
	 }
      else if (strncasecmp(buffer,"WeightedCoverage:",17) == 0)
	 {
	 opts.setCountedCoverage(atof(buffer+17)) ;
	 }
      else if (strncasecmp(buffer,"FreqCoverage:",13) == 0)
	 {
	 opts.setFreqCoverage(atof(buffer+13)) ;
	 }
      else if (strncasecmp(buffer,"MatchFactor:",12) == 0)
	 {
	 opts.setMatchFactor(atof(buffer+12)) ;
	 }
      else if (strncasecmp(buffer,"UTF8:",5) == 0)
	 {
	 // add the UTF-8 representations of the listed codepoints to the
	 //  model, highly discounted
	 add_UTF8_codepoints(ngrams,buffer+5,total_bytes) ;
	 }
      else
	 {
	 if (try_guessing_script)
	    {
	    have_script = guess_script(opts) ;
	    try_guessing_script = false ;
	    }
	 endptr = 0 ;
	 char *bufptr = buffer ;
	 (void)FrSkipWhitespace(bufptr) ;
	 bool stopgram = false ;
	 if (*bufptr == '-')
	    {
	    stopgram = true ;
	    bufptr++ ;
	    }
	 unsigned long count = strtoul(bufptr,&endptr,0) ;
	 if (count == 0)
	    {
	    count = 1 ;
	    stopgram = true ;
	    }
	 if (endptr && endptr > buffer)
	    {
	    unsigned shift_count = 0 ;
	    if (*endptr >= 'a' && *endptr <= 'z' &&
		(endptr[1] == '\t' || endptr[1] == ' '))
	       {
	       if (!scaled)
		  {
		  cerr << "Warning: found scaled count in counts file that does not contain" << endl ;
		  cerr << "the Scaled: directive; enabling scaled counts for remainder." << endl ;
		  scaled = true ;
		  }
	       shift_count = *endptr++ - 'a' + 1 ;
	       }
	    while (*endptr == '\t' || *endptr == ' ')
	       endptr++ ;
	    char *key = endptr ;
	    char *keyptr = key ;
	    while (*endptr && *endptr != '\n' && *endptr != '\r')
	       {
	       if (*endptr == '\\')
		  {
		  // un-quote the next character
		  endptr++ ;
		  if (!*endptr)
		     break ;
		  switch (*endptr)
		     {
		     case '0':
			*keyptr++ = '\0' ;
			break ;
		     case 'n':
			*keyptr++ = '\n' ;
			break ;
		     case 'r':
			*keyptr++ = '\r' ;
			break ;
		     case 't':
			*keyptr++ = '\t' ;
			break ;
		     case 'f':
			*keyptr++ = '\f' ;
			break ;
		     default:
			*keyptr++ = *endptr ;
		     }
		  endptr++ ;
		  }
	       else
		  *keyptr++ = *endptr++ ;
	       }
	    size_t len = keyptr - key ;
	    if (len > 0)
	       {
	       if (crubadan_format)
		  {
		  if (*key == '<')
		     *key = ' ' ;
		  if (keyptr[-1] == '>')
		     keyptr[-1] = ' ' ;
		  // add in bigram counts from the current ngram
		  for (size_t i = 0 ; i+1 < len ; i++)
		     {
		     crubadan_bigrams->incr(key[i],key[i+1],count) ;
		     }
		  }
	       // we have a valid frequency-list entry, so add it to the
	       //   trie and update the global byte count
	       if (count > 1 || !crubadan_format)
		  {
		  if (scaled)
		     {
		     count <<= shift_count ;
		     ngrams->insert((uint8_t*)key,len,count,stopgram) ;
		     }
		  else
		     ngrams->increment((uint8_t*)key,len,count,stopgram) ;
		  if (crubadan_format && len > 3)
		     {
		     if (keyptr[-1] == ' ')
			len-- ;
		     ngrams->increment((uint8_t*)key,len,count) ;
		     if (len > 3 && *key == ' ')
			ngrams->increment((uint8_t*)(key+1),len-1,count) ;
		     }
		  }
	       if (!have_total_bytes)
		  total_bytes += (len * count / 4) ;
	       }
	    }
	 }
      first_line = false ;
      }
   if (have_bigram_counts)
      {
      delete crubadan_bigrams ;
      bigrams = new BigramCounts ;
      if (!bigrams->read(fp))
	 {
	 cerr << "Error reading bigram counts in vocabulary file" << endl ;
	 delete bigrams ;
	 bigrams = 0 ;
	 }
      }
   else
      {
      if (crubadan_bigrams)
	 crubadan_bigrams->scaleTotal(100) ;
      bigrams = crubadan_bigrams ;
      }
   total_bytes = (uint64_t)(total_bytes * codepoint_discount) ;
   return true ;
}

//----------------------------------------------------------------------

static bool load_frequencies(const char **filelist, unsigned num_files,
			     LanguageID &opts, bool textcat_format, bool no_save)
{
   cout << "Loading frequency list " ;
   if (textcat_format)
      cout << "(TextCat format)" << endl ;
   else if (crubadan_format)
      cout << "(Crubadan format)" << endl ;
   else
      cout << "(MkLangID format)" << endl ;
   NybbleTrie *ngrams = new NybbleTrie ;
   if (!ngrams)
      {
      FrNoMemory("while loading frequency lists") ;
      return false ;
      }
   uint64_t total_bytes = 0 ;
   BigramCounts *bigrams = 0 ;
   bool scaled = false ;
   for (size_t i = 0 ; i < num_files ; i++)
      {
      const char *filename = filelist[i] ;
      if (filename && *filename)
	 {
	 bool piped ;
	 FILE *fp = FrOpenMaybeCompressedInfile(filename,piped) ;
	 if (fp)
	    {
	    cout << "  Reading " << filename << endl ;
	    load_frequencies(fp,ngrams,total_bytes,textcat_format,opts,
			     bigrams,scaled) ;
	    FrCloseMaybeCompressedInfile(fp,piped) ;
	    if (textcat_format || num_files > 1)
	       {
	       delete bigrams ;
	       bigrams = 0 ;
	       }
	    }
	 }
      }
   merge_bigrams(ngrams,bigrams,scaled,total_bytes) ;
   delete bigrams ;
   if (ngrams->size() > 0)
      {
      // output the merged vocabulary list as text if requested
      minimum_length = 1 ;
      if (vocabulary_file)
	 dump_vocabulary(ngrams,scaled,vocabulary_file,1000,total_bytes,
			 opts) ;
      // now that we have read in the n-grams, augment the database with that
      //   list for the indicated language and encoding
      if (no_save)
	 {
	 if (!vocabulary_file)
	    cerr << "*** N-grams WERE NOT SAVED (read-only database) ***" << endl ;
	 }
      else
	 {
	 cout << "Updating database" << endl ;
	 if (!scaled)
	    ngrams->scaleFrequencies(total_bytes,smoothing_power,log_smoothing_power) ;
	 add_ngrams(ngrams,total_bytes,opts,filelist[0]) ;
	 }
      delete ngrams ;
      return true ;
      }
   else
      {
      delete ngrams ;
      return false ;
      }
}

//----------------------------------------------------------------------

// this global makes merge_ngram non-reentrant
static const PackedTrieFreq *base_frequency = 0 ;
static unsigned *model_sizes = 0 ;

static bool merge_ngrams(const PackedTrieNode *node, const uint8_t *key,
			 unsigned keylen, void *user_data)
{
   NybbleTrie **merged = (NybbleTrie**)user_data ;
   const PackedTrieFreq *freqlist = node->frequencies(base_frequency) ;
   for ( ; freqlist ; freqlist = freqlist->next())
      {
      if (!freqlist->isStopgram())
	 {
	 unsigned id = freqlist->languageID() ;
	 if (model_sizes)
	    model_sizes[id]++ ;
	 if (merged[id])
	    {
	    uint32_t freq = freqlist->scaledScore() ;
	    (void)merged[id]->insertMax(key,keylen,freq,false) ;
	    }
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

static NybbleTrie *find_encoding(const char *enc_name,
				 NybbleTrie **&encodings,
				 LanguageID **&enc_info,
				 unsigned &num_encs, unsigned &encs_alloc)
{
   if (!enc_name || !*enc_name)
      return 0 ;
   for (unsigned id = 0 ; id < num_encs ; id++)
      {
      if (enc_info[id] && enc_info[id]->encoding() &&
	  strcmp(enc_info[id]->encoding(),enc_name) == 0)
	 {
	 return encodings[id] ;
	 }
      }
   // if we get to this point, the specified encoding has not yet been seen
   //   so allocate a new trie and languageID record for the encoding
   if (num_encs >= encs_alloc)
      {
      unsigned new_alloc = 2 * encs_alloc ;
      NybbleTrie **new_encs = FrNewR(NybbleTrie*,encodings,new_alloc) ;
      LanguageID **new_info = FrNewR(LanguageID*,enc_info,new_alloc) ;
      if (new_encs)
	 encodings = new_encs ;
      if (new_info)
	 enc_info = new_info ;
      if (new_encs && new_info)
	 encs_alloc = new_alloc ;
      }
   if (num_encs < encs_alloc)
      {
      encodings[num_encs] = new NybbleTrie ;
      enc_info[num_encs] = new LanguageID("CLUS=Clustered","XX",enc_name,"merged") ;
      return encodings[num_encs++] ;
      }
   else
      return 0 ;
}

//----------------------------------------------------------------------

static bool cluster_models_by_charset(LanguageIdentifier *clusterdb,
				      const char *cluster_dbfile)
{
   unsigned num_encs = 0 ;
   unsigned encs_alloc = 50 ;
   NybbleTrie **encodings = FrNewC(NybbleTrie*,encs_alloc) ;
   LanguageID **enc_info = FrNewC(LanguageID*,encs_alloc) ;
   // make a mapping from language ID to per-encoding merged models
   unsigned numlangs = language_identifier->numLanguages() ;
   NybbleTrie **merged = FrNewC(NybbleTrie*,numlangs) ;
   for (unsigned langid = 0 ; langid < numlangs ; langid++)
      {
      // get the character encoding for the current model and find the
      //   merged trie for that encoding
      const char *enc_name = language_identifier->languageEncoding(langid) ;
      merged[langid]
	 = find_encoding(enc_name,encodings,enc_info,num_encs,encs_alloc) ;
      if (!merged[langid])
	 {
	 FrNoMemory("while merging language models") ;
	 break ;
	 }
      }
   // iterate over all of the ngrams in the database, merging each frequency
   //   record into the appropriate per-encoding model
   const PackedMultiTrie *ptrie = language_identifier->packedTrie() ;
   base_frequency = ptrie->frequencyBaseAddress() ;
   model_sizes = FrNewC(unsigned,numlangs) ;
   unsigned maxkey = ptrie->longestKey() ;
   uint8_t keybuf[maxkey] ;
   ptrie->enumerate(keybuf,maxkey,merge_ngrams,merged) ;
   base_frequency = 0 ;
   // figure out the maximum size of an individual model for each encoding
   unsigned max_sizes[num_encs] ;
   memset(max_sizes,'\0',sizeof(max_sizes)) ;
   for (size_t i = 0 ; i < num_encs ; i++)
      {
      for (unsigned langid = 0 ; langid < numlangs ; langid++)
	 {
	 if (merged[langid] == encodings[i] && model_sizes[langid] > max_sizes[i])
	    max_sizes[i] = model_sizes[langid] ;
	 }
      }
   // collect all of the merged models into the new language database
   LanguageIdentifier *ident = language_identifier ;
   (void)clusterdb->unpackedTrie() ; // ensure that we are unpacked
   language_identifier = clusterdb ;
   for (unsigned i = 0 ; i < num_encs ; i++)
      {
      bool have_max_length = true ;
      cerr << "adding encoding " << i << "  " << enc_info[i]->encoding() << endl;
      NybbleTrie *clustered = restrict_ngrams(encodings[i],2*max_sizes[i],
					      1,maxkey,1,have_max_length) ;
      delete encodings[i] ;
      uint64_t total_bytes = 1 ; //FIXME!!!
      add_ngrams(clustered,total_bytes,*(enc_info[i]),"???") ;
      delete clustered ;
      delete enc_info[i] ;
      }
   save_database(cluster_dbfile) ;
   language_identifier = ident ;
   FrFree(model_sizes) ; model_sizes = 0 ;
   FrFree(encodings) ;
   FrFree(enc_info) ;
   return true ;
}

//----------------------------------------------------------------------

static bool cluster_models(const char *cluster_db_name, double cluster_thresh)
{
   if (cluster_thresh < 0.0 || cluster_thresh > 1.0)
      return false ;
   LanguageIdentifier *clusterdb = new LanguageIdentifier(cluster_db_name) ;
   if (!clusterdb)
      {
      cerr << "Unable to create clustered language database " 
	   << cluster_db_name << endl ;
      return false ;
      }
   if (clusterdb->numLanguages() > 0)
      {
      cerr << "Non-empty language database specified: "
	   << cluster_db_name << endl ;
      return false ;
      }
   if (cluster_thresh == 0.0)
      {
      // cluster all models with the same character set together
      return cluster_models_by_charset(clusterdb,cluster_db_name) ;
      }
   else
      {
//FIXME
      cerr << "clustering thresholds other than 0.0 not implemented yet."
	   << endl ;
      return false ;
      }
}

//----------------------------------------------------------------------

static bool compute_ngrams(const char **filelist, unsigned num_files,
			   NybbleTrie *&ngrams,
			   bool skip_newlines, bool omit_bigrams,
			   bool ignore_whitespace, uint64_t &total_bytes,
			   bool aligned)
{
   // accumulate the most common n-grams (for n >= 3) by
   //   1. counting all trigrams
   //   2. filtering down to the K most frequent
   //   3. counting all n-grams (n > 3) which have the trigrams from step
   //         2 as a prefix
   //   4. filtering down the result of step 3 to the K most frequent overall
   // since we get bigrams counts essentially for free after step 1, also
   // add in the complete set of nonzero bigram counts while we're at it
   TrigramCounts *counts = new TrigramCounts ;
   ngrams = 0 ;
   if (!counts)
      return false ;
   ngrams = new NybbleTrie ;
   if (!ngrams)
      {
      delete counts ;
      return false ;
      }
   BigramCounts *bi_counts = 0 ;
   BigramCounts **bigram_ptr = omit_bigrams ? 0 : &bi_counts ;
   total_bytes = count_trigrams(filelist,num_files,*counts,
				skip_newlines,ignore_whitespace,
				aligned,bigram_ptr) ;
   unsigned top_K = set_oversampling(topK,ABSOLUTE_MIN_LENGTH,minimum_length,
				     aligned) ;
   counts->filter(top_K,maximum_length,verbose) ;
   ngrams->ignoreWhiteSpace(ignore_whitespace) ;
   if (counts->enumerate(*ngrams) && ngrams->longestKey() > 0)
      {
      delete counts ;
      counts = 0 ;
      bool small_data = (total_bytes * top_K) < 1E11 ;
      // start with five-grams, and continue increasing length until we
      //   no longer have any maximal-length ngrams included in the top K
      bool have_max_length = false ;
      // if we're processing virtual 16-bit units, extend the length
      //   by multiples of 16 bits
      unsigned expansion = (aligned || bigram_extension != BigramExt_None) ? 2 : 1 ;
      unsigned min_length = 4 ; // we've already counted up to 3-grams
      unsigned max_length = 4 + (expansion * (small_data ? 2 : 1)) ;
      if (max_length > maximum_length)
	  max_length = maximum_length ;
      do {
         NybbleTrie *new_ngrams
	    = count_ngrams(filelist, num_files, ngrams, min_length,
			   max_length, have_max_length, skip_newlines,
			   ignore_whitespace, aligned) ;
	 if (new_ngrams)
	    {
	    delete ngrams ;
	    ngrams = new_ngrams ;
	    }
	 else
	    have_max_length = false ;
	 // update lengths for next iteration
	 min_length = max_length + 1 ;
	 unsigned increment
	    = expansion + expansion*(max_length-ABSOLUTE_MIN_LENGTH + 1) / 2 ;
	 if (increment > expansion * MAX_INCREMENT)
	    increment = expansion * MAX_INCREMENT ;
	 if (small_data)
	    increment *= 2 ;
	 max_length += increment ;
	 if (max_length > maximum_length)
	    max_length = maximum_length ;
         } while (have_max_length && min_length <= maximum_length) ;
      }
   if (!omit_bigrams)
      {
      merge_bigrams(ngrams,bi_counts,false,total_bytes) ;
      }
   delete bi_counts ;
   delete counts ;
   return true ;
}

//----------------------------------------------------------------------

static bool process_files(const char **filelist, unsigned num_files,
			  const LanguageID &base_opts, NybbleTrie *curr_ngrams,
			  uint64_t training_bytes,
			  bool skip_newlines, bool omit_bigrams,
			  bool ignore_whitespace, NybbleTrie *stop_grams,
			  const NybbleTrie *ngram_weights, bool no_save,
			  bool /*check_script FIXME*/)
{
   LanguageID opts(base_opts) ;
   NybbleTrie *ngrams ;
   uint64_t total_bytes = 0 ;
   bool scaled = false ;
   if (curr_ngrams && curr_ngrams->size() > 0)
      {
      cout << "Using baseline n-gram model from language database" << endl ;
      ngrams = curr_ngrams ;
      scaled = true ;
      total_bytes = training_bytes ;
      }
   else if (!compute_ngrams(filelist,num_files,ngrams,skip_newlines,
			    omit_bigrams,ignore_whitespace,total_bytes,
			    opts.alignment() > 1))
      return false ;
   compute_coverage(opts,filelist,num_files,ngrams,ignore_whitespace,scaled) ;
   add_stop_grams(filelist,num_files,ngrams,stop_grams,ngram_weights,
		  scaled,total_bytes,ignore_whitespace) ;
   // output the vocabulary list as text if requested
   if (vocabulary_file)
      {
      unsigned max_length = ngrams->longestKey() ;
      dump_vocabulary(ngrams,scaled,vocabulary_file,max_length,
		      total_bytes,opts);
      }
   // now that we have the top K n-grams, augment the database with that
   //   list for the indicated language and encoding
   if (!no_save)
      {
      if (!scaled)
	 ngrams->scaleFrequencies(total_bytes,smoothing_power,log_smoothing_power) ;
      add_ngrams(ngrams,total_bytes,opts,filelist[0]) ;
      }
   else if (!vocabulary_file)
      {
      cerr << "*** N-grams WERE NOT SAVED (read-only database) ***" << endl ;
      }
   delete ngrams ;
   return true ;
}

/************************************************************************/
/*	Main Program							*/
/************************************************************************/

static void parse_bigram_extension(const char *arg)
{
   if (arg && *arg)
      {
      if (*arg == 'b' || *arg == 'B')
	 bigram_extension = BigramExt_ASCIIBigEndian ;
      else if (*arg == 'l' || *arg == 'L')
	 bigram_extension = BigramExt_ASCIILittleEndian ;
      else if (*arg == '-' || *arg == 'n' || *arg == 'N')
	 bigram_extension = BigramExt_None ;
      else
	 {
	 cerr << "Invalid value for -2 flag; bigram extension disabled."
	      << endl ;
	 bigram_extension = BigramExt_None ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

static void parse_UTF8_extension(const char *arg)
{
   if (arg && *arg)
      {
      if (*arg == 'b' || *arg == 'B')
	 bigram_extension = BigramExt_UTF8BigEndian ;
      else if (*arg == 'l' || *arg == 'L')
	 bigram_extension = BigramExt_UTF8LittleEndian ;
      else if (*arg == '-' || *arg == 'n' || *arg == 'N')
	 bigram_extension = BigramExt_None ;
      else
	 {
	 cerr << "Invalid value for -8 flag; UTF8-to-UTF16 conversion disabled."
	      << endl ;
	 bigram_extension = BigramExt_None ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

static void parse_clustering(const char *arg, double &threshold,
			     const char *&dbfile)
{
   if (arg && *arg)
      {
      char *endptr = 0 ;
      double val = strtod(arg,&endptr) ;
      if (val < 0.0)
	 {
	 val = 0.0 ;
	 cerr << "-C threshold adjusted to 0.0" << endl ;
	 }
      else if (val > 1.0)
	 {
	 val = 1.0 ;
	 cerr << "-C threshold adjusted to 1.0" << endl ;
	 }
      if (endptr && *endptr == ',')
	 {
	 dbfile = endptr + 1 ;
	 threshold = val ;
	 }
      else
	 {
	 cerr << "-C flag missing filename" << endl ;
	 dbfile = 0 ;
	 threshold = -1.0 ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

static void parse_byte_limit(const char *spec)
{
   if (spec)
      {
      if (spec[0] == '@')
	 {
	 subsample_input = true ;
	 spec++ ;
	 }
      byte_limit = atol(spec) ;
      }
   return ;
}

//----------------------------------------------------------------------

static void parse_smoothing_power(const char *spec)
{
   if (spec && *spec)
      {
      double smooth = atof(spec) ;
      // negative smoothing is now logarithmic rather than exponentiation
      if (smooth < 0.0)
	 {
	 // since we're using logs, it makes sense for the user-provided
	 //   power to be a log as well.
	 smoothing_power = -::pow(10.0,-smooth) ;
	 log_smoothing_power = scaling_log_power(smoothing_power) ;
	 }
      else
	 {
	 if (smooth > 5.0)
	    smooth = 5.0 ;
	 smoothing_power = smooth ;
	 log_smoothing_power = 1.0 ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

static void parse_translit(const char *spec, char *&from, char *&to)
{
   from = 0 ;
   to = 0 ;
   if (spec)
      {
      from = FrDupString(spec) ;
      char *comma = strchr(from,',') ;
      if (comma)
	 {
	 if (comma[1])
	    to = FrDupString(comma+1) ;
	 if (comma != from)
	    *comma = '\0' ;
	 else
	    {
	    cerr << "You may not omit the FROM encoding for -T" << endl ;
	    FrFree(from) ;
	    from = 0 ;
	    }
	 }
      }
   return;
}

//----------------------------------------------------------------------

static bool process_argument_group(int &argc, const char **&argv,
				   LanguageID &lang_info, bool no_save,
				   const char *argv0)
{
   // reset any options which must be specified separately for each file group
   vocabulary_file = 0 ;
   bool frequency_list = false ;
   bool frequency_textcat = false ;
   bool skip_newlines = false ;
   bool omit_bigrams = false ;
   bool end_of_args = false ;
   bool ignore_whitespace = false ;
   const char *related_langs = 0 ;
   const char *cluster_db = 0 ;
   double cluster_thresh = -1.0 ;  // never cluster
   char *from = 0 ;
   char *to = 0 ;
   crubadan_format = false ;
   convert_Latin1 = false ;
   byte_limit = ~0 ;
   // process any switches
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
	 case '1': convert_Latin1 = true ;			break ;
	 case '2': parse_bigram_extension(get_arg(argc,argv)) ; break ;
	 case '8': parse_UTF8_extension(get_arg(argc,argv)) ; 	break ;
	 case 'C': parse_clustering(get_arg(argc,argv),
				    cluster_thresh,cluster_db) ; break ;
	 case 'D': do_dump_trie = true ;			break ;
	 case 'l': lang_info.setLanguage(get_arg(argc,argv)) ;	break ;
	 case 'r': lang_info.setRegion(get_arg(argc,argv)) ;	break ;
	 case 'e': lang_info.setEncoding(get_arg(argc,argv)) ;	break ;
	 case 's': lang_info.setSource(get_arg(argc,argv)) ;	break ;
	 case 'W': lang_info.setScript(get_arg(argc,argv)) ;	break ;
	 case 'k': topK = atoi(get_arg(argc,argv)) ;		break ;
	 case 'm': minimum_length = atoi(get_arg(argc,argv)) ;	break ;
	 case 'M': maximum_length = atoi(get_arg(argc,argv)) ;	break ;
	 case 'i': ignore_whitespace = true ;			break ;
	 case 'n': skip_newlines = true ;
	           if (argv[1][2] == 'n') skip_numbers = true ; break ;
	 case 'a': affix_ratio = atof(get_arg(argc,argv)) ;	break ;
	 case 'A': alignment = atoi(get_arg(argc,argv)) ;	break ;
	 case 'b': omit_bigrams = true ;			break ;
	 case 'B': unique_boost = atof(get_arg(argc,argv)) ;	break ;
	 case 'd': discount_factor = atof(get_arg(argc,argv)) ; break ;
	 case 'O': max_oversample = atof(get_arg(argc,argv)) ;  break ;
	 case 'f': frequency_list = true ;
		   frequency_textcat = argv[1][2] == 't' ;
		   crubadan_format = argv[1][2] == 'c' ;	break ;
         case 'L': parse_byte_limit(get_arg(argc,argv)) ;	break ;
	 case 'R': related_langs = get_arg(argc,argv) ;		break ;
	 case 'S': parse_smoothing_power(get_arg(argc,argv)) ;	break ;
	 case 'T': parse_translit(get_arg(argc,argv),from,to) ;	break ;
	 case 'v': verbose = true ;				break ;
	 case 'x': store_similarities = true ;			break ;
	 case 'w': vocabulary_file = argv[1]+2 ;		break ;
	 case 'h':
	 default: usage(argv0,argv[1]) ;			break ;
	 }
      argc-- ;
      argv++ ;
      }
   if (unique_boost < 1.0)
      unique_boost = 1.0 ;
   if (byte_limit < (uint64_t)~0 && verbose)
      cout << "Limiting training to " << byte_limit << " bytes" << endl ;
   if (minimum_length < ABSOLUTE_MIN_LENGTH && !frequency_list)
      {
      minimum_length = ABSOLUTE_MIN_LENGTH ;
      cerr << "Minimum length adjusted to " << ABSOLUTE_MIN_LENGTH << endl ;
      }
   if (bigram_extension != BigramExt_None && minimum_length < 4)
      minimum_length = 4 ;
   if (maximum_length > ABSOLUTE_MAX_LENGTH)
      {
      maximum_length = ABSOLUTE_MAX_LENGTH ;
      cerr << "Maximum length adjusted to " << ABSOLUTE_MAX_LENGTH << endl ;
      }
   if (crubadan_format)
      {
      // set defaults for files from the Crubadan crawler
      lang_info.setEncoding("utf8") ;
      if (!lang_info.source() || !*lang_info.source())
	 lang_info.setSource("Crubadan-Project") ;
      }
   bool check_script = false ;
   if (!guess_script(lang_info))
      {
      if (strcasecmp(lang_info.encoding(),"utf8") == 0 ||
	  strcasecmp(lang_info.encoding(),"utf-8") == 0 ||
	  strncasecmp(lang_info.encoding(),"utf16",5) == 0 ||
	  strncasecmp(lang_info.encoding(),"utf-16",6) == 0)
	 {
	 check_script = true ;
	 }
      }
   if (maximum_length < minimum_length)
      maximum_length = minimum_length ;
   // enforce a valid alignment size
   if (alignment > 4)
      alignment = 4 ;
   else if (alignment == 3)
      alignment = 2 ;
   else if (alignment < 1)
      alignment = 1 ;
   lang_info.setAlignment(alignment) ;
   // check the affix ratio and adjust as needed
   if (affix_ratio > 1.0)
      affix_ratio = 2.0 ;  // disable affix filtering
   else if (affix_ratio < MIN_AFFIX_RATIO)
      affix_ratio = MIN_AFFIX_RATIO ;
   // now accumulate all the named files until we hit the end of the command
   //   line or another switch
   const char **filelist = argv + 1 ;
   while (argc > 1 && argv[1] && (end_of_args || argv[1][0] != '-'))
      {
      argc-- ;
      argv++ ;
      }
   bool success = false ;
   if (cluster_db && *cluster_db)
      {
      success = cluster_models(cluster_db,cluster_thresh) ;
      }
   else if (frequency_list)
      {
      while (filelist <= argv)
	 {
	 unsigned filecount = 1 ;
	 if (frequency_textcat)
	    filecount = (argv - filelist + 1) ;
	 LanguageID local_lang_info(&lang_info) ;
	 if (load_frequencies(filelist,filecount,local_lang_info,
			      frequency_textcat,no_save))
	    success = true ;
	 filelist += filecount ;
	 }
      }
   else
      {
      // check for a transliteration request
      char *translit_to
	 = Fr_aprintf("%s//TRANSLIT",to ? to : lang_info.encoding()) ;
      if (!initialize_transliteration(from,translit_to))
	 {
	 cerr << "Unable to perform conversion from " << from << " to " << translit_to
	      << endl ;
	 }
      FrFree(translit_to) ;
      NybbleTrie *curr_ngrams = 0 ;
      NybbleTrie *ngram_weights = 0 ;
      uint64_t training_bytes = 0 ;
      NybbleTrie *stop_grams = load_stop_grams(&lang_info,related_langs,
					       curr_ngrams,ngram_weights,
					       training_bytes) ;
      success = process_files(filelist,argv-filelist+1,lang_info,
			      curr_ngrams,training_bytes,
			      skip_newlines,omit_bigrams,ignore_whitespace,
			      stop_grams,ngram_weights,no_save,check_script) ;
      delete stop_grams ;
      delete ngram_weights ;
      shutdown_transliteration() ;
      }
   FrFree(from) ;
   FrFree(to) ;
   return success ;
}

//----------------------------------------------------------------------

static int real_main(int argc, const char **argv)
{
   const char *argv0 = argv[0] ;
   const char *database_file = DEFAULT_LANGID_DATABASE ;
   bool no_save = false ;
   if (argc > 1 && argv[1][0] == '=')
      {
      if (argv[1][1] == '=')
	 {
	 no_save = true ;
	 database_file = argv[1]+2 ;
	 }
      else
	 database_file = argv[1]+1 ;
      argv++ ;
      argc-- ;
      }
   if (argc < 2)
      {
      usage(argv0,0) ;
      return 1 ;
      }
   language_identifier = load_language_database(database_file,"",true) ;
   bool success = false ;
   LanguageID lang_info("en","US","utf-8",0) ;
   while (argc > 1)
      {
      if (process_argument_group(argc,argv,lang_info,no_save,argv0))
	 {
	 success = true ;
	 }
      }
   if (success && !no_save)
      save_database(database_file) ;
   delete language_identifier ;
   language_identifier = 0 ;
   return 0 ;
}

//----------------------------------------------------------------------

int main(int argc, const char **argv)
{
   initialize_FramepaC() ;
   int status = real_main(argc,argv) ;
   FrShutdown() ;
   return status ;
}

// end of file mklangid.C //
