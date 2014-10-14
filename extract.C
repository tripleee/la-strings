/************************************************************************/
/*                                                                      */
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     extract.C							*/
/*  Version:  1.24							*/
/*  LastEdit: 13mar2014							*/
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
#include <fcntl.h>
#include <iostream>
#include <memory.h>
#include <unistd.h>
#include "charset.h"
#include "extract.h"
#include "score.h"
#include "langident/trie.h"
#include "langident/langid.h"
#include "FramepaC.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std ;

/************************************************************************/
/*	Manifest Constants						*/
/************************************************************************/

#define EXTRACT_BUFFER_LENGTH 8192

// how many bytes to examine for determining the character encoding to
//   use when extracting the next few strings
#define SCAN_SIZE 384
#define SCAN_OVERLAP 64

// how close to the highest score do other languages have to be for them
//   to be listed as multiple guesses?
#define MULTI_LANG_THRESHOLD 0.85

#define UNAMBIGUITY_BONUS_FACTOR 1.15

// set the multiplicative factor by which to decay the prior scores each time
//   we re-identify character encodings due to a discontinuity in strings
#define DISCONTINUITY_DECAY_FACTOR 0.75

// set the multiplicative factor by which to scale the scores of all models
//   which have encodings other than the encoding used to extract the current
//   string
#define ALTERNATE_CHARSET_FACTOR 0.9

// how many repetitions of a 16-bit value do we require before skipping
//   a block of input as irrelevant for extraction?
#define MIN_REPEATS 12

/************************************************************************/
/*	Types for this module						*/
/************************************************************************/

class InputStreamFile : public InputStream
   {
   private:
      FILE *m_fp ;
   public:
      InputStreamFile(FILE *fp) { m_fp = fp ; }
      virtual ~InputStreamFile()
	 { if (m_fp && m_fp != stdin) { fclose(m_fp) ; } }

      virtual bool endOfData() const { return m_fp ? feof(m_fp) : true ; }
      virtual uint64_t currentOffset() const
	 { return (!m_fp || m_fp == stdin) ? 0 : (uint64_t)ftell(m_fp) ; }
      virtual unsigned get(unsigned count, unsigned char *buffer)
	 { return m_fp ? fread(buffer,sizeof(char),count,m_fp) : 0 ; }
   } ;

/************************************************************************/
/*	Global variables for this module				*/
/************************************************************************/

static LanguageScores *prior_language_scores = 0 ;
double unambiguity_bonus_factor = UNAMBIGUITY_BONUS_FACTOR ;

/************************************************************************/
/*	Helper functions	       					*/
/************************************************************************/

static uint64_t filesize(FILE *fp)
{
   if (!fp)
      return 0 ;
   // note: while fstat() could be used here, this version will reflect the
   //   correct size even if the file has been written to without the directory
   //   entry being updated (as happens, e.g. under Windoze)
   int fd = fileno(fp) ;
   uint64_t pos = lseek(fd,0,SEEK_CUR) ;
   uint64_t end = lseek(fd,0L,SEEK_END) ;  // find file's size
   lseek(fd,(off_t)pos,SEEK_SET) ;         // return to original position
   return end ;
}

//----------------------------------------------------------------------

static bool is_stdin(const char *filename)
{
   return filename != 0 && strcmp(filename,"-") == 0 ;
}

//----------------------------------------------------------------------

void set_binary_mode(FILE *fp, const ExtractParameters *params)
{
   if (fp)
      {
      if (params->outputFormat() == OF_UTF16LE)
	 {
#ifdef O_BINARY
	 setmode(fileno(fp),O_BINARY) ;
#endif
	 fputs("\xFF\xFE",fp) ;
	 }
      else if (params->outputFormat() == OF_UTF16BE)
	 {
#ifdef O_BINARY
	 setmode(fileno(fp),O_BINARY) ;
#endif
	 fputs("\xFE\xFF",fp) ;
	 }
      }
   return ;
}

/************************************************************************/
/*	Methods for class ExtractParameters				*/
/************************************************************************/

ExtractParameters::ExtractParameters(const ExtractParameters &orig)
{
   memcpy(this,&orig,sizeof(ExtractParameters)) ;
   m_charsets = 0 ; m_numcharsets = 0 ;
   m_encsets = 0 ; m_numencsets = 0 ;
   setCharSets() ;
   return ;
}

//----------------------------------------------------------------------

ExtractParameters::~ExtractParameters()
{
   delete m_priorscores ; m_priorscores = 0 ;
   clearCharSets() ;
   return ;
}

//----------------------------------------------------------------------

void ExtractParameters::clearCharSets()
{
   FrFree(m_charsets) ; m_charsets = 0 ;
   m_numcharsets = 0 ;
   FrFree(m_encsets) ; m_encsets = 0 ;
   m_numencsets = 0 ;
   return ;
}

//----------------------------------------------------------------------

void ExtractParameters::setCharSets()
{
   FrFree(m_charsets) ;
   m_charsets = 0 ;
   m_numcharsets = 0 ;
   FrFree(m_encsets) ;
   m_encsets = 0 ;
   m_numencsets = 0 ;
   const LanguageIdentifier *ident = languageIdentifier() ;
   if (ident)
      {
      size_t numsets = ident->numLanguages() ;
      m_charsets = FrNewN(const CharacterSet*,numsets) ;
      if (m_charsets)
	 {
	 m_numcharsets = numsets ;
	 CharacterSetCache *cache = CharacterSetCache::instance() ;
	 for (size_t i = 0 ; i < numsets ; i++)
	    {
	    const char *encoding = ident->languageEncoding(i) ;
	    m_charsets[i] = cache->getCharSet(encoding) ;
	    }
	 }
      ident = ident->charsetIdentifier() ;
      if (ident)
	 {
	 size_t numsets = ident->numLanguages() ;
	 m_encsets = FrNewN(const CharacterSet*,numsets) ;
	 if (m_encsets)
	    {
	    m_numencsets = numsets ;
	    CharacterSetCache *cache = CharacterSetCache::instance() ;
	    for (size_t i = 0 ; i < numsets ; i++)
	       {
	       const char *encoding = ident->languageEncoding(i) ;
	       m_encsets[i] = cache->getCharSet(encoding) ;
	       }
	    }
	 else
	    {
	    m_numencsets = 0 ;
	    }
	 }
      }
   return ;
}

