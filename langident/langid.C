/************************************************************************/
/*                                                                      */
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     langid.C							*/
/*  Version:  1.24							*/
/*  LastEdit: 14mar2014							*/
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

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <float.h>
#include <stdint.h>
#include "langid.h"
#include "mtrie.h"
#include "ptrie.h"
#include "FramepaC.h"

/************************************************************************/
/*	Manifest Constants						*/
/************************************************************************/

#ifdef __GNUC__
# define likely(x) __builtin_expect((x),1)
# ifndef unlikely
#   define unlikely(x) __builtin_expect((x),0)
# endif /* !unlikely */
#else
# define likely(x) (x)
# define unlikely(x) (x)
#endif

#ifndef UINT32_MAX
# define UINT32_MAX		0xFFFFFFFFU
#endif

/************************************************************************/
/*	Types								*/
/************************************************************************/

class ScoreAndID
   {
   private:
      double   m_score ;
      unsigned short m_id ;
   public:
      ScoreAndID() {}
      void init(double score, unsigned short id)
	 { m_score = score ; m_id = id ; }

      // accessors
      double score() const { return m_score ; }
      unsigned short id() const { return m_id ; }

      // manipulators
      static void swap(ScoreAndID &, ScoreAndID &) ;

      // comparison
      static int compare(const ScoreAndID &, const ScoreAndID &) ;
   } ;

typedef unsigned char LONG64buffer[8] ;

/************************************************************************/
/*	Global variables						*/
/************************************************************************/

FrAllocator LanguageID::allocator("LanguageID",sizeof(LanguageID)) ;

FrAllocator LanguageScores::allocator("LanguageScores",
				      sizeof(LanguageScores)) ;

FrAllocator WeightedLanguageScores::allocator("WtLanguageScores",
					      sizeof(WeightedLanguageScores)) ;

//static double stop_gram_penalty = -15.0 ;
static double stop_gram_penalty = -9.0 ;

/************************************************************************/
/*	Helper functions						*/
/************************************************************************/

static uint8_t read_byte(FILE *fp, uint32_t default_value = (uint8_t)~0)
{
   uint8_t valbuf ;
   if (fread(&valbuf,sizeof(valbuf),1,fp) != 1)
      return default_value ;
   return valbuf ;
}

//----------------------------------------------------------------------

static bool write_uint8(FILE *fp, uint8_t value)
{
   uint8_t valbuf = value ;
   return fwrite(&valbuf,sizeof(valbuf),1,fp) == 1 ;
}

//----------------------------------------------------------------------

static uint32_t read_uint32(FILE *fp, uint32_t default_value = (uint32_t)~0)
{
   LONGbuffer valbuf ;
   if (fread(valbuf,sizeof(valbuf),1,fp) != 1)
      return default_value ;
   return FrLoadLong(valbuf) ;
}

//----------------------------------------------------------------------

static bool write_uint32(FILE *fp, uint32_t value)
{
   LONGbuffer valbuf ;
   FrStoreLong(value,valbuf) ;
   return fwrite(valbuf,sizeof(valbuf),1,fp) == 1 ;
}

//----------------------------------------------------------------------

static char *read_fixed_field(FILE *fp, size_t len)
{
   if (len == 0)
      return 0 ;
   char *buf = FrNewN(char,len) ;
   if (buf)
      {
      if (fread(buf,sizeof(char),len,fp) < len)
	 {
	 delete buf ;
	 buf = 0 ;
	 }
      else
	 {
	 // ensure proper string termination if the file didn't have a NUL
	 buf[len-1] = '\0' ;
	 }
      }
   return buf ;
}

//----------------------------------------------------------------------

static bool read_uint64(FILE *fp, uint64_t &value)
{
   char buf[sizeof(uint64_t)] ;
   if (fread(buf,sizeof(buf),1,fp) < 1)
      {
      value = 0 ;
      return false ;
      }
   else
      {
      value = FrLoad64(buf) ;
      return true ;
      }
}

//----------------------------------------------------------------------

static bool write_fixed_field(FILE *fp, const char *s, size_t len)
{
   size_t string_len = s ? strlen(s) : 0 ;
   for (size_t i = 0 ; i+1 < len && i < string_len ; i++)
      {
      if (fputc(s[i],fp) == EOF)
	 return false ;
      }
   for (size_t i = string_len ; i+1 < len ; i++)
      {
      if (fputc('\0',fp) == EOF)
	 return false ;
      }
   return fputc('\0',fp) != EOF ;
}

//----------------------------------------------------------------------

static bool write_uint64(FILE *fp, uint64_t value)
{
   char buf[sizeof(uint64_t)] ;
   FrStore64(value,buf) ;
   return fwrite(buf,sizeof(buf),1,fp) == 1 ;
}

//----------------------------------------------------------------------

static void parse_language_description(const char *descript,
				       char *&language,
				       char *&region,
				       char *&encoding,
				       char *&source)
{
   // the format of a desciptor is lang_REG-encoding/source
   language = 0 ;
   region = 0 ;
   encoding = 0 ;
   source = 0 ;
   if (descript && *descript)
      {
      const char *underscore = strchr(descript,'_') ;
      const char *dash = strchr(descript,'-') ;
      const char *slash = strchr(descript,'/') ;
      const char *nul = strchr(descript,'\0') ;
      const char *lang_end = underscore ;
      if (!lang_end || (dash && dash < lang_end)) lang_end = dash ;
      if (!lang_end || (slash && slash < lang_end)) lang_end = slash ;
      if (!lang_end) lang_end = nul ;
      language = FrNewN(char,lang_end - descript + 1) ;
      if (language)
	 {
	 memcpy(language,descript,lang_end - descript) ;
	 language[lang_end - descript] = '\0' ;
	 }
      if (underscore)
	 {
	 // we have a region specified
	 const char *reg_end = dash ;
	 if (!reg_end || (slash && slash < reg_end)) reg_end = slash ;
	 if (!reg_end) reg_end = nul ;
	 region = FrNewN(char,reg_end - underscore) ;
	 if (region)
	    {
	    memcpy(region,underscore+1,reg_end - underscore) ;
	    region[reg_end - (underscore + 1)] = '\0' ;
	    }
	 }
      if (dash)
	 {
	 // we have an encoding specified
	 const char *enc_end = slash ;
	 if (!enc_end) enc_end = nul ;
	 encoding = FrNewN(char,enc_end - dash) ;
	 if (encoding)
	    {
	    memcpy(encoding,dash+1,enc_end - dash) ;
	    encoding[enc_end - (dash + 1)] = '\0' ;
	    }
	 }
      if (slash)
	 {
	 // we have a source specified
	 source = FrNewN(char,nul - slash) ;
	 if (source)
	    {
	    memcpy(source,slash+1,nul - slash) ;
	    source[nul - (slash + 1)] = '\0' ;
	    }
	 }
      }
   return ;
}

//----------------------------------------------------------------------

static double length_factor(unsigned len)
{
//   return 400.0 * ::pow(len,0.20) ;
   return 270.0 * ::pow(len,0.75) ;
}

//----------------------------------------------------------------------

static double *make_length_factors(unsigned max_length,
				   double bigram_weight)
{
   if (max_length < 3)
      max_length = 3 ;
   double *factors = FrNewN(double,max_length+1) ;
   if (factors)
      {
      factors[0] = 0.0 ;
      factors[1] = 1.0 ;
      factors[2] = bigram_weight * length_factor(2) ;
      for (size_t i = 3 ; i <= max_length ; i++)
	 {
	 factors[i] = length_factor(i) ;
	 }
      }
   return factors ;
}

//----------------------------------------------------------------------

static void free_length_factors(double *length_factors)
{
   FrFree(length_factors) ;
   return ;
}

//----------------------------------------------------------------------

static double scale_score(uint32_t score)
{
//   double scaled = ::sqrt(::sqrt(score / (100.0 * TRIE_SCALE_FACTOR))) ;
   // smoothing is now precomputed in the language model database
   double scaled = 1.0 ;
   if (score & 1)
      {
      scaled = stop_gram_penalty ;
      score &= ~1 ;
      }
   return scaled * score / (100.0 * TRIE_SCALE_FACTOR) ;
}

/************************************************************************/
/*	Methods for class ScoreAndID					*/
/************************************************************************/

int ScoreAndID::compare(const ScoreAndID &s1, const ScoreAndID &s2)
{
   if (s1.score() < s2.score())
      return +1 ;
   else if (s1.score() > s2.score())
      return -1 ;
   else
      return 0 ;
}

//----------------------------------------------------------------------

void ScoreAndID::swap(ScoreAndID &s1, ScoreAndID &s2)
{
   double tmpscore = s1.score() ;
   s1.m_score = s2.score() ;
   s2.m_score = tmpscore ;
   unsigned tmpid = s1.id() ;
   s1.m_id = s2.id() ;
   s2.m_id = tmpid ;
   return ;
}

/************************************************************************/
/*	Methods for class BigramCounts					*/
/************************************************************************/

BigramCounts::BigramCounts(const BigramCounts *orig)
{
   copy(orig) ;
   return ;
}

