/****************************** -*- C++ -*- *****************************/
/*                                                                      */
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     extract.h							*/
/*  Version:  1.23							*/
/*  LastEdit: 20aug2013							*/
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

#ifndef __EXTRACT_H_INCLUDED
#define __EXTRACT_H_INCLUDED

#include <stdint.h>
#include "language.h" // for OutputFormat

/************************************************************************/
/*	Manifest Constants						*/
/************************************************************************/

#define MIN_STRING_LENGTH 4
#define DEFAULT_MAX_GAP 1000
#define DEFAULT_ALPHA_PERCENT 0.25
#define DEFAULT_DESIRED_PERCENT 0.5
#define DEFAULT_MIN_SCORE 0.1

// how many character sets will we attempt to check if automatic charset
//   identification is unsuccessful?
#define ENCID_FALLBACK_SETS 3

/************************************************************************/
/************************************************************************/

class LanguageIdentifier ;

typedef bool StringOutputFunction(const unsigned char *buf, unsigned len,
				  uint64_t bufloc, CharacterSet *charset,
				  double confidence, const class LanguageScores *scores,
				  const class ExtractParameters *params) ;

class ExtractParameters
   {
   private:
      LanguageIdentifier     *m_langident ;
      StringOutputFunction   *m_outputfn ;
      const CharacterSet    **m_charsets ;
      const CharacterSet    **m_encsets ;   // character sets for encoding identification
      const char             *m_outdir ;
      class feature_recorder *m_featurerec ;
      class feature_recorder *m_languagerec ;
      class feature_recorder *m_encodingrec ;
      const class sbuf_t     *m_sbuf ;
      mutable LanguageScores *m_priorscores ;
      uint64_t	m_start ;
      uint64_t	m_end ;
      unsigned	m_maxgap ;
      unsigned	m_minstring ;
      unsigned  m_locationradix ;
      unsigned  m_maxlangs ;
      unsigned  m_numcharsets ;		// how many character-set mappings do we have?
      unsigned  m_numencsets ;		// how many encoding-identification mappings do we have?
      double	m_desiredpercent ;
      double	m_alphapercent ;
      double	m_minscore ;
      OutputFormat m_output_format ;
      bool	m_newlines ;
      bool	m_showconf ;
      bool	m_showfile ;
      bool	m_showenc ;
      bool	m_showscript ;
      bool	m_separate_outputs ;
      bool	m_identify_lang ;
      bool	m_smooth_scores ;
      bool	m_count_languages ;
      bool	m_friendly_name ;
      bool	m_romanize ;
      bool      m_forceCRLF ;
   public:
      ExtractParameters()
	 { m_start = 0 ; m_end = (uint64_t)~0 ; m_maxgap = DEFAULT_MAX_GAP ;
	   m_outputfn = 0 ; m_featurerec = 0 ; m_languagerec = 0 ;
	   m_encodingrec = 0 ; m_sbuf = 0 ;
	   m_outdir = "." ;
	   m_charsets = 0 ; m_numcharsets = 0 ;
	   m_encsets = 0 ; m_numencsets = 0 ;
	   m_langident = 0 ; m_maxlangs = 1 ;
	   m_priorscores = 0 ;
	   m_minstring = MIN_STRING_LENGTH;
	   m_desiredpercent = DEFAULT_DESIRED_PERCENT ;
	   m_alphapercent = DEFAULT_ALPHA_PERCENT ;
	   m_locationradix = 0 ; m_minscore = DEFAULT_MIN_SCORE ;
	   m_smooth_scores = true ; m_showscript = false ; 
	   m_newlines = false ; m_showconf = false ; m_showenc = false ;
	   m_showfile = false ; m_separate_outputs = false ;
	   m_identify_lang = false ; m_output_format = OF_Native ;
	   m_romanize = false ; m_forceCRLF = false ; }
      ExtractParameters(const ExtractParameters &orig) ;
      ~ExtractParameters() ;

      // accessors
      LanguageIdentifier *languageIdentifier() const
	 { return m_langident ; }
      StringOutputFunction *outputFn() const { return m_outputfn ; }
      const char *outputDirectory() const { return m_outdir ; }
      class feature_recorder *featureRecorder() const { return m_featurerec ; }
      class feature_recorder *languageRecorder() const { return m_languagerec ; }
      class feature_recorder *encodingRecorder() const { return m_encodingrec ; }
      const class sbuf_t *sbuf() const { return m_sbuf ; }
      LanguageScores *&priorLanguageScores() const { return m_priorscores ; }
      uint64_t startOffset() const { return m_start ; }
      uint64_t endOffset() const { return m_end ; }
      unsigned minimumString() const { return m_minstring ; }
      unsigned maximumGap() const { return m_maxgap ; }
      unsigned maxLanguages() const { return m_maxlangs ; }
      double minDesiredPercent() const { return m_desiredpercent ; }
      double minAlphaPercent() const { return m_alphapercent ; }
      double minimumScore() const { return m_minscore ; }
      OutputFormat outputFormat() const { return m_output_format ; }
      bool newlinesAllowed() const { return m_newlines; }
      bool showConfidence() const { return m_showconf ; }
      bool showFilename() const { return m_showfile ; }
      bool showEncoding() const { return m_showenc ; }
      bool separateOutputs() const { return m_separate_outputs ; }
      bool identifyLanguage() const { return m_identify_lang ; }
      bool smoothLanguageScores() const { return m_smooth_scores ; }
      bool countLanguages() const { return m_count_languages ; }
      bool showFriendlyName() const { return m_friendly_name ; }
      bool showScript() const { return m_showscript ; }
      bool convert2UTF8() const { return m_output_format == OF_UTF8 ; }
      bool romanizeOutput() const { return m_romanize ; }
      bool forceCRLF() const { return m_forceCRLF ; }
      unsigned showLocation() const { return m_locationradix ; }
      const CharacterSet *charset(unsigned N) const
	 { return (N < m_numcharsets) ? m_charsets[N] : 0 ; }
      const CharacterSet *encodingCharset(unsigned N) const
	 { return (N < m_numencsets) ? m_encsets[N] : 0 ; }

      // setters
      void setLanguageIdentifier(LanguageIdentifier *ident)
	 { m_langident = ident ; }
      void setOutputFunction(StringOutputFunction *fn)
	 { m_outputfn = fn ; }
      void setOutputDirectory(const char *dir)
	 { m_outdir = (dir && *dir) ? dir : "." ; }
      void setFeatureRecorder(class feature_recorder *fr)
	 { m_featurerec = fr ; }
      void setLanguageRecorder(class feature_recorder *fr)
	 { m_languagerec = fr ; }
      void setEncodingRecorder(class feature_recorder *fr)
	 { m_encodingrec = fr ; }
      void setSbuf(const class sbuf_t *sbuf) { m_sbuf = sbuf ; }
      void setCharSets() ;
      void clearCharSets() ;
      void setRange(uint64_t s, uint64_t e) { m_start = s ; m_end = e ; }
      void setLength(unsigned l) { m_minstring = l ; }
      void setGap(unsigned g) { m_maxgap = g ; }
      void setMaxLanguages(unsigned l) { m_maxlangs = l ; }
      void allowNewlines(bool allow = true) { m_newlines = allow ; }
      void wantConfidence(bool conf = true) { m_showconf = conf ; }
      void wantFilename(bool show = true) { m_showfile = show ; }
      void wantEncoding(bool enc = true) { m_showenc = enc ; }
      void wantSeparateOutputs(bool sep = true) { m_separate_outputs = sep ; }
      void identifyLanguage(bool ident = true) { m_identify_lang = ident ; }
      void wantScript(bool scr = true) { m_showscript = scr ; }
      void wantSmoothedScores(bool sm = true) { m_smooth_scores = sm ; }
      void countLanguages(bool count) { m_count_languages = count ; }

      void wantFriendlyName(bool fr = true) { m_friendly_name = fr ; }
      void outputFormat(OutputFormat fmt) { m_output_format = fmt ; }
      void romanizeOutput(bool rom) { m_romanize = rom ; }
      void wantLocation(char locspec) ;
      void wantCRLF(bool force) ;
      void setDesired(double d) { m_desiredpercent = d ; }
      void setAlpha(double a) { m_alphapercent = a ; }
      void setMinimumScore(double s)
      	 { m_minscore = (s > 0.0) ? s : DEFAULT_MIN_SCORE ; }
   } ;

//----------------------------------------------------------------------

// abstract base class
class InputStream
   {
   private:
      // no data members
   public:
      InputStream() {}
      virtual ~InputStream() {}

      // accessors
      virtual bool endOfData() const = 0 ;
      virtual uint64_t currentOffset() const = 0 ;
      virtual unsigned get(unsigned count, unsigned char *buffer) = 0 ;
   } ;

//----------------------------------------------------------------------

void free_language_scores() ;

void extract_text(InputStream *in, FILE *outfp, const char *filename,
		  uint64_t end_offset, const CharacterSet * const *charsets,
		  const ExtractParameters *params, bool verbose,
		  class LanguageScores *given_langscores = 0,
		  class LanguageScores *given_charset_scores = 0) ;

void extract_text(const CharacterSet * const *charsets,
		  const ExtractParameters *params,
		  bool verbose, int argc, const char **argv) ;

#endif /* !__EXTRACT_H_INCLUDED */

/* end of file extract.h */