//----------------------------------------------------------------------

void ExtractParameters::wantLocation(char locspec)
{
   switch (locspec)
      {
      case 'o':
      case 'O':
	 m_locationradix = 8 ;
	 break ;
      case 'd':
      case 'D':
	 m_locationradix = 10 ;
	 break ;
      case 'h':
      case 'H':
      case 'x':
      case 'X':
	 m_locationradix = 16 ;
	 break ;
      default:
	 m_locationradix = 0 ;
	 break ;
      }
   return ;
}

//----------------------------------------------------------------------

void ExtractParameters::wantCRLF(bool force)
{
//   (void)force ;
//#ifdef unix
   m_forceCRLF = force ;
//#endif
   return ;
}

/************************************************************************/
/************************************************************************/

static void discount_alternate_charsets(LanguageScores *scores,
					const ExtractParameters *params,
					const CharacterSet *charset)
{
   if (!scores || !charset)
      return ;
   for (size_t i = 0 ; i < scores->numLanguages() ; i++)
      {
      if (params->charset(i) != charset)
	 scores->scaleScore(i,ALTERNATE_CHARSET_FACTOR) ;
      }
   return ;
}

//----------------------------------------------------------------------

static void decay_language_scores()
{
   if (prior_language_scores)
      prior_language_scores->scaleScores(DISCONTINUITY_DECAY_FACTOR) ;
   return ;
}

//----------------------------------------------------------------------

static void add_unambiguous_bonus(const LanguageScores *scores)
{
   if (prior_language_scores)
      prior_language_scores->scaleScore(scores->topLanguage(),
					unambiguity_bonus_factor) ;
   return ;
}

//----------------------------------------------------------------------

void free_language_scores()
{
   delete prior_language_scores ;
   prior_language_scores = 0 ;
   return ;
}

//----------------------------------------------------------------------

static bool same_language(const char *name1, const char *name2)
{
   if (!name1 || !name2)
      return false ;
   return strcmp(name1,name2) == 0 ;
}

//----------------------------------------------------------------------

extern uint32_t get_UTF8(char *&string) ;

//----------------------------------------------------------------------

static void write_UTF16(FILE *fp, uint32_t codepoint, bool big_endian)
{
   if (codepoint > 0xFFFF)
      {
      // write two surrogate code points to represent the value that doesn't
      //  fit into 16 bits
      write_UTF16(fp,0xD800 + ((codepoint >> 10) & 0x3FF),big_endian) ;
      write_UTF16(fp,0xDC00 + (codepoint & 0x3FF),big_endian) ;
      }
   else if (big_endian)
      {
      fputc((codepoint >> 8) & 0xFF,fp) ;
      fputc((codepoint & 0xFF),fp) ;
      }
   else // little-endian
      {
      fputc((codepoint & 0xFF),fp) ;
      fputc((codepoint >> 8) & 0xFF,fp) ;
      }
   return;
}

//----------------------------------------------------------------------

static void print(FILE *fp, const ExtractParameters *params,
		  const char *fmt, ...)
{
   FrVarArgs(fmt) ;
   char *string = Fr_vaprintf(fmt,args) ;
   if (string)
      {
      // apply the appropriate conversion
      if (params->outputFormat() == OF_UTF16LE)
	 {
	 char *s = string ;
	 while (*s)
	    {
	    uint32_t codepoint = get_UTF8(s) ;
	    write_UTF16(fp,codepoint,false) ;
	    }
	 }
      else if (params->outputFormat() == OF_UTF16BE)
	 {
	 char *s = string ;
	 while (*s)
	    {
	    uint32_t codepoint = get_UTF8(s) ;
	    write_UTF16(fp,codepoint,true) ;
	    }
	 }
      else
	 {
	 // OF_Native/OF_UTF8 don't need any conversion
	 fputs(string,fp) ;
	 }
      }
   FrFree(string) ;
   FrVarArgEnd() ;
   return ;
}

//----------------------------------------------------------------------

static void write(FILE *fp, const ExtractParameters *params, size_t bytes,
		  const unsigned char *string)
{
   (void)params ; //!!!
   fwrite(string,1,bytes,fp) ;
   return ;
}

//----------------------------------------------------------------------