//----------------------------------------------------------------------

BigramCounts::BigramCounts(FILE *fp)
{
   if (!fp || fread(m_counts,1,sizeof(m_counts),fp) < sizeof(m_counts))
      {
      // we could not read the data, so clear all the counts, effectively
      //   removing this bigram model from consideration
      memset(m_counts,'\0',sizeof(m_counts)) ;
      }
   m_total = 0 ;
   for (size_t i = 0 ; i < lengthof(m_counts) ; i++)
      {
      m_total += m_counts[i] ;
      }
   return ;
}

//----------------------------------------------------------------------

void BigramCounts::copy(const BigramCounts *orig)
{
   if (orig)
      {
      for (size_t i = 0 ; i < lengthof(m_counts) ; i++)
	 m_counts[i] = orig->m_counts[i] ;
      m_total = orig->m_total ;
      }
   else
      {
      for (size_t i = 0 ; i < lengthof(m_counts) ; i++)
	 m_counts[i] = 0 ;
      m_total = 0 ;
      }
   return ;
}

//----------------------------------------------------------------------

double BigramCounts::averageProbability(const char *buffer, size_t buflen)
const
{
   double prob = 0.0 ;
   if (buffer && buflen > 1)
      {
      for (size_t i = 1 ; i < buflen ; i++)
	 {
	 prob += this->count(buffer[i-1],buffer[i]) ;
	 }
      prob /= m_total ;
      prob /= (buflen - 1) ;
      }
   return prob ;
}

//----------------------------------------------------------------------

BigramCounts *BigramCounts::load(FILE *fp)
{
   if (fp)
      {
      BigramCounts *model = new BigramCounts ;
      if (model->read(fp))
	 return model ;
      delete model ;
      }
   return 0 ;
}

//----------------------------------------------------------------------