static void show_string(FILE *outfp, const unsigned char *buffer,
			unsigned len, uint64_t bufloc, const char *filename,
			double confidence, const ExtractParameters *params,
			CharacterSet *charset,
			LanguageScores *scores, bool verbose)
{
   const LanguageIdentifier *ident = params->languageIdentifier() ;
   if (params->outputFn())
      {
      discount_alternate_charsets(scores,params,charset) ;
      if (scores && params->smoothLanguageScores())
	 scores = smoothed_language_scores(scores,params->priorLanguageScores(),len) ;
      params->outputFn()(buffer,len,bufloc,charset,confidence,scores,params) ;
      return ;
      }
   if (params->showFilename())
      {
      print(outfp,params,"%s:\t",filename) ;
      }
   switch (params->showLocation())
      {
      case 8:
	 print(outfp,params,"%8.08lo ",(unsigned long)bufloc) ;
	 break ;
      case 10:
	 print(outfp,params,"%8.08lu ",(unsigned long)bufloc) ;
	 break ;
      case 16:
	 print(outfp,params,"%8.08lX ",(unsigned long)bufloc) ;
	 break ;
      default:
	 // nothing to do
	 break ;
      }
   if (params->showConfidence())
      print(outfp,params,"%6.3f\t",confidence) ;
   if (params->identifyLanguage())
      {
      if (ident)
	 {
	 bool delete_scores = false ;;
	 if (!scores)
	    {
	    scores = ident->identify((const char*)buffer,len) ;
	    ident->finishIdentification(scores) ;
	    delete_scores = true ;
	    }
	 discount_alternate_charsets(scores,params,charset) ;
	 if (params->smoothLanguageScores())
	    {
	    scores = smoothed_language_scores(scores,len) ;
	    }
	 if (scores)
	    {
	    // sort the scores, trimming out any scores below the
	    //   greater of LANGID_ZERO_SCORE and THRESHOLD * maxscore
	    scores->sort(MULTI_LANG_THRESHOLD,2*params->maxLanguages()) ;
	    size_t shown = 0 ;
#if 0
	    // if a language has a lot of models, this could erroneously
	    //   flag the identification as unsure
	    bool unsure = (scores->numLanguages() > params->maxLanguages() &&
			   scores->numLanguages() > 5) ;
	    if (scores->score(0) > SURE_THRESHOLD)
	       unsure = false ;
#else
	    bool unsure = false ;
#endif
	    if (params->countLanguages() && scores->score(0) > GUESS_CUTOFF)
	       ((LanguageIdentifier*)ident)->incrStringCount(scores->languageNumber(0)) ;
	    for (size_t i = 0 ;
		 i < scores->numLanguages() && shown < params->maxLanguages() ;
		 i++)
	       {
	       double sc = scores->score(i) ;
	       const char *langname = "??" ;
	       const char *script = "UNK" ;
	       if (sc > GUESS_CUTOFF)
		  {
		  unsigned langnum = scores->languageNumber(i) ;
		  langname = ident->languageName(langnum) ;
		  for (size_t j = 0 ; j < i ; j++)
		     {
		     if (same_language(ident->languageName(scores->languageNumber(j)),
				       langname))
			{
			langname = 0 ;
			break ;
			}
		     }
		  if (langname)
		     script = ident->languageScript(langnum) ;
		  }
	       else if (shown > 0)
		  break ;
	       if (langname)
		  {
		  if (shown > 0)
		     print(outfp,params,",") ;
		  shown++ ;
		  if (params->showFriendlyName())
		     {
		     print(outfp,params,"%s",langname) ;
		     }
		  else
		     {
		     print(outfp,params,"%.7s",langname) ;
		     }
		  if (params->showEncoding())
		     {
		     print(outfp,params,"_%s",params->charset(scores->languageNumber(i))->encodingName()) ;
		     }
		  if (params->showScript())
		     {
		     print(outfp,params,"@%s",script) ;
		     }
		  if (verbose)
		     {
		     print(outfp,params,":%g",sc) ;
		     }
		  else if ((sc < UNSURE_CUTOFF || unsure) &&
			   *langname != '?')
		     {
		     print(outfp,params,"?") ;
		     }
		  }
	       }
	    if (scores->numLanguages() == 1)
	       add_unambiguous_bonus(scores) ;
	    if (shown > 0)
	       print(outfp,params,"\t") ;
	    if (delete_scores)
	       ident->freeScores(scores) ;
	    }
	 }
      }
   if (charset)
      {
      if (params->showEncoding())
	 print(outfp,params,"%s\t",charset->encodingName()) ;
      if (params->outputFormat() != OF_Native)
	 {
	 charset->writeAsUTF(outfp, buffer, len, false,
			     params->outputFormat()) ;
	 if (params->romanizeOutput() && charset->romanizable(buffer,len))
	    {
	    print(outfp,params,"\n") ;
	    if (params->forceCRLF())
	       {
	       print(outfp,params,"\r") ;
	       }
	    print(outfp,params,"  -->\t") ;
	    charset->writeAsUTF(outfp, buffer, len, true,
				params->outputFormat()) ;
	    }
	 }
      else if (charset->filterNUL())
	 {
	 for (size_t i = 0 ; i < len ; i++)
	    {
	    if (buffer[i])
	       print(outfp,params,"%c",buffer[i]) ;
	    }
	 }
      else
	 write(outfp, params, len, buffer) ;
      }
   else
      {
      write(outfp, params, len, buffer) ;
      }
   if (buffer[len -1] != '\n')
      {
      print(outfp,params,"\n") ;
      }
   if (params->forceCRLF())
      {
      print(outfp,params,"\r") ;
      }
   return ;
}

//----------------------------------------------------------------------

static int extract_text(const unsigned char *bufstart, int buflen,
			const CharacterSet *charset,
			const ExtractParameters *params,
			double &confidence)
{
   int length = 0 ;
   confidence = 0.0 ;
   StringScore score ;
   NybbleTriePointer *dictionary = charset->dictionary() ;
   if (dictionary)
      score.haveDictionary() ;
   bool prev_alphanum = false ;
   EscapeState in_escape_sequence = ES_None ;
   const unsigned char *buf = bufstart ;
   while (buflen > 0)
      {
      wchar_t codepoint ;
      int charsize = charset->nextCodePoint(buf,codepoint,in_escape_sequence) ;
      if (charsize <= 0 || codepoint == 0)
	 break ;
      score.update(charset,codepoint,charsize) ;
      if (dictionary)
	 {
	 bool alphanum = charset->isAlphaNum(codepoint) ;
	 if (alphanum)
	    {
	    if (!prev_alphanum) 	      // start of a potential word?
	       dictionary->resetKey() ;
	    for (int i = 0 ; i < charsize ; i++)
	       dictionary->extendKey(buf[i]) ;
	    }
	 else
	    {
	    if (prev_alphanum) // passed end of potential word?
	       {
	       if (dictionary->lookupSuccessful())
		  score.addWord(dictionary->keyLength()) ;
	       }
	    if (codepoint == ' ' || codepoint == '\t')
	       score.addWord(1) ;
	    }
	 prev_alphanum = alphanum ;
	 }
      length += charsize ;
      buf += charsize ;
      buflen -= charsize ;
      if (score.undesiredRun() > params->maximumGap())
	 {
	 length -= score.undesiredRun() ;
	 break ;
	 }
      }
   if (dictionary && prev_alphanum && dictionary->lookupSuccessful())
      score.addWord(dictionary->keyLength()) ;
   score.finalize() ;
   bool passed_filter = true ;
   if (length > 0)
      {
      if (score.totalChars() < params->minimumString())
	 passed_filter = false ;
      if (score.alphaPercent() < params->minAlphaPercent())
	 passed_filter = false ;
      if (score.desiredPercent() < params->minDesiredPercent())
	 passed_filter = false ;
      confidence = score.computeScore() * charset->detectionReliability() ;
      }
   delete dictionary ;
   // negative length means advance that many bytes without displaying
   //   the string
   return passed_filter ? length : -length ;
}

//----------------------------------------------------------------------

static int try_extract_text(const unsigned char *buf, int buflen,
			    CharacterSet **charsets,
			    const ExtractParameters *params,
			    double &confidence,
			    CharacterSet *&best_charset)
{
   double best_conf = 0.0 ;
   int best_length = 0 ;
   int best_neg_length = 0 ;
   best_charset = 0 ;
   for (unsigned i = 0 ; charsets[i] ; i++)
      {
      double conf = -1.0 ;
      int len = extract_text(buf,buflen,charsets[i],params,conf) ;
      if (len > best_length ||
	  (len == best_length && conf > best_conf))
	 {
	 best_length = len ;
	 best_conf = conf ;
	 best_charset = charsets[i] ;
	 }
      else if (len < best_neg_length)
	 best_neg_length = len ;
      }
   confidence = best_conf ;
   return best_length > 0 ? best_length : best_neg_length ;
}

//----------------------------------------------------------------------

static int extract_text(const unsigned char *buf, int buflen,
			CharacterSet **charsets,
			const ExtractParameters *params,
			double &confidence,
			CharacterSet *&best_charset,
			unsigned &offset,
			LanguageScores *&scores)
{
   double conf1 ;
   CharacterSet *set1 ;
   int len1 = try_extract_text(buf,buflen,charsets,params,conf1,set1) ;
   offset = 0 ;
   if (set1 && set1->alignment() > 1 && buflen > 2)
      {
      int abslen1 = abs(len1) ;
      int nl1 = set1->consumeNewlines(buf + abslen1, buflen - abslen1) ;
      abslen1 += nl1 ;
      // accept the offset string as the actual string if:
      //   a) it is longer
      //   b) it is the same overall length but otherwise better
      //   c) the non-offset string consumes the entire buffer
      //      (probably false positive) while the offset string does
      //      not
      // AND
      //   the non-offset string is NOT terminated with a newline
      if (nl1 == 0)
	 {
	 double conf2 ;
	 CharacterSet *set2 ;
	 int len2 = try_extract_text(buf+1,buflen-1,charsets,params,
				     conf2,set2) ;
	 int abslen2 = abs(len2) ;
	 if (set2)
	    abslen2 += set2->consumeNewlines(buf + 1 + abslen2,
					     buflen - 1 - abslen2) ;
	 if (abslen2 > abslen1 ||
	     (abslen2 == abslen1 && (len2 > len1 || conf2 >= conf1)) ||
	     (abslen1 >= buflen && abslen2 < buflen-1))
	    {
            offset = 1 ;
	    conf1 = conf2 ;
	    set1 = set2 ;
	    len1 = len2 ;
	    }
	 }
      }
   LanguageIdentifier *ident = params->languageIdentifier() ;
   if (ident && len1 > 2)
      {
      scores = ident->identify(scores,(const char*)buf+offset,len1,false) ;
      if (scores)
	 {
	 double rawscore = scores->highestScore() ;
	 if (len1 >= 12 && (rawscore > GUESS_CUTOFF) &&
	     set1->detectionReliability() < 1.0)
	    {
	    // for strings of at least 12 bytes, check whether the
	    //   encoding for the highest-scoring language matches the
	    //   extraction encoding and adjust if there is a conflict
	    //   for an unreliable extraction encoding
	    unsigned bestlangid = scores->highestLangID() ;
	    const CharacterSet *set = params->charset(bestlangid) ;
	    if (set)
	       {
	       if (set->detectionReliability() >= 1.0 ||
		   (set1->alignment() > 1 && set->alignment() > 1 &&
		    set->bigEndian() != set1->bigEndian()))
		  {
		  // force an extraction in the encoding that scored
		  //   highest on the language models to get the
		  //   correct size/endianness
		  if (set->alignment() == 1 && set1->alignment() > 1)
		     offset = 0 ;
		  set1 = (CharacterSet*)set ;
		  len1 = 0 ;
		  while (len1 <= 0 && offset + 2 < (unsigned)buflen)
		     {
		     len1 = extract_text(buf+offset,buflen-offset,set1,
					 params,conf1) ;
		     if (len1 <= 0)
			offset++ ;
		     }
		  if (len1 <= 0)
		     {
		     if (len1 >= -1)
			len1 = -set->alignment() ;
		     }
		  }
	       }
	    }
	 // the longer the string, the higher the proportion of the
	 //  overall confidence score we take from the language-id
	 //  score: 1/4 for strings up to 8 bytes, 1/3 for strings
	 //  9-20 bytes, 1/2 for strings 21-40 bytes, 2/3 for 41-60
	 //  bytes, 4/5 for 61-80 bytes, and 7/8 for 81+ bytes
	 double langidscore = 10.0 * rawscore ;
	 if (len1 <= 8)
	    {
	    conf1 = (3.0 * conf1 + langidscore) / 4.0 ;
	    }
	 else if (len1 <= 20)
	    {
	    conf1 = (2.0 * conf1 + langidscore) / 3.0 ;
	    }
	 else if (len1 <= 40)
	    {
	    conf1 = (conf1 + langidscore) / 2.0 ;
	    }
	 else if (len1 <= 60)
	    {
	    conf1 = (conf1 + (2.0 * langidscore)) / 3.0 ;
	    }
	 else if (len1 <= 80)
	    {
	    conf1 = (conf1 + (4.0 * langidscore)) / 5.0 ;
	    }
	 else
	    {
	    conf1 = (conf1 + (7.0 * langidscore)) / 8.0 ;
	    }
	 // cap the score just in case it goes really high, to keep the
	 //   output nicely formatted
	 if (conf1 > 99.999)
	    conf1 = 99.999 ;
	 }
      }
   confidence = conf1 ;
   best_charset = set1 ;
   return len1 ;
}