bool BigramCounts::read(FILE *fp)
{
   memset(m_counts,'\0',sizeof(m_counts)) ;
   m_total = 0 ;
   for (size_t c1 = 0 ; c1 <= 0xFF ; c1++)
      {
      for (size_t c2 = 0 ; c2 <= 0xFF ; c2++)
	 {
	 char line[FrMAX_LINE] ;
	 if (!fgets(line,sizeof(line),fp))
	    return false ;
	 char *end ;
	 char *lineptr = line ;
	 FrSkipWhitespace(lineptr) ;
	 unsigned long count = strtoul(lineptr,&end,0) ;
	 if (end != lineptr)
	    {
	    m_total += count ;
	    set(c1,c2,count) ;
	    }
	 else
	    return false ;
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

bool BigramCounts::readBinary(FILE *fp)
{
   if (fp)
      {
      return (fread(m_counts,sizeof(m_counts[0]),lengthof(m_counts),fp)
	      == lengthof(m_counts)) ;
      }
   return false ;
}

//----------------------------------------------------------------------

bool BigramCounts::dumpCounts(FILE *fp) const
{
   if (!fp)
      return false ;
   for (size_t c1 = 0 ; c1 <= 0xFF ; c1++)
      {
      for (size_t c2 = 0 ; c2 <= 0xFF ; c2++)
	 {
	 fprintf(fp,"%lu\n",(unsigned long)count(c1,c2)) ;
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

bool BigramCounts::save(FILE *fp) const
{
   if (fp)
      {
      return fwrite(m_counts,1,sizeof(m_counts),fp) == sizeof(m_counts) ;
      }
   return false ;
}

/************************************************************************/
/*	Methods for class LanguageID					*/
/************************************************************************/

LanguageID::LanguageID()
{
   clear() ;
   return ;
}

//----------------------------------------------------------------------

LanguageID::LanguageID(const char *lang, const char *reg, const char *enc,
		       const char *source, const char *scr)
{
   clear() ;
   setLanguage(lang) ;
   setRegion(reg) ;
   setEncoding(enc) ;
   setSource(source) ;
   setScript(scr) ;
   return ;
}

//----------------------------------------------------------------------

LanguageID::LanguageID(const LanguageID &orig)
{
   clear() ;
   setLanguage(orig.language(),orig.friendlyName()) ;
   setRegion(orig.region()) ;
   setEncoding(orig.encoding()) ;
   setSource(orig.source()) ;
   setScript(orig.script()) ;
   setAlignment(orig.alignment()) ;
   setCoverageFactor(orig.coverageFactor()) ;
   setCountedCoverage(orig.countedCoverage()) ;
   setFreqCoverage(orig.freqCoverage()) ;
   setMatchFactor(orig.matchFactor()) ;
   return ;
}

//----------------------------------------------------------------------

LanguageID::LanguageID(const LanguageID *orig)
{
   clear() ;
   if (orig)
      {
      setLanguage(orig->language(),orig->friendlyName()) ;
      setRegion(orig->region()) ;
      setEncoding(orig->encoding()) ;
      setSource(orig->source()) ;
      setScript(orig->script()) ;
      setAlignment(orig->alignment()) ;
      setCoverageFactor(orig->coverageFactor()) ;
      setCountedCoverage(orig->countedCoverage()) ;
      setFreqCoverage(orig->freqCoverage()) ;
      setMatchFactor(orig->matchFactor()) ;
      }
   return ;
}

//----------------------------------------------------------------------

LanguageID::~LanguageID()
{
   setLanguage(0) ;
   setRegion(0) ;
   setEncoding(0) ;
   setSource(0) ;
   setScript(0) ;
   return ;
}

//----------------------------------------------------------------------

void LanguageID::clear()
{
   m_language = 0 ;
   m_friendlyname = 0 ;
   m_region = 0 ;
   m_encoding = 0 ;
   m_source = 0 ;
   m_script = 0 ;
   m_trainbytes = 0 ;
   m_alignment = 1 ;
   m_coverage = 0.0 ;
   m_countcover = 0.0 ;
   m_freqcover = 0.0 ;
   m_matchfactor = 0.0 ;
   return ;
}

//----------------------------------------------------------------------

void LanguageID::setLanguage(const char *lang, const char *friendly)
{
   FrFree(m_language) ;
   if (lang && friendly && lang != friendly)
      m_language = Fr_aprintf("%s=%s",lang,friendly) ;
   else
      m_language = FrDupString(lang) ;
   m_friendlyname = m_language ;
   if (m_language)
      {
      char *eq = strchr(m_language,'=') ;
      if (eq)
	 {
	 m_friendlyname = eq + 1 ;
	 *eq = '\0' ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

void LanguageID::setRegion(const char *region)
{
   FrFree(m_region) ;
   m_region = FrDupString(region) ;
   return ;
}

//----------------------------------------------------------------------

void LanguageID::setEncoding(const char *encoding)
{
   FrFree(m_encoding) ;
   m_encoding = FrDupString(encoding) ;
   return ;
}

//----------------------------------------------------------------------

void LanguageID::setSource(const char *source)
{
   FrFree(m_source) ;
   m_source = FrDupString(source) ;
   return ;
}

//----------------------------------------------------------------------

void LanguageID::setScript(const char *scr)
{
   FrFree(m_script) ;
   m_script = FrDupString(scr) ;
   return ;
}

//----------------------------------------------------------------------

void LanguageID::setAlignment(const char *align)
{
   int a = align ? atoi(align) : 1 ;
   if (a < 1)
      a = 1 ;
   else if (a > 4)
      a = 4 ;
   else if (a == 3)
      a = 2 ;
   setAlignment(a) ;
   return ;
}

//----------------------------------------------------------------------

void LanguageID::setCoverageFactor(double coverage)
{
   if (coverage <= 0.0 || coverage > 1.0)
      coverage = 1.0 ;
   m_coverage = coverage ;
   return ;
}

//----------------------------------------------------------------------

void LanguageID::setCountedCoverage(double coverage)
{
   if (coverage < 0.0)
      coverage = 0.0 ;
   if (coverage > MAX_WEIGHTED_COVER)
      coverage = MAX_WEIGHTED_COVER ;
   m_countcover = coverage ;
   return ;
}

//----------------------------------------------------------------------

void LanguageID::setFreqCoverage(double coverage)
{
   if (coverage < 0.0)
      coverage = 0.0 ;
   if (coverage > MAX_FREQ_COVER)
      coverage = MAX_FREQ_COVER ;
   m_freqcover = coverage ;
   return ;
}

//----------------------------------------------------------------------

void LanguageID::setMatchFactor(double match)
{
   if (match < 0.0)
      match = 0.0 ;
   if (match > MAX_MATCH_FACTOR)
      match = MAX_MATCH_FACTOR ;
   m_matchfactor = match ;
   return ;
}

//----------------------------------------------------------------------

LanguageID *LanguageID::read(FILE *fp, unsigned file_version)
{
   if (!fp)
      return 0 ;
   LanguageID *langID = new LanguageID() ;
   if (!read(fp,langID,file_version))
      {
      delete langID ;
      langID = 0 ;
      }
   return langID ;
}

//----------------------------------------------------------------------

bool LanguageID::sameLanguage(const LanguageID &info, bool ignore_region) const
{
   const char *info_1, *info_2 ;
   info_1 = language() ;
   info_2 = info.language() ;
   if (info_1 != info_2 && (!info_1 || !info_2 || strcmp(info_1,info_2) != 0))
      return false ;
   if (!ignore_region)
      {
      info_1 = region() ;
      info_2 = info.region() ;
      if (info_1 != info_2 &&
	  (!info_1 || !info_2 || strcmp(info_1,info_2) != 0))
	 return false ;
      }
   info_1 = encoding() ;
   info_2 = info.encoding() ;
   if (info_1 != info_2 && (!info_1 || !info_2 || strcmp(info_1,info_2) != 0))
      return false ;
   return true ;
}

//----------------------------------------------------------------------

bool LanguageID::matches(const LanguageID *lang_info) const
{
   if (!lang_info)
      return false ;			// MUST specify a language
   if (strcasecmp(language(),lang_info->language()) != 0)
      return false ;
   const char *reg = lang_info->region() ;
   if (reg && *reg && region() && strcasecmp(region(),reg) != 0)
      return false ;
   const char *enc = lang_info->encoding() ;
   if (enc && *enc && encoding() && strcasecmp(encoding(),enc) != 0)
      return false ;
   const char *src = lang_info->source() ;
   if (src && *src && source() && strcasecmp(source(),src) != 0)
      return false ;
   return true ;
}

//----------------------------------------------------------------------

bool LanguageID::matches(const char *lang, const char *reg,
			 const char *enc, const char *src) const
{
   if (!lang || !*lang)
      return false ;			// MUST specify a language
   if (strcasecmp(language(),lang) != 0)
      return false ;
   if (reg && *reg && region() && strcasecmp(region(),reg) != 0)
      return false ;
   if (enc && *enc && encoding() && strcasecmp(encoding(),enc) != 0)
      return false ;
   if (src && *src && source() && strcasecmp(source(),src) != 0)
      return false ;
   return true ;
}

//----------------------------------------------------------------------

bool LanguageID::operator == (const LanguageID &info) const
{
   const char *info_1, *info_2 ;
   info_1 = language() ;
   info_2 = info.language() ;
   if (info_1 != info_2 && (!info_1 || !info_2 || strcmp(info_1,info_2) != 0))
      return false ;
   info_1 = region() ;
   info_2 = info.region() ;
   if (info_1 != info_2 && (!info_1 || !info_2 || strcmp(info_1,info_2) != 0))
      return false ;
   info_1 = encoding() ;
   info_2 = info.encoding() ;
   if (info_1 != info_2 && (!info_1 || !info_2 || strcmp(info_1,info_2) != 0))
      return false ;
   info_1 = source() ;
   info_2 = info.source() ;
   if (info_1 != info_2 && (!info_1 || !info_2 || strcmp(info_1,info_2) != 0))
      return false ;
   return true ;
}

//----------------------------------------------------------------------

bool LanguageID::read(FILE *fp, LanguageID *langID, unsigned version)
{
   if (!langID)
      return false ;
   langID->m_language = read_fixed_field(fp,LANGID_STRING_LENGTH) ;
   langID->m_region = read_fixed_field(fp,LANGID_STRING_LENGTH) ;
   langID->m_encoding = read_fixed_field(fp,LANGID_STRING_LENGTH) ;
   langID->m_source = read_fixed_field(fp,LANGID_STRING_LENGTH) ;
   langID->m_script = read_fixed_field(fp,LANGID_STRING_LENGTH) ;
   (void)read_uint64(fp,langID->m_trainbytes ) ;
   uint8_t align = read_byte(fp, 1) ;
   if (align < 1) align = 1 ;
   (void)read_byte(fp,0) ;
   (void)read_byte(fp,0) ;
   (void)read_byte(fp,0) ;
   uint32_t cover = 0 ;
   cover = read_uint32(fp,0) ;
   double cover_factor = ((double)cover) / (double)UINT32_MAX ;
   langID->setCoverageFactor(cover_factor) ;
   if (version > 4)
      {
      cover = read_uint32(fp,0) ;
      langID->setCountedCoverage(cover * MAX_WEIGHTED_COVER / UINT32_MAX ) ;
      cover = read_uint32(fp,0) ;
      langID->setFreqCoverage(cover * MAX_FREQ_COVER / UINT32_MAX ) ;
      cover = read_uint32(fp,0) ;
      langID->setMatchFactor(cover * MAX_MATCH_FACTOR / UINT32_MAX ) ;
      }
   langID->m_friendlyname = langID->m_language ;
   langID->setAlignment(align) ;
   if (langID->m_language)
      {
      char *eq = strchr(langID->m_language,'=') ;
      if (eq)
	 {
	 langID->m_friendlyname = eq + 1 ;
	 *eq = '\0' ;
	 }
      }
   return langID->m_language != 0 && langID->m_encoding != 0 ;
}

//----------------------------------------------------------------------

bool LanguageID::write(FILE *fp) const
{
   if (!fp)
      return false ;
   bool success = false ;
   if (m_friendlyname && m_friendlyname != m_language && 
       m_friendlyname > m_language)
      {
      ((char*)m_friendlyname)[-1] = '=' ;
      }
   double count_cover = m_countcover / MAX_WEIGHTED_COVER ;
   double freq_cover = m_freqcover / MAX_FREQ_COVER ;
   double match_factor = matchFactor() / MAX_MATCH_FACTOR ;
   // to simplify the version 1-5 file formats, we'll use fixed-size fields
   if (write_fixed_field(fp,m_language,LANGID_STRING_LENGTH) &&
       write_fixed_field(fp,m_region,LANGID_STRING_LENGTH) &&
       write_fixed_field(fp,m_encoding,LANGID_STRING_LENGTH) &&
       write_fixed_field(fp,m_source,LANGID_STRING_LENGTH) &&
       write_fixed_field(fp,m_script,LANGID_STRING_LENGTH) &&
       write_uint64(fp,m_trainbytes) &&
       write_uint8(fp,m_alignment) &&
       write_uint8(fp,0) &&
       write_uint8(fp,0) &&
       write_uint8(fp,0) &&
       write_uint32(fp,(uint32_t)(m_coverage * UINT32_MAX) ) &&
       write_uint32(fp,(uint32_t)(count_cover * UINT32_MAX) ) &&
       write_uint32(fp,(uint32_t)(freq_cover * UINT32_MAX) ) &&
       write_uint32(fp,(uint32_t)(match_factor * UINT32_MAX) ))
      success = true ;
   if (m_friendlyname && m_friendlyname != m_language && 
       m_friendlyname > m_language)
      ((char*)m_friendlyname)[-1] = '\0' ;
   return success ;
}

/************************************************************************/
/*	Methods for class LanguageScores				*/
/************************************************************************/

LanguageScores::LanguageScores(size_t num_languages)
{
   char *buffer = FrNewN(char,num_languages * (sizeof(double)+sizeof(unsigned short))) ;
   m_sorted = false ;
   m_userdata = 0 ;
   m_active_language = 0 ;
   if (buffer)
      {
      m_scores = (double*)buffer ;
      m_lang_ids = (unsigned short*)(m_scores + num_languages) ;
      m_num_languages = num_languages ;
      m_max_languages = num_languages ;
      for (size_t i = 0 ; i < num_languages ; i++)
	 {
	 m_scores[i] = 0.0 ;
	 m_lang_ids[i] = (unsigned short)i ;
	 }
      }
   else
      {
      m_scores = 0 ;
      m_lang_ids = 0 ;
      m_num_languages = 0 ;
      m_max_languages = 0 ;
      }
   return ;
}

//----------------------------------------------------------------------

LanguageScores::LanguageScores(const LanguageScores *orig)
{
   if (orig)
      {
      unsigned nlang = orig->numLanguages() ;
      m_num_languages = nlang ;
      char *buffer = FrNewN(char,nlang * (sizeof(double)+sizeof(unsigned short))) ;
      if (buffer)
	 {
	 m_scores = (double*)buffer ;
	 m_lang_ids = (unsigned short*)(m_scores + nlang) ;
	 m_sorted = orig->m_sorted ;
	 for (size_t i = 0 ; i < nlang ; i++)
	    {
	    m_scores[i] = orig->score(i) ;
	    m_lang_ids[i] = (unsigned short)orig->languageNumber(i) ;
	    }
	 }
      else
	 {
	 m_scores = 0 ;
	 m_lang_ids = 0 ;
	 m_num_languages = 0 ;
	 }
      }
   else
      {
      m_num_languages = 0 ;
      m_scores = 0 ;
      m_lang_ids = 0 ;
      }
   return ;
}

//----------------------------------------------------------------------

LanguageScores::LanguageScores(const LanguageScores *orig, double scale)
{
   if (orig)
      {
      unsigned nlang = orig->numLanguages() ;
      m_num_languages = nlang ;
      char *buffer = FrNewN(char,nlang * (sizeof(double)+sizeof(unsigned short))) ;
      if (buffer)
	 {
	 m_scores = (double*)buffer ;
	 m_lang_ids = (unsigned short*)(m_scores + nlang) ;
	 m_sorted = orig->m_sorted ;
	 for (size_t i = 0 ; i < nlang ; i++)
	    {
	    m_scores[i] = orig->score(i) * scale ;
	    m_lang_ids[i] = (unsigned short)orig->languageNumber(i) ;
	    }
	 }
      else
	 {
	 m_scores = 0 ;
	 m_lang_ids = 0 ;
	 m_num_languages = 0 ;
	 }
      }
   else
      {
      m_num_languages = 0 ;
      m_scores = 0 ;
      m_lang_ids = 0 ;
      }
   return ;
}

//----------------------------------------------------------------------

LanguageScores::~LanguageScores()
{
   FrFree(m_scores) ;
   m_scores = 0 ;
   m_lang_ids = 0 ;
   m_num_languages = 0 ;
   m_sorted = false ;
   return ;
}

//----------------------------------------------------------------------

double LanguageScores::highestScore() const
{
   if (sorted())
      {
      return m_scores[0] ;
      }
   else
      {
      double highest = 0.0 ;
      size_t nlang = numLanguages() ;
      for (size_t i = 0 ; i < nlang ; i++)
	 {
	 if (m_scores[i] > highest)
	    highest = m_scores[i] ;
	 }
      return highest ;
      }
}

//----------------------------------------------------------------------

unsigned LanguageScores::highestLangID() const
{
   if (sorted())
      {
      return m_lang_ids[0] ;
      }
   else
      {
      double highest = 0.0 ;
      unsigned langid = (unsigned)~0 ;
      for (size_t i = 0 ; i < numLanguages() ; i++)
	 {
	 if (m_scores[i] > highest)
	    {
	    highest = m_scores[i] ;
	    langid = i ;
	    }
	 }
      return langid ;
      }
}

//----------------------------------------------------------------------

unsigned LanguageScores::nonzeroScores() const
{
   if (sorted())
      {
      return (m_scores[0] > LANGID_ZERO_SCORE) ? numLanguages() : 0 ;
      }
   else
      {
      size_t count = 0 ;
      for (size_t i = 0 ; i < numLanguages() ; i++)
	 {
	 if (m_scores[i] > LANGID_ZERO_SCORE)
	    count++ ;
	 }
      return count ;
      }
}

//----------------------------------------------------------------------

void LanguageScores::clear()
{
   m_num_languages = maxLanguages() ;
   for (size_t i = 0 ; i < numLanguages() ; i++)
      {
      m_scores[i] = 0.0 ;
      }
   m_sorted = false ;
   return ;
}

//----------------------------------------------------------------------

void LanguageScores::scaleScores(double scale_factor)
{
   for (size_t i = 0 ; i < numLanguages() ; i++)
      {
      m_scores[i] *= scale_factor ;
      }
   return ;
}

//----------------------------------------------------------------------

void LanguageScores::sqrtScores()
{
   for (size_t i = 0 ; i < numLanguages() ; i++)
      {
      m_scores[i] = ::sqrt(m_scores[i]) ;
      }
   return ;
}

//----------------------------------------------------------------------

void LanguageScores::add(const LanguageScores *scores, double weight)
{
   if (scores && weight != 0)
      {
      size_t count = numLanguages() ;
      if (scores->numLanguages() < count)
	 count = scores->numLanguages() ;
      for (size_t i = 0 ; i < count ; i++)
	 {
	 m_scores[i] += (scores->m_scores[i] * weight) ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

void LanguageScores::addThresholded(const LanguageScores *scores,
				    double threshold, double weight)
{
   if (scores && weight != 0)
      {
      size_t count = numLanguages() ;
      if (scores->numLanguages() < count)
	 count = scores->numLanguages() ;
      for (size_t i = 0 ; i < count ; i++)
	 {
	 double sc = scores->m_scores[i] ;
	 if (sc >= threshold)
	    m_scores[i] += (sc * weight) ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

void LanguageScores::subtract(const LanguageScores *scores, double weight)
{
   if (scores && weight != 0)
      {
      size_t count = numLanguages() ;
      if (scores->numLanguages() < count)
	 count = scores->numLanguages() ;
      for (size_t i = 0 ; i < count ; i++)
	 {
	 m_scores[i] -= (scores->m_scores[i] * weight) ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

bool LanguageScores::lambdaCombineWithPrior(LanguageScores *prior, double lambda,
					    double smoothing)
{
   size_t count = numLanguages() ;
   if (prior && prior->numLanguages())
      {
      for (size_t i = 0 ; i < count ; i++)
	 {
	 double priorscore = prior->m_scores[i] ;
	 double currscore = m_scores[i] ;
	 if (currscore >= LANGID_ZERO_SCORE)
	    prior->m_scores[i] += currscore * smoothing ;
	 m_scores[i] = lambda * currscore + (1.0 - lambda) * priorscore ;
	 }
      return true ;
      }
   return false ;
}

//----------------------------------------------------------------------

void LanguageScores::sort(double cutoff_ratio)
{
   if (!sorted() && numLanguages() > 0)
      {
      ScoreAndID *scores_and_ids = new ScoreAndID[numLanguages()] ;
      if (!scores_and_ids)
	 {
	 //!!!
	 return ;
	 }
      double cutoff = LANGID_ZERO_SCORE ;
      if (cutoff_ratio > 0.0)
	 {
	 if (cutoff_ratio > 1.0)
	    cutoff_ratio = 1.0 ;
	 double threshold = highestScore() * cutoff_ratio ;
	 if (threshold > cutoff)
	    cutoff = threshold ;
	 }
      unsigned num_scores = 0 ;
      for (unsigned i = 0 ; i < numLanguages() ; i++)
	 {
	 if (m_scores[i] >= cutoff)
	    scores_and_ids[num_scores++].init(m_scores[i],m_lang_ids[i]) ;
	 }
      if (num_scores > 0)
	 {
	 if (num_scores > 1)
	    {
	    FrQuickSort(scores_and_ids,num_scores) ;
//            FrSmoothSort(scores_and_ids,numLanguages()) ;
	    }
	 for (unsigned i = 0 ; i < num_scores ; i++)
	    {
	    m_scores[i] = scores_and_ids[i].score() ;
	    m_lang_ids[i] = scores_and_ids[i].id() ;
	    }
	 m_num_languages = num_scores ;
	 }
      else
	 {
	 // nothing is above our cutoff, but we can't just discard everything,
	 //   so scan for the highest score and make that the sole score
	 for (unsigned i = 1 ; i < numLanguages() ; i++)
	    {
	    if (m_scores[i] > m_scores[0])
	       {
	       m_scores[0] = m_scores[i] ;
	       m_lang_ids[0] = m_lang_ids[i] ;
	       }
	    }
	 m_num_languages = 1 ;
	 }
      delete [] scores_and_ids ;
      m_sorted = true ;
      }
   return ;
}

//----------------------------------------------------------------------

static void insert(double score, unsigned lang_id,
		   ScoreAndID *scores_and_ids,
		   unsigned &num_scores,
		   unsigned max_scores)
{
   for (size_t i = 0 ; i < num_scores ; i++)
      {
      if (score > scores_and_ids[i].score())
	 {
	 if (num_scores < max_scores)
	    num_scores++ ;
	 for (size_t j = num_scores - 1 ; j > i ; j--)
	    {
	    scores_and_ids[j] = scores_and_ids[j-1] ;
	    }
	 scores_and_ids[i].init(score,lang_id) ;
	 return ;
	 }
      }
   if (num_scores == 0)
      {
      scores_and_ids[0].init(score,lang_id) ;
      num_scores++ ;
      }
   return ;
}

//----------------------------------------------------------------------

void LanguageScores::sort(double cutoff_ratio, unsigned max_langs)
{
   if (max_langs == 0 || max_langs > 10 || max_langs >= numLanguages())
      sort(cutoff_ratio) ;
   else if (!sorted() && numLanguages() > 0)
      {
      ScoreAndID *scores_and_ids = new ScoreAndID[max_langs] ;
      if (!scores_and_ids)
	 {
	 //!!!
	 return ;
	 }
      double cutoff = LANGID_ZERO_SCORE ;
      unsigned num_scores = 0 ;
      for (unsigned i = 0 ; i < numLanguages() ; i++)
	 {
	 if (m_scores[i] >= cutoff)
	    {
	    insert(m_scores[i],m_lang_ids[i],scores_and_ids,
		   num_scores,max_langs) ;
	    double threshold = scores_and_ids[0].score() * cutoff_ratio ;
	    if (threshold > cutoff)
	       {
	       cutoff = threshold ;
	       }
	    }
	 }
      if (num_scores > 0)
	 {
	 for (unsigned i = 0 ; i < num_scores ; i++)
	    {
	    m_scores[i] = scores_and_ids[i].score() ;
	    m_lang_ids[i] = scores_and_ids[i].id() ;
	    }
	 m_num_languages = num_scores ;
	 }
      else
	 {
	 // nothing is above our cutoff, but we can't just discard everything,
	 //   so scan for the highest score and make that the sole score
	 for (unsigned i = 1 ; i < numLanguages() ; i++)
	    {
	    if (m_scores[i] > m_scores[0])
	       {
	       m_scores[0] = m_scores[i] ;
	       m_lang_ids[0] = m_lang_ids[i] ;
	       }
	    }
	 m_num_languages = 1 ;
	 }
      delete [] scores_and_ids ;
      m_sorted = true ;
      }
   return ;
}

//----------------------------------------------------------------------

// the following variable makes sortByName() non-threadsafe
static const LanguageID *sort_langinfo = 0 ;

static int compare_names(const ScoreAndID *s1, const ScoreAndID *s2)
{
   if (!sort_langinfo)
      return s1->id() - s2->id() ;
   const char *name1 = sort_langinfo[s1->id()].language() ;
   const char *name2 = sort_langinfo[s2->id()].language() ;
   if (name1 && name2)
      {
      return strcmp(name1,name2) ;
      }
   else if (!name2)
      return -1 ;
   else if (!name1)
      return +1 ;
   return 0 ; // items are identical by sort order
}

//----------------------------------------------------------------------

void LanguageScores::sortByName(const LanguageID *langinfo)
{
   if (numLanguages() > 0 && langinfo != 0)
      {
      ScoreAndID *scores_and_ids = new ScoreAndID[numLanguages()] ;
      if (!scores_and_ids)
	 {
	 //!!!
	 return ;
	 }
      unsigned num_scores = 0 ;
      for (unsigned i = 0 ; i < numLanguages() ; i++)
	 {
	 if (m_scores[i] > 0.0)
	    scores_and_ids[num_scores++].init(m_scores[i],m_lang_ids[i]) ;
	 }
      sort_langinfo = langinfo ;
      FrQuickSort(scores_and_ids,num_scores,compare_names) ;
      for (unsigned i = 0 ; i < num_scores ; i++)
	 {
	 m_scores[i] = scores_and_ids[i].score() ;
	 m_lang_ids[i] = scores_and_ids[i].id() ;
	 }
      m_num_languages = num_scores ;
      delete [] scores_and_ids ;
      }
   return ;
}

//----------------------------------------------------------------------

void LanguageScores::mergeDuplicateNamesAndSort(const LanguageID *langinfo)
{
   if (!langinfo)
      return ;
   sortByName(langinfo) ;
   for (size_t i = 0 ; i + 1 < numLanguages() ; i++)
      {
      const char *name1 = langinfo[languageNumber(i)].language() ;
      if (!name1 || !*name1)
	 continue ;
      for (size_t j = i + 1 ; j < numLanguages() ; j++)
	 {
	 const char *name2 = langinfo[languageNumber(j)].language() ;
	 if (!name2 || !*name2)
	    break ;
	 if (strcmp(name1,name2) == 0)
	    {
	    m_scores[i] += m_scores[j] ;
	    m_scores[j] = 0.0 ;
	    }
	 }
      }
   sort() ;
   return ;
}

//----------------------------------------------------------------------

void LanguageScores::filterDuplicates(const LanguageIdentifier *langid,
				      bool ignore_region)
{
   if (!langid)
      return ;
   unsigned dest = 1 ;
   for (size_t i = 1 ; i < numLanguages() ; i++)
      {
      bool is_dup = false ;
      for (size_t j = 0 ; j < i ; j++)
	 {
	 if (langid->sameLanguage(i,j,ignore_region))
	    {
	    is_dup = true ;
	    break ;
	    }
	 }
      if (!is_dup)
	 {
	 m_lang_ids[dest] = m_lang_ids[i] ;
	 m_scores[dest] = m_scores[i] ;
	 dest++ ;
	 }
      }
   m_num_languages = dest ;
   return ;
}

/************************************************************************/
/*	Methods for class WeightedLanguageScores			*/
/************************************************************************/

WeightedLanguageScores::WeightedLanguageScores(size_t num_languages,
					       double default_weight)
   : LanguageScores(num_languages)
{
   m_weights = FrNewN(double,numLanguages()) ;
   if (m_weights)
      {
      for (size_t i = 0 ; i < numLanguages() ; i++)
	 {
	 m_weights[i] = default_weight ;
	 }
      }
   else
      {
      m_num_languages = 0 ;
      }
   return ;
}

//----------------------------------------------------------------------

WeightedLanguageScores::~WeightedLanguageScores()
{
   FrFree(m_weights) ;
   m_weights = 0 ;
   return ;
}

//----------------------------------------------------------------------

void WeightedLanguageScores::sqrtWeights()
{
   for (size_t i = 0 ; i < numLanguages() ; i++)
      {
      m_weights[i] = ::sqrt(m_weights[i]) ;
      }
   return ;
}

/************************************************************************/
/*	Methods for class LanguageIdentifier				*/
/************************************************************************/

LanguageIdentifier::LanguageIdentifier(const char *language_data_file,
				       bool verbose)
{
   m_alloc_languages = 0 ;
   m_num_languages = 0 ;
   m_langdata = 0 ;
   m_langinfo = 0 ;
   m_uncomplangdata = 0 ;
   m_alignments = 0 ;
   m_length_factors = 0 ;
   m_directory = 0 ;
   m_apply_cover_factor = true ;
   useFriendlyName(false) ;
   charsetIdentifier(0) ;
   setBigramWeight(DEFAULT_BIGRAM_WEIGHT) ;
   runVerbosely(verbose) ;
   if (language_data_file && *language_data_file)
      {
      FILE *fp = fopen(language_data_file,"rb") ;
      if (fp)
	 {
	 unsigned version = 0 ;
	 if (checkSignature(fp,&version))
	    {
	    m_directory = FrFileDirectory(language_data_file) ;
	    m_num_languages = read_uint32(fp,0) ;
	    if (numLanguages() > 0)
	       {
	       m_langinfo = FrNewC(LanguageID,numLanguages()) ;
	       if (!m_langinfo)
		  {
		  m_num_languages = 0 ;
		  FrFree(m_langinfo) ;	m_langinfo = 0 ;
		  }
	       else
		  {
		  m_alloc_languages = numLanguages() ;
		  }
	       uint8_t have_bigrams = false ;
	       (void)fread(&have_bigrams,sizeof(have_bigrams),1,fp) ;
	       // skip the reserved padding
	       fseek(fp,LANGID_PADBYTES_1,SEEK_CUR) ;
	       // read the language info records
	       for (size_t i = 0 ; i < numLanguages() ; i++)
		  {
		  if (!LanguageID::read(fp,&m_langinfo[i],version))
		     {
		     m_num_languages = 0 ;
		     break ;
		     }
		  }
	       // next, read the multi-trie
	       if (m_num_languages > 0)
		  {
		  m_langdata = PackedMultiTrie::load(fp,language_data_file) ;
		  if (m_langdata)
		     {
		     m_alloc_languages = numLanguages() ;
		     }
		  }
	       // finally, load the data mapping, if present
//FIXME
	       }
	    }
	 fclose(fp) ;
	 }
      }
   if (!PackedTrieFreq::dataMappingInitialized())
      {
      PackedTrieFreq::initDataMapping(scale_score) ;
      }
   m_unaligned = 0 ;
   setAlignments() ;
   setAdjustmentFactors() ;
   if (!m_langdata)
      m_langdata = new PackedMultiTrie ;
   if (!m_langinfo)
      m_langinfo = FrNewC(LanguageID,1) ;
   m_string_counts = FrNewC(size_t,numLanguages()) ;
   if (m_langdata)
      m_length_factors = make_length_factors(m_langdata->longestKey(),m_bigram_weight) ;
   return ;
}

//----------------------------------------------------------------------

LanguageIdentifier::~LanguageIdentifier()
{
   free_length_factors(m_length_factors) ;
   m_length_factors = 0 ;
   delete m_langdata ;		m_langdata = 0 ;
   delete m_uncomplangdata ;	m_uncomplangdata = 0 ;
   for (size_t i = 0 ; i < numLanguages() ; i++)
      {
      m_langinfo[i].LanguageID::~LanguageID() ;
      }
   FrFree(m_langinfo) ;		m_langinfo = 0 ;
   FrFree(m_alignments) ; 	m_alignments = 0 ;
   FrFree(m_unaligned) ;	m_unaligned = 0 ;
   FrFree(m_string_counts) ;	m_string_counts = 0 ;
   FrFree(m_directory) ;	m_directory = 0 ;
   m_num_languages = 0 ;
   m_alloc_languages = 0 ;
   return ;
}

//----------------------------------------------------------------------

void LanguageIdentifier::setAlignments()
{
   FrFree(m_alignments) ;
   m_alignments = FrNewN(uint8_t,PACKED_TRIE_LANGID_MASK + 1) ;
   for (size_t i = 0 ; i < numLanguages() ; i++)
      {
      uint8_t align = languageInfo(i)->alignment() ;
      m_alignments[i] = align ;
      }
   for (size_t i = numLanguages() ; i <= PACKED_TRIE_LANGID_MASK ; i++)
      {
      m_alignments[i] = (uint8_t)~0 ;
      }
   if (!m_unaligned)
      {
      m_unaligned = FrNewN(uint8_t,PACKED_TRIE_LANGID_MASK + 1) ;
      for (size_t i = 0 ; i < numLanguages() ; i++)
	 {
	 m_unaligned[i] = 1 ;
	 }
      for (size_t i = numLanguages() ; i <= PACKED_TRIE_LANGID_MASK ; i++)
	 {
	 m_unaligned[i] = (uint8_t)~0 ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

bool LanguageIdentifier::setAdjustmentFactors()
{
   FrFree(m_adjustments) ;
   m_adjustments = FrNewN(double,numLanguages()) ;
   if (!m_adjustments)
      return false ;
   for (size_t i = 0 ; i < numLanguages() ; i++)
      {
      const LanguageID *lang_info = languageInfo(i) ;
      m_adjustments[i] = 1.0 ;
      if (lang_info)
	 {
//!!!	 double cover = lang_info->coverageFactor() ;
//	 double cover = lang_info->countedCoverage() ;
	 double cover = lang_info->matchFactor() ;
	 if (cover > 0.0)
	    {
	    cover = ::pow(cover,0.25) ;
	    double align = 1.0 ;
	    // adjust for the fact that an enforced alignment of
	    //   greater than 1 forces the match factor to be lower
	    //   since only 1/align bytes could possibly start a match
	    if (m_alignments && m_alignments[i] <= 8)
	       align = m_alignments[i] ;
	    m_adjustments[i] = align / cover ;
	    }
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

PackedMultiTrie *LanguageIdentifier::packedTrie()
{
   if (!m_langdata && m_uncomplangdata)
      {
      m_langdata = new PackedMultiTrie(m_uncomplangdata) ;
      delete m_uncomplangdata ;
      m_uncomplangdata = 0 ;
      }
   return m_langdata ;
}

//----------------------------------------------------------------------

MultiTrie *LanguageIdentifier::unpackedTrie()
{
   if (!m_uncomplangdata && m_langdata)
      {
      m_uncomplangdata = new MultiTrie(m_langdata) ;
      delete m_langdata ;
      m_langdata = 0 ;
      }
   return m_uncomplangdata ;
}

//----------------------------------------------------------------------

const char *LanguageIdentifier::languageName(size_t N) const
{
   if (N < numLanguages())
      {
      return (m_friendly_name
	      ? m_langinfo[N].friendlyName()
	      : m_langinfo[N].language()) ;
      }
   return 0 ;
}

//----------------------------------------------------------------------

const char *LanguageIdentifier::friendlyName(size_t N) const
{
   if (N < numLanguages())
      {
      return m_langinfo[N].friendlyName() ;
      }
   return 0 ;
}

//----------------------------------------------------------------------

const char *LanguageIdentifier::languageEncoding(size_t N) const
{
   if (N < numLanguages())
      {
      return m_langinfo[N].encoding() ;
      }
   return 0 ;
}

//----------------------------------------------------------------------

const char *LanguageIdentifier::languageSource(size_t N) const
{
   if (N < numLanguages())
      {
      return m_langinfo[N].source() ;
      }
   return 0 ;
}

//----------------------------------------------------------------------

const char *LanguageIdentifier::languageScript(size_t N) const
{
   if (N < numLanguages())
      {
      return m_langinfo[N].script() ? m_langinfo[N].script() : "UNKNOWN" ;
      }
   return 0 ;
}

//----------------------------------------------------------------------

char *LanguageIdentifier::languageDescriptor(size_t N) const
{
   if (N < numLanguages())
      {
      return Fr_aprintf("%s_%s-%s",
			m_langinfo[N].language(),
			m_langinfo[N].region(),
			m_langinfo[N].encoding()) ;
      }
   return 0 ;
}

//----------------------------------------------------------------------

unsigned LanguageIdentifier::languageNumber(const LanguageID *lang_info)
   const
{
   unsigned modelnum = (unsigned)~0 ;
   if (lang_info)
      {
      // scan the list of models in the identifier for a uniquely-matching
      //   model
      for (size_t i = 0 ; i < numLanguages() ; i++)
	 {
	 const LanguageID *info = languageInfo(i) ;
	 if (info && info->matches(lang_info))
	    {
	    if (modelnum != (unsigned)~0)
	       {
	       if (verbose())
		  {
		  cerr << "Multiple models match language specifier" << endl ;
		  }
	       modelnum = (unsigned)~0 ;
	       break ;
	       }
	    modelnum = i ;
	    }
	 }
      }
   return modelnum ;
}

//----------------------------------------------------------------------

unsigned LanguageIdentifier::languageNumber(const char *langdescript) const
{
   unsigned modelnum = (unsigned)~0 ;
   if (langdescript)
      {
      // parse the description into language, region, encoding, and source
      char *language = 0 ;
      char *region = 0 ;
      char *encoding = 0 ;
      char *source = 0 ;
      parse_language_description(langdescript,language,region,encoding,source);
      // scan the list of models in the identifier for a uniquely-matching
      //   model
      for (size_t i = 0 ; i < numLanguages() ; i++)
	 {
	 const LanguageID *info = languageInfo(i) ;
	 if (info && info->matches(language,region,encoding,source))
	    {
	    if (modelnum != (unsigned)~0)
	       {
	       if (verbose())
		  {
		  cerr << "Multiple models match language specifier "
		       << langdescript << endl ;
		  }
	       modelnum = (unsigned)~0 ;
	       break ;
	       }
	    modelnum = i ;
	    }
	 }
      FrFree(language) ;
      FrFree(region) ;
      FrFree(encoding) ;
      FrFree(source) ;
      }
   return modelnum ;
}

//----------------------------------------------------------------------

static const unsigned max_alignments[4] = { 4, 1, 2, 1 } ;

static void identify_languages(const char *buffer, size_t buflen,
			       const PackedMultiTrie *langdata,
			       LanguageScores *scores,
			       const uint8_t *alignments,
			       const double *length_factors,
			       bool apply_stop_grams,
			       size_t length_normalizer)
{
   //assert(scores != 0) ;
   unsigned minhist = length_factors[2] ? 1 : 2 ;
   double *score_array = scores->scoreArray() ;
   double normalizer = (double)length_normalizer ;
   for (size_t index = 0 ; index + minhist < buflen ; index++)
      {
      uint32_t nodeindex = PTRIE_ROOT_INDEX ;
      if ((nodeindex = langdata->extendKey((uint8_t)buffer[index],nodeindex))
	  == NULL_INDEX)
	 continue ;
      if (minhist > 1 &&
	  (nodeindex = langdata->extendKey((uint8_t)buffer[index+1],nodeindex))
	  == NULL_INDEX)
	 continue ;
      // we have character sets with alignments of 1, 2, or 4 bytes; the
      //   low two bits of the offset from the start of the buffer tells
      //   us the maximum alignment which is valid at this point
      unsigned max_alignment = max_alignments[index%4] ;
      // since we'll almost always fail to extend the key before hitting
      //   the longest key in the trie, we can avoid conditional assignments
      //   and extra math by simply trying to extend the key all the way to
      //   the end of the buffer
      for (size_t i = index + minhist ; i < buflen ; i++)
	 {
	 uint8_t keybyte = (uint8_t)buffer[i] ;
	 if ((nodeindex = langdata->extendKey(keybyte,nodeindex))
	     == NULL_INDEX)
	    break ;
	 // check whether we're at a leaf node; if so, add all of the
	 //   frequencies to the scores
	 PackedTrieNode *node = langdata->node(nodeindex) ;
	 if (node->leaf())
	    {
	    double len_factor = length_factors[i - index + 1] ;
	    const PackedTrieFreq *f
	       = node->frequencies(langdata->frequencyBaseAddress()) ;
	    // normalize by text length so that scores are
	    //   comparable between different buffer sizes
	    len_factor /= normalizer ;
	    if (apply_stop_grams)
	       {
	       do {
		  unsigned id = f->languageID() ;
		  // ignore mis-aligned ngrams; we avoid a check that
		  //   'id' is in range by setting all possible IDs
		  //   above the number of models in the database such
		  //   that the alignment check never succeeds
		  if (likely(alignments[id] <= max_alignment))
		     {
		     double prob = f->mappedScore() ;
		     score_array[id] += (prob * len_factor) ;
		     }
		  f++ ;
	          } while (!f[-1].isLast()) ;
	       }
	    else
	       {
	       do {
		  unsigned id = f->languageID() ;
		  // ignore mis-aligned ngrams; we avoid a check that
		  //   'id' is in range by setting all possible IDs
		  //   above the number of models in the database such
		  //   that the alignment check never succeeds
		  if (likely(alignments[id] <= max_alignment))
		     {
		     double prob = f->mappedScore() ;
		     if (unlikely(prob <= 0.0))
			break ;		// only stopgrams from here on
		     score_array[id] += (prob * len_factor) ;
		     }
		  f++ ;
	          } while (!f[-1].isLast()) ;
	       }
	    }
	 }
      }
   return ;
}

//----------------------------------------------------------------------

bool LanguageIdentifier::identify(LanguageScores *scores,
				  const char *buffer, size_t buflen,
				  const uint8_t *alignments,
				  bool ignore_whitespace,
				  bool apply_stop_grams,
				  size_t length_normalization) const
{
   if (!buffer || !scores || !m_langdata)
      return false ;
   if (scores && scores->maxLanguages() == numLanguages())
      {
      scores->clear() ;
      }
   else
      {
      freeScores(scores) ;
      scores = new LanguageScores(numLanguages()) ;
      }
   m_langdata->ignoreWhiteSpace(ignore_whitespace) ;
   if (m_length_factors)
      m_length_factors[2] = m_bigram_weight * length_factor(2) ;
   if (!alignments)
      alignments = m_unaligned ;
   if (length_normalization == 0)
      length_normalization = buflen ;
   identify_languages(buffer,buflen,m_langdata,scores,alignments,
		      m_length_factors,apply_stop_grams,
		      length_normalization) ;
   m_langdata->ignoreWhiteSpace(false) ;
   return true ;
}

//----------------------------------------------------------------------

LanguageScores *LanguageIdentifier::identify(const char *buffer,
					     size_t buflen,
					     bool ignore_whitespace,
					     bool apply_stop_grams,
					     bool enforce_alignment) const
{
   if (!buffer || !buflen || !m_langdata)
      return 0 ;
   LanguageScores *scores = new LanguageScores(numLanguages()) ;
   const uint8_t *align = enforce_alignment ? m_alignments : 0 ;
   if (!identify(scores,buffer,buflen,align,ignore_whitespace,
		 apply_stop_grams,0))
      {
      delete scores ;
      scores = 0 ;
      }
   return scores ;
}

//----------------------------------------------------------------------

LanguageScores *LanguageIdentifier::identify(LanguageScores *scores,
					     const char *buffer,
					     size_t buflen,
					     bool ignore_whitespace,
					     bool apply_stop_grams,
					     bool enforce_alignment) const
{
   if (!buffer || !buflen || !m_langdata)
      return 0 ;
   if (scores && scores->maxLanguages() == numLanguages())
      {
      scores->clear() ;
      }
   else
      {
      freeScores(scores) ;
      scores = new LanguageScores(numLanguages()) ;
      }
   const uint8_t *align = enforce_alignment ? m_alignments : 0 ;
   if (!identify(scores,buffer,buflen,align,ignore_whitespace,
		 apply_stop_grams,0))
      {
      delete scores ;
      scores = 0 ;
      }
   return scores ;
}

//----------------------------------------------------------------------

bool LanguageIdentifier::finishIdentification(LanguageScores *scores, unsigned highestN,
					      double cutoff_ratio) const
{
   if (!scores)
      return false ;
   if (applyCoverageFactor())
      {
      for (size_t i = 0 ; i < scores->numLanguages() ; i++)
	 {
	 scores->setScore(i,scores->score(i) * adjustmentFactor(scores->languageNumber(i))) ;
	 }
      }
   if (highestN > 0)
      {
      if (highestN > scores->numLanguages())
	 highestN = scores->numLanguages() ;
      scores->sort(cutoff_ratio,highestN) ;
      }
   return true ;
}

//----------------------------------------------------------------------

void LanguageIdentifier::freeScores(LanguageScores *scores)
{
   delete scores ;
   return ;
}

//----------------------------------------------------------------------

static bool cosine_term(const PackedTrieNode *node, const uint8_t *,
			unsigned /*keylen*/, void *user_data)
{
   WeightedLanguageScores *scores = (WeightedLanguageScores*)user_data ;
   double lang1prob = 0.0 ;
   const PackedTrieFreq *frequencies
      = node->frequencies((PackedTrieFreq*)scores->userData()) ;
   unsigned langid = scores->activeLanguage() ;
   for (const PackedTrieFreq *freq = frequencies ; freq ; freq = freq->next())
      {
      if (freq->languageID() == langid)
	 {
	 if (!freq->isStopgram())
	    lang1prob = freq->probability() ;
	 break ;
	 }
      }
   for (const PackedTrieFreq *freq = frequencies ; freq ; freq = freq->next())
      {
      unsigned lang2 = freq->languageID() ;
      if (!freq->isStopgram())
	 {
	 double lang2prob = freq->probability() ;
	 scores->incrWeight(lang2,lang2prob * lang2prob) ;
	 scores->increment(lang2,lang1prob * lang2prob) ;
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

#if 0
//USEME
static bool all_cosine_term(const PackedTrieNode *node, const uint8_t *,
			    unsigned /*keylen*/, void *user_data)
{
   WeightedLanguageScores **scores = (WeightedLanguageScores**)user_data ;
   const PackedTrieFreq *frequencies
      = node->frequencies((PackedTrieFreq*)scores[0]->userData()) ;
   for (const PackedTrieFreq *freq = frequencies ; freq ; freq = freq->next())
      {
      if (freq->isStopgram())
	 continue ;
      double lang1prob = freq->probability() ;
      unsigned langid = freq->languageID() ;
      for (const PackedTrieFreq *freq2 = frequencies ;
	   freq2 ;
	   freq2 = freq->next())
	 {
	 if (freq2->isStopgram())
	    continue ;
	 unsigned lang2 = freq2->languageID() ;
	 double lang2prob = freq2->probability() ;
	 scores[langid]->incrWeight(lang2,lang2prob * lang2prob) ;
	 scores[langid]->increment(lang2,lang1prob * lang2prob) ;
	 }
      }
   return true ;
}
#endif /* 0 */

//----------------------------------------------------------------------

LanguageScores *LanguageIdentifier::similarity(unsigned langid) const
{
   if (langid >= numLanguages() || !trie())
      return 0 ;
   WeightedLanguageScores *scores
      = new WeightedLanguageScores(numLanguages(),0.0) ;
   if (scores && trie())
      {
      scores->setLanguage(langid) ;
      scores->setUserData((void*)trie()->frequencyBaseAddress()) ;
      unsigned maxkey = trie()->longestKey() ;
      uint8_t *keybuf = FrNewN(uint8_t,maxkey+1) ;
      trie()->enumerate(keybuf,maxkey,cosine_term,scores) ;
      FrFree(keybuf) ;
      scores->sqrtWeights() ;
      for (size_t i = 0 ; i < numLanguages() ; i++)
	 {
	 double wt = scores->weight(i) * scores->weight(langid) ;
	 if (wt > 0.0)
	    scores->setScore(i,(scores->score(i)/ wt)) ;
	 }
      }
   return scores ;
}

//----------------------------------------------------------------------

bool LanguageIdentifier::computeSimilarities()
{
//FIXME
   return true ;
}

//----------------------------------------------------------------------

bool LanguageIdentifier::sameLanguage(size_t L1, size_t L2,
				      bool ignore_region) const
{
   if (L1 < numLanguages() && L2 < numLanguages())
      {
      return m_langinfo[L1].sameLanguage(m_langinfo[L2],ignore_region) ;
      }
   return false ;
}

//----------------------------------------------------------------------

uint32_t LanguageIdentifier::addLanguage(const LanguageID &info,
					 uint64_t train_bytes)
{
   for (size_t i = 0 ; i < numLanguages() ; i++)
      {
      if (m_langinfo[i] == info)
	 return i ;
      }
   LanguageID *new_info ;
   if (numLanguages() >= allocLanguages())
      {
      size_t new_alloc = numLanguages() ? 2 * numLanguages() : 250 ;
      new_info = FrNewR(LanguageID,m_langinfo,new_alloc) ;
      if (!new_info)
	 return unknown_lang ;
      m_langinfo = new_info ;
      m_alloc_languages = new_alloc ;
      }
   uint32_t langID = m_num_languages++ ;
   new (&m_langinfo[langID]) LanguageID(&info) ;
   m_langinfo[langID].setTraining(train_bytes) ;
   return langID ;
}

//----------------------------------------------------------------------

void LanguageIdentifier::incrStringCount(size_t langnum)
{
   if (m_string_counts && langnum < numLanguages())
      m_string_counts[langnum]++ ;
   return ;
}

//----------------------------------------------------------------------

bool LanguageIdentifier::checkSignature(FILE *fp, unsigned *file_version)
{
   if (file_version)
      *file_version = 0 ;
   const unsigned siglen = sizeof(LANGID_FILE_SIGNATURE) ;
   char buffer[siglen] ;
   if (fread(buffer,sizeof(char),siglen,fp) != siglen)
      {
      errno = EACCES ;
      return false ;
      }
   if (memcmp(buffer,LANGID_FILE_SIGNATURE,siglen) != 0)
      {
      errno = EINVAL ;
      return false ;
      }
   unsigned char version ;
   if (fread(&version,sizeof(version),1,fp) != 1 ||
       version < LANGID_MIN_FILE_VERSION || version > LANGID_FILE_VERSION)
      {
      errno = EINVAL ;
      return false ;
      }
   if (file_version)
      *file_version = version ;
   return true ;
}

//----------------------------------------------------------------------

bool LanguageIdentifier::writeStatistics(FILE *fp) const
{
   if (!fp || !m_string_counts)
      return false ;
   fprintf(fp,"===================\n") ;
   fprintf(fp,"Number of strings extracted, by language:\n") ;
   LanguageScores counts(numLanguages()) ;
   for (size_t i = 0 ; i < numLanguages() ; i++)
      {
      counts.setScore(i,m_string_counts[i]) ;
      }
   counts.mergeDuplicateNamesAndSort(m_langinfo) ;
   for (size_t i = 0 ; i < numLanguages() ; i++)
      {
      double count = counts.score(i) ;
      if (count <= 0.0)
	 break ;
      unsigned langnum = counts.languageNumber(i) ;
      fprintf(fp," %7lu\t%s\n",(unsigned long)count,languageName(langnum)) ;
      }
   fprintf(fp,"===================\n") ;
   return true ;
}

//----------------------------------------------------------------------

bool LanguageIdentifier::writeHeader(FILE *fp) const
{
   // write the signature string
   const unsigned siglen = sizeof(LANGID_FILE_SIGNATURE) ;
   if (fwrite(LANGID_FILE_SIGNATURE,siglen,1,fp) != 1)
      return false ;
   // follow with the format version number
   unsigned char version = LANGID_FILE_VERSION ;
   if (fwrite(&version,sizeof(version),1,fp) != 1)
      return false ;
   // then the number of language info records
   if (!write_uint32(fp,numLanguages()))
      return false ;
   // set the flag for whether we have bigram models following the multi-trie
   uint8_t have_bigrams = 1 ;
   if (fwrite(&have_bigrams,sizeof(have_bigrams),1,fp) != 1)
      return false ;
   // pad the header with NULs for the unused reserved portion of the header
   for (size_t i = 0 ; i < LANGID_PADBYTES_1 ; i++)
      {
      if (fputc('\0',fp) == EOF)
	 return false ;
      }
   return true ;
}

//----------------------------------------------------------------------

static int compare_frequencies(const MultiTrieFrequency &f1,
			       const MultiTrieFrequency &f2)
{
   bool s1 = f1.isStopgram() ;
   bool s2 = f2.isStopgram() ;
   // all stopgrams go after non-stopgrams
   if (s1 && !s2)
      return +1 ;
   else if (!s1 && s2)
      return -1 ;
   // within stopgram/non-stopgram, sort by language ID for cache locality
   uint32_t id1 = f1.languageID() ;
   uint32_t id2 = f2.languageID() ;
   if (id1 < id2)
      return -1 ;
   else
      return +1 ;
}

//----------------------------------------------------------------------

static bool sort_frequencies(const MultiTrieNode *n, const uint8_t * /*key*/,
			     unsigned /*keylen*/, void * /*user_data*/)
{
   MultiTrieFrequency *f = n->frequencies() ;
   if (f)
      {
      // sort the frequency records
      f = FrMergeSort(f,compare_frequencies) ;
      MultiTrieNode *node = ((MultiTrieNode*)n) ;
      if (!node->setFrequencies(f))
	 return false ;
      }
   return true ;
}

//----------------------------------------------------------------------

bool LanguageIdentifier::write(FILE *fp)
{
   bool success = writeHeader(fp) ;
   if (success)
      {
      // sort the frequency records for each leaf node so that stop-grams
      //   come last
      MultiTrie *mtrie = unpackedTrie() ;
      uint8_t keybuf[500] ;
      if (mtrie &&
	  !mtrie->enumerate(keybuf,sizeof(keybuf),sort_frequencies,mtrie))
	 {
	 success = false ;
	 }
      // write out the languageID records
      for (size_t i = 0 ; i < numLanguages() ; i++)
	 {
	 m_langinfo[i].write(fp) ;
	 }
      // now write out the trie
      PackedMultiTrie *trie = packedTrie() ;
      if (!trie || !trie->write(fp))
	 {
	 success = false ;
	 }
      write_uint32(fp,(uint32_t)~0) ;
      // finally, write out the mapping from stored frequency value to
      //   actual weighted value
      off_t dm_offset = ftell(fp) ;
      if (PackedTrieFreq::writeDataMapping(fp))
	 {
	 fseek(fp,LANGID_FILE_DMOFFSET,SEEK_SET) ;
	 write_uint64(fp,dm_offset) ;
	 }
      }
   return success ;
}

//----------------------------------------------------------------------

static bool write_langident(FILE *fp, void *user_data)
{
   LanguageIdentifier *langid = (LanguageIdentifier*)user_data ;
   return langid->write(fp) ;
}

//----------------------------------------------------------------------

bool LanguageIdentifier::write(const char *filename) const
{
   if (filename && *filename)
      {
      return FrSafelyRewriteFile(filename,write_langident,(void*)this) ;
      }
   return false ;
}

//----------------------------------------------------------------------

bool LanguageIdentifier::dump(FILE *fp, bool show_ngrams) const
{
   fprintf(fp,"LanguageIdentifier Begin\n") ;
   for (size_t i = 0 ; i < numLanguages() ; i++)
      {
      const LanguageID *info = &m_langinfo[i] ;
      fprintf(fp,"  Lang %2u: %s_%s-%s / %s\n",(unsigned)i,
	      info->language(),info->region(),info->encoding(),info->source());
      }
   bool success = true ;
   if (m_langdata && show_ngrams)
      {
      fprintf(fp,"LanguageIdentifier Trie\n") ;
      success = m_langdata->dump(fp) ;
      }
   else if (m_uncomplangdata && show_ngrams)
      {
      fprintf(fp,"LanguageIdentifier Trie\n") ;
      success = m_uncomplangdata->dump(fp) ;
      }
   fprintf(fp,"LanguageIdentifier End\n") ;
   return success ;
}

/************************************************************************/
/*	Procedural interface						*/
/************************************************************************/

static LanguageIdentifier *try_loading(const char *database_file,
				       bool verbose)
{
   if (!database_file)
      return 0 ;
   char *db_filename = 0 ;
   if (database_file[0] == '~' && database_file[1] == '/')
      {
      const char *home = getenv("HOME") ;
      if (home)
	 {
	 db_filename = Fr_aprintf("%s%s",home,database_file+1) ;
	 }
      else
	 {
	 const char *user = getenv("USER") ;
	 if (user)
	    db_filename = Fr_aprintf("/home/%s%s",user,database_file+1) ;
	 }
      }
   if (!db_filename)
      db_filename = FrDupString(database_file) ;
   LanguageIdentifier *id = new LanguageIdentifier(db_filename,verbose) ;
   FrFree(db_filename) ;
   if (!id)
      {
      FrNoMemory("loading language database") ;
      return 0 ;
      }
   else if (id->numLanguages() == 0)
      {
      if (verbose)
	 cerr << "Unsuccessfully tried to open " << database_file << endl ;
      delete id ;
      return 0 ;
      }
   else if (verbose)
      {
      cerr << "Opened language database " << database_file << endl ;
      }
   return id ;
}

//----------------------------------------------------------------------

LanguageIdentifier *load_language_database(const char *database_file,
					   const char *charset_file,
					   bool create, bool verbose)
{
   LanguageIdentifier *id = 0 ;
   if (database_file && *database_file)
      id = try_loading(database_file, verbose) ;
   if (!id && !create)
      {
      id = try_loading(FALLBACK_LANGID_DATABASE, verbose) ;
      if (!id)
	 {
	 id = try_loading(ALTERNATE_LANGID_DATABASE, verbose) ;
	 if (!id)
	    id = try_loading(DEFAULT_LANGID_DATABASE, verbose) ;
	 }
      }
   if (!id && create)
      id = new LanguageIdentifier(database_file,verbose) ;
   if (!id)
      {
      if (database_file && *database_file)
	 {
	 cerr << "Warning: Unable to load database from " << database_file
	      << endl ;
	 }
      else
	 {
	 cerr << "Warning: unable to load database from standard locations"
	      << endl ;
	 }
      }
   else
      {
      LanguageIdentifier *cs = 0 ;
      if (charset_file)
	 {
	 if (*charset_file)
	    cs = try_loading(charset_file, verbose) ;
	 else
	    cs = id ;
	 }
      if (!cs)
	 {
	 cs = try_loading(FALLBACK_CHARSET_DATABASE, verbose) ;
	 if (!cs)
	    {
	    cs = try_loading(ALTERNATE_CHARSET_DATABASE, verbose) ;
	    if (!cs)
	       cs = try_loading(DEFAULT_CHARSET_DATABASE, verbose) ;
	    }
	 }
      if (!cs)
	 cs = id ;
      id->charsetIdentifier(cs) ;
      }
   return id ;
}

//----------------------------------------------------------------------

void unload_language_database(LanguageIdentifier *id)
{
   if (!id)
      return ;
   if (id->charsetIdentifier() != id)
      {
      delete id->charsetIdentifier() ;
      id->charsetIdentifier(0) ;
      }
   delete id ;
   return ;
}

//----------------------------------------------------------------------

double set_stopgram_penalty(double pen)
{
   double old_pen = stop_gram_penalty ;
   stop_gram_penalty = -10.0 * pen ;
   return old_pen ;
}

// end of file langid.C //