//----------------------------------------------------------------------

static bool buffer_contains_ASCII16(const unsigned char *buffer,
				    size_t buflen)
{
   size_t valid = 0 ;
   size_t weight = 1 ;
   const size_t max_weight = 3 ;
   for (size_t i = 0 ; i + 5 < buflen ; i++)
      {
      if (buffer[i+1] == 0 && buffer[i+3] == 0 && buffer[i+5] == 0)
	 {
	 // check for three consecutive ASCII characters encoded in 16 bits
	 if (buffer[i] > '\0' && buffer[i] < '\x7F' &&
	     buffer[i+2] > '\0' && buffer[i+2] < '\x7F' &&
	     buffer[i+4] > '\0' && buffer[i+4] < '\x7F')
	    {
	    valid += weight ;
	    if (weight < max_weight) weight++ ;
	    i += 5 ;
	    }
	 // check for wide Latin-1 or equivalent -- allow any nonzero
	 //   value for the low-order byte, but ensure four consecutive
	 //   valid characters
	 else if (i + 7 < buflen &&
		  buffer[i] && buffer[i+2] && buffer[i+4] && buffer[i+6] &&
		  buffer[i+7] == 0)
	    {
	    valid += weight ;
	    if (weight < max_weight) weight++ ;
	    i += 7 ;
	    }
	 else
	    weight = 1 ;
	 }
      else
	 weight = 1 ;
      }
   return (valid > 6) && (valid >= buflen / 64) ;
}

//----------------------------------------------------------------------

static bool buffer_contains_UTF8(const unsigned char *buffer,
				 size_t buflen)
{
   size_t valid = 0 ;
   size_t multibyte = 0 ;
   size_t weight = 1 ;
   const size_t max_weight = 4 ;
   for (size_t i = 0 ; i + 3 < buflen ; i++)
      {
      if ((buffer[i] & 0xE0) == 0xC0 && (buffer[i+1] & 0xC0) == 0x80 &&
	  (buffer[i+2] & 0xC0) != 0x80)
	 {
	 multibyte++ ;
	 valid += weight ;
	 if (weight < max_weight) weight++ ;
	 i++ ;
	 }
      else if ((buffer[i] & 0xF0) == 0xE0 && (buffer[i+1] & 0xC0) == 0x80 &&
	       (buffer[i+2] & 0xC0) == 0x80 && (buffer[i+3] & 0xC0) != 0x80)
	 {
	 multibyte++ ;
	 valid += weight ;
	 if (weight < max_weight) weight++ ;
	 i += 2 ;
	 }
      else if (i + 4 < buflen &&
	       (buffer[i] & 0xF8) == 0xF0 && (buffer[i+1] & 0xC0) == 0x80 &&
	       (buffer[i+2] & 0xC0) == 0x80 && (buffer[i+3] & 0xC0) == 0x80 &&
	       (buffer[i+4] & 0xC0) != 0x80)
	 {
	 multibyte++ ;
	 valid += weight ;
	 if (weight < max_weight) weight++ ;
	 i += 3 ;
	 }
      else if (i + 5 < buflen &&
	       (buffer[i] & 0xFC) == 0xF8 && (buffer[i+1] & 0xC0) == 0x80 &&
	       (buffer[i+2]&0xC0) == 0x80 && (buffer[i+3] & 0xC0) == 0x80 &&
	       (buffer[i+4] & 0xC0) == 0x80 && (buffer[i+5] & 0xC0) != 0x80)
	 {
	 multibyte++ ;
	 valid += weight ;
	 if (weight < max_weight) weight++ ;
	 i += 4 ;
	 }
      else if (i + 6 < buflen &&
	       (buffer[i] & 0xFE) == 0xFC && (buffer[i+1] & 0xC0) == 0x80 &&
	       (buffer[i+2]&0xC0) == 0x80 && (buffer[i+3] & 0xC0) == 0x80 &&
	       (buffer[i+4] & 0xC0) == 0x80 && (buffer[i+5] & 0xC0) == 0x80 &&
	       (buffer[i+6] & 0xC0) != 0x80)
	 {
	 multibyte++ ;
	 valid += weight ;
	 if (weight < max_weight) weight++ ;
	 i += 5 ;
	 }
      else if (buffer[i] > 0 && buffer[i] < 0x7F)
	 {
	 if (weight + 1 > max_weight)
	    valid++ ;
	 else
	    weight++ ;
	 }
      else // if (buffer[i] == 0 || buffer[i] >= 0x7F)
	 {
	 // not a valid UTF-8 codepoint, so no boost for a following valid
	 //   codepoint
	 weight = 1 ;
	 }
      }
   return (valid > 60 || multibyte > 4) && valid >= buflen / 24 ;
}

//----------------------------------------------------------------------

static CharacterSet **identify_charsets(const unsigned char *buffer,
					size_t buflen,
					const ExtractParameters *params,
					LanguageScores *&scores,
					CharacterSet **sets)

{
   LanguageIdentifier *ident = params->languageIdentifier() ;
   if (ident)
      ident = ident->charsetIdentifier() ;
   if (ident)
      {
      double bigram_weight = ident->bigramWeight() ;
      ident->setBigramWeight(0.0) ;
      scores = ident->identify(scores,(const char*)buffer,buflen,false,false,false) ;
      ident->setBigramWeight(bigram_weight) ;
      }
   else
      {
      delete scores ;
      scores = 0 ;
      }
   CharacterSetCache *cache = CharacterSetCache::instance() ;
   size_t setcount = 0 ;
   if (scores)
      {
      if (sets)
	 {
	 for (size_t i = 0 ; i <= ident->numLanguages()+ENCID_FALLBACK_SETS ; i++)
	    sets[i] = 0 ;
	 }
      else
	 sets = FrNewC(CharacterSet*,ident->numLanguages()+ENCID_FALLBACK_SETS+1) ;
      for (size_t i = 0 ; i < scores->numLanguages() ; i++)
	 {
	 const CharacterSet *set = params->encodingCharset(scores->languageNumber(i)) ;
	 if (set)
	    scores->scaleScore(i,set->detectionReliability()) ;
	 }
      // sort the scores, trimming out any below LANGID_ZERO_SCORE or
      //   0.5*maxscore
      scores->sort(0.5) ;
      size_t num_sets = scores->nonzeroScores() ;
      for (size_t i = 0 ; i < num_sets ; i++)
	 {
	 // only use the language-detected encodings if we're not guessing
	 if (scores->score(i) < GUESS_CUTOFF / 4.0)
	    break ;
	 CharacterSet *set =
	    (CharacterSet*)params->encodingCharset(scores->languageNumber(i)) ;
	 if (!set)
	    continue ;
//cerr<<" enc "<<scores->languageNumber(i)<<" = "<<set->encodingName()<<endl;
	 sets[setcount++] = set ;
	 // eliminate duplicate character sets for efficiency
	 for (size_t j = 0 ; j < setcount-1 ; j++)
	    {
	    if (sets[j] == set)
	       {
	       setcount-- ;
	       break ;
	       }
	    }
	 }
      }
   else if (cache)
      {
      if (sets)
	 {
	 for (size_t i = 0 ; i <= ENCID_FALLBACK_SETS ; i++)
	    sets[i] = 0 ;
	 }
      else
	 sets = FrNewC(CharacterSet*,ENCID_FALLBACK_SETS+1) ;
      }
   else
      {
      if (sets)
	 FrFree(sets) ;
      return 0 ;
      }
#if 0
   if (sets)
      {
      sets[setcount] = 0 ;
      cerr << "detected charsets:" ;
      for (size_t i = 0 ; sets[i] ; i++)
	 {
	 cerr << ' ' << sets[i]->encodingName() ;
	 }
      cerr << endl ;
      }
#endif
   if (sets)
      {
      const CharacterSet *cs_ASCII = cache->getCharSet("ASCII") ;
      const CharacterSet *cs_ASCII16 = cache->getCharSet("ASCII-16LE") ;
      const CharacterSet *cs_UTF8 = cache->getCharSet("UTF-8") ;
      bool have_ASCII = false ;
      bool have_ASCII16 = false ;
      bool have_UTF8 = false ;
      size_t ascii_pos = 0 ;
      for (size_t i = 0 ; i < setcount ; i++)
	 {
	 if (sets[i] == cs_ASCII16)
	    have_ASCII16 = true ;
	 else if (sets[i] == cs_UTF8)
	    {
	    have_UTF8 = true ;
	    // swap the charset to the front of the list -- because of its
	    //   low rate of false positives, we want to process UTF-8 before
	    //   other similar charsets like Latin-1 so that UTF-8 wins a
	    //   tie in length with the other charset
	    sets[i] = sets[0] ;
	    sets[0] = (CharacterSet*)cs_UTF8 ;
	    if (sets[i] == cs_ASCII)
	       ascii_pos = i ;
	    }
	 else if (sets[i] == cs_ASCII)
	    {
	    have_ASCII = true ;
	    ascii_pos = i ;
	    }
	 }
      if (setcount == 0)
	 {
	 // fallback is to always search for plain ASCII if we can't
	 //   figure out the language
	 sets[setcount++] = (CharacterSet*)cs_ASCII ;
	 have_ASCII = true ;
	 }
      if (!have_ASCII /*&& !have_UTF8 */)
	 {
	 ascii_pos = setcount++ ;
	 }
      if (ascii_pos > 0)
	 {
	 // because most other character sets are supersets of ASCII,
	 //   if we get a string that consists solely of ASCII
	 //   characters, we want it to be identified as ASCII rather
	 //   than UTF-8 or another superset charset, so move ASCII
	 //   into first position.  If we previously swapped UTF-8 to
	 //   the front, it becomes second
         unsigned dest = have_UTF8 ? 1 : 0 ;
	 for (size_t i = ascii_pos ; i > dest ; i--)
	    {
	    sets[i] = sets[i-1] ;
	    }
	 sets[dest] = (CharacterSet*)cs_ASCII ;
	 }
      if (sets[0] != cs_UTF8 && buffer_contains_UTF8(buffer,buflen))
	 {
	 // make sure UTF8 is in second position after ASCII
	 if (setcount > 1)
	    {
	    sets[setcount++] = sets[1] ;
	    sets[1] = (CharacterSet*)cs_UTF8 ;
	    }
	 else
	    sets[setcount++] = (CharacterSet*)cs_UTF8 ;
	 }
      if (!have_ASCII16 && buffer_contains_ASCII16(buffer,buflen))
	 {
	 // keep ASCII-16LE near the front of the list
	 if (setcount > 2)
	    {
	    sets[setcount++] = sets[1] ;
	    sets[2] = (CharacterSet*)cs_ASCII16 ;
	    }
	 else
	    sets[setcount++] = (CharacterSet*)cs_ASCII16 ;
	 }
      }
   sets[setcount] = 0 ;
#if 0
   cerr << " reported charsets:" ;
   for (size_t i = 0 ; sets[i] ; i++)
      {
      cerr << ' ' << sets[i]->encodingName() ;
      }
   cerr << endl ;
#endif
   return sets ;
}

//----------------------------------------------------------------------

static bool fill_buffer(InputStream *in, unsigned char *buffer,
			unsigned &buflen, unsigned &offset,
			uint64_t &bufloc, uint64_t end_offset)
{
   bool skipped_bytes ;
   do {
      // move any remnant of the previous buffer down to the start
      //   of the buffer
      if (buflen > 0)
	 {
	 if (offset < buflen)
	    memmove(buffer,buffer + offset, buflen - offset) ;
	 else if (offset > buflen)
	    offset = buflen ;
	 buflen -= offset ;
	 bufloc += offset ;
	 offset = 0 ;
	 }
      // (re)fill the buffer
      unsigned cnt = 0 ;
      if (bufloc < end_offset && !in->endOfData())
	 {
	 unsigned remaining = EXTRACT_BUFFER_LENGTH - buflen ;
	 if (bufloc + remaining > end_offset)
	    remaining = (unsigned)(end_offset - bufloc) ;
	 cnt = in->get(remaining,buffer + buflen) ;
	 }
      if (cnt > 0)
	 {
	 buflen += cnt ;
	 }
      // check for and skip repeated bytes at the start of the buffer
      skipped_bytes = false ;
      if (buflen > MIN_REPEATS*sizeof(uint16_t) &&
	  FrLoadShort(buffer+0) == FrLoadShort(buffer+sizeof(uint16_t)))
	 {
	 uint16_t value = FrLoadShort(buffer+0) ;
	 unsigned repeats = 2 ;
	 while (repeats < buflen / sizeof(uint16_t) &&
		FrLoadShort(buffer+(sizeof(uint16_t)*repeats)) == value)
	    {
	    repeats++ ;
	    }
	 if (repeats >= MIN_REPEATS)
	    {
	    offset = repeats * sizeof(uint16_t) ;
	    skipped_bytes = true ;
	    }
	 }
      } while (skipped_bytes) ;
   return buflen > 0 ;
}

//----------------------------------------------------------------------

void extract_text(InputStream *in, FILE *outfp, const char *filename,
		  uint64_t end_offset, const CharacterSet * const *given_charsets,
		  const ExtractParameters *params, bool verbose,
		  LanguageScores *given_langscores,
		  LanguageScores *given_charset_scores)
{
   unsigned char buffer[EXTRACT_BUFFER_LENGTH+4] ;
   // ^^^ add padding in case of a partial codepoint at the end of the buffer
   // initialize the over-run area to keep memory checkers happy
   buffer[EXTRACT_BUFFER_LENGTH] = buffer[EXTRACT_BUFFER_LENGTH+1] = '\xFF' ;
   unsigned buflen = 0 ;
   unsigned offset = 0 ;
   uint64_t bufloc = in->currentOffset() ;
   bool automatic_charsets = !given_charsets || (given_charsets[0] == 0) ;
   CharacterSet **charsets = automatic_charsets ? 0 : (CharacterSet**)given_charsets ;
   LanguageScores *charset_scores = given_charset_scores ;
   LanguageScores *langscores = given_langscores ;
   while ((!in->endOfData() && bufloc < end_offset) || buflen > offset)
      {
      if (!fill_buffer(in,buffer,buflen,offset,bufloc,end_offset))
	 break ;
      // figure out the next re-fill point
      unsigned highwater = EXTRACT_BUFFER_LENGTH / 2 ;
      // if using language identification with charset AUTO, scan the
      //   buffer to figure out which character sets are in use
      if (automatic_charsets)
	 {
	 // reduce the re-fill point, so that we get to scan for
	 //   language identification more often
	 if (buflen < SCAN_SIZE - SCAN_OVERLAP)
	    highwater = buflen ;
	 else
	    highwater = SCAN_SIZE - SCAN_OVERLAP ;
	 unsigned scan_size = (buflen < SCAN_SIZE) ? buflen : SCAN_SIZE ;
	 charsets = identify_charsets(buffer,scan_size,params,charset_scores,charsets) ;
	 }
      else if (highwater > buflen)
	 highwater = buflen ;
      // scan the current buffer for strings
      unsigned skipped = 0 ;
      unsigned extracted_strings = 0 ;
      while (offset < highwater)
	 {
	 double confidence ;
	 CharacterSet *charset ;
	 unsigned adj ;
	 int len = extract_text(buffer + offset, buflen - offset, 
				charsets, params, confidence, charset, adj,
				langscores) ;
	 offset += adj ;
	 if (len > 0)
	    {
	    if ((unsigned)len >= buflen - offset)
	       {
	       // the extraction may have over-run the buffer if there
	       //   was an incomplete code point at the end, so adjust
	       //   the length downward
	       if ((unsigned)len > buflen - offset)
		  len = buflen - offset ;
	       // if we got a "string" consisting of the entire
	       //   buffer for a highly-ambiguous encoding such as
	       //   UTF-16, that's probably a false positive, so
	       //   retry from the highwater mark
	       if (charset && charset->detectionReliability() < 1.0)
		  {
		  if ((unsigned)len > highwater)
		     len = highwater ;
		  }
	       }
	    if (confidence >= params->minimumScore())
	       {
	       show_string(outfp, buffer + offset, len, bufloc + offset,
			   filename, confidence, params, charset,
			   langscores, verbose) ;
	       }
	    offset += len ;
	    unsigned nl = charset->consumeNewlines(buffer + offset, buflen - offset) ;
	    offset += nl ;
	    if (confidence < params->minimumScore())
	       {
	       // if we extracted using a multi-byte charset that
	       //   enforces alignment, back up to allow for erroneous
	       //   inclusion of the first byte of the next string
	       if (len > 1 && charset && charset->alignment() > 1 && nl == 0)
		  {
		  offset-- ;
		  }
	       }
	    skipped = 0 ;
	    extracted_strings++ ;
	    continue ;
	    }
	 //else 
	 if (len < 0)
	    {
	    offset += (-len) ;
	    skipped += (-len) ;
	    }
	 else // if (len == 0)
	    {
	    offset++ ;		// try scanning starting at next byte
	    skipped++ ;
	    }
	 // if we extract two or more strings and then find a substantial
	 //   stretch of non-strings, re-identify the encoding
	 if (skipped > 20 && automatic_charsets &&
	     extracted_strings > 1 && offset > SCAN_SIZE / 4)
	    {
	    decay_language_scores() ;
	    break ;
	    }
	 }
      }
   if (automatic_charsets)
      {
      FrFree(charsets) ;
      charsets = 0 ;
      }
   const LanguageIdentifier *ident = params->languageIdentifier() ;
   if (!given_langscores)
      {
      if (ident)
	 ident->freeScores(langscores) ;
      else
	 delete langscores ;
      }
   if (!given_charset_scores)
      {
      if (ident)
	 ident = ident->charsetIdentifier() ;
      if (ident)
	 ident->freeScores(charset_scores) ;
      else
	 delete charset_scores ;
      }
   return ;
}

//----------------------------------------------------------------------

static bool is_char_device(FILE *fp)
{
   struct stat statbuffer ;

   if (fstat(fileno(fp),&statbuffer) != 0)
      return false ;
   return S_ISCHR(statbuffer.st_mode) ;
}

//----------------------------------------------------------------------

static void extract_text(FILE *fp, const char *filename,
			 const CharacterSet * const *charsets,
			 const ExtractParameters *params,
			 bool verbose)
{
   uint64_t end_offset ;
   if (isatty(fileno(fp)) || is_char_device(fp))
      end_offset = (uint64_t)~0 ;
   else
      end_offset = filesize(fp) ;
   if (params->endOffset() < end_offset)
      end_offset = params->endOffset() ;
   if (end_offset >= params->startOffset() &&
       (params->startOffset() == 0 ||
	fseek(fp, params->startOffset(), SEEK_SET) == 0))
      {
      FILE *outfp = stdout ;
      if (params->separateOutputs())
	 {
	 const char *outdir = params->outputDirectory() ;
	 const char *basename = FrFileBasename(filename) ;
	 (void)FrCreatePath(outdir) ;
	 char *outname = Fr_aprintf("%s/%s.strings",outdir,basename) ;
	 outfp = fopen(outname,"w") ;
	 set_binary_mode(outfp,params) ;
	 if (!outfp)
	    {
	    fprintf(stderr,"Unable to open '%s' for writing\n",outname) ;
	    if (strcmp(outdir,".") != 0)
	       {
	       FrFree(outname) ;
	       return ;
	       }
	    outfp = stdout ;
	    }
	 FrFree(outname) ;
	 }
      InputStreamFile instream(fp) ;
      extract_text(&instream,outfp,filename,end_offset,charsets,params,
		   verbose) ;
      if (outfp != stdout)
	 fclose(outfp) ;
      }
   return ;
}

//----------------------------------------------------------------------

void extract_text(const CharacterSet * const *charsets,
		  const ExtractParameters *params,
		  bool verbose, int argc, const char **argv)
{
   if (!params->separateOutputs())
      {
      set_binary_mode(stdout,params) ;
      }
   if (argc == 1)
      {
      // process standard input
      if (verbose)
	 cerr << "**** Extracting text from standard input" << endl ;
      extract_text(stdin,"standard input",charsets,params,verbose) ;
      return ;
      }
   while (argc > 1)
      {
      const char *filename = argv[1] ;
      if (filename && *filename)
	 {
	 FILE *fp ;
	 if (is_stdin(filename))
	    {
	    filename = "standard input" ;
	    fp = stdin ;
	    }
	 else
	    fp = fopen(filename,"rb") ;
	 if (!fp)
	    {
	    cerr << "**** Unable to open " << filename << " ****" << endl ;
	    }
	 else
	    {
	    if (verbose)
	       cerr << "**** Extracting text from file " << filename << endl ;
	    extract_text(fp,filename,charsets,params,verbose) ;
	    }
	 }
      argc-- ;
      argv++ ;
      }
   return ;
}

// end of file extract.C //
