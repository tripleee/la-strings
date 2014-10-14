/************************************************************************/
/*									*/
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     scan_strings.C						*/
/*  Version:  1.24				       			*/
/*  LastEdit: 13nov2013							*/
/*									*/
/*  (c) Copyright 2011,2013 Ralf Brown/CMU				*/
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

#ifdef BULK_EXTRACTOR

#include <cstdlib>
#include <cstdio>
#include "charset.h"
#include "extract.h"
#include "langident/langid.h"
#include "la-strings.h"

// bulk_extractor 1.4.0 headers don't compile cleanly with all warnings enabled
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wextra"
#include "bulk_extractor.h"
#pragma GCC diagnostic pop

// avoid symbol not found error
const sbuf_t *sbuf_t::highest_parent() const 
{
    const sbuf_t *hp = this;
    while(hp->parent != 0){
        hp = hp->parent;
    }
    return hp;
}

/************************************************************************/
/*	Types for this module						*/
/************************************************************************/

class InputStreamBE : public InputStream
   {
   private:
      const uint8_t *m_streamstart ;
      const uint8_t *m_streamend ;
      const uint8_t *m_curr ;
      unsigned	     m_margin ;		// everything beyond m_streamstart+m_margin is margin
      uint64_t       m_baseoffset ;

   public:
      InputStreamBE(const uint8_t *s, const uint8_t *e, unsigned pagesize,
		    uint64_t baseoff = 0)
	 { m_curr = m_streamstart = s ; m_streamend = e ;
	   m_margin = pagesize ; m_baseoffset = baseoff ; }
      virtual ~InputStreamBE() {}

      // accessors
      virtual bool endOfData() const
	 { return m_curr >= m_streamend || m_curr > m_streamstart + m_margin ; }
      virtual uint64_t currentOffset() const
	 { return m_baseoffset + (m_curr - m_streamstart) ; }
      virtual unsigned get(unsigned count, unsigned char *buffer)
	 {
	    if (endOfData()) return 0 ;
	    if (count > (m_streamend - m_curr))
	       count = m_streamend - m_curr ;
	    memcpy(buffer,m_curr,count) ;
	    m_curr += count ;
	    return count ;
	 }
   } ;

/************************************************************************/
/*	Global variables for this module				*/
/************************************************************************/

static ExtractParameters params ;
static __thread LanguageScores *lang_scores = 0 ;
static __thread LanguageScores *charset_scores = 0 ;
static __thread bool thread_initialized = false ;

static const CharacterSet *cs_ASCII = 0 ;
static const CharacterSet *cs_ASCII16 = 0 ;
static const CharacterSet *cs_UTF8 = 0 ;

static FrThreadKey scanner_key ;

// configuration retrieved from bulk_extractor
static int debug = 0 ;
static string cfg_langid_db ;
static string cfg_charset_db ;
static uint32_t cfg_minstring = MIN_STRING_LENGTH ;
static uint32_t cfg_threshold = 600 ;
static bool cfg_friendly = false ;
static bool cfg_smoothing = false ;
static bool cfg_unknowns = false ;
static bool cfg_printenc = true ;

/************************************************************************/
/*	Global data for this module					*/
/************************************************************************/

static const char help_langid[] = 
   "The pathname of the language identification database, will search in standard locations if not specified" ;
static const char help_charsetid[] =
   "The pathname of the character-set identification database (use main langid database if not available)" ;
static const char help_minstring[] =
   "The minimum length of strings to extract" ;
static const char help_threshold[] =
   "The minimum score a string must have to be output, in hundredths; useful range is 400-1500" ;
static const char help_longlang[] =
   "Output full language names rather than language codes" ;
static const char help_smoothing[] =
   "Apply inter-string smoothing to language scores" ;
static const char help_unknowns[] =
   "Output strings whose language can't be identified" ;
static const char help_printenc[] =
   "Output identified character sets" ;
static const char help_nohist[] =
   "Don't generate histograms for extracted strings and encodings" ;

/************************************************************************/
/************************************************************************/

static bool got_hit(const unsigned char *buf, unsigned len, uint64_t bufloc,
		    CharacterSet *charset, double confidence,
		    const LanguageScores *scores, const ExtractParameters *params)
{
   (void)confidence ;
   const sbuf_t *sbuf = params->sbuf() ;
   feature_recorder *fr = params->featureRecorder() ;
   pos0_t pos0 = sbuf->pos0 ;
   pos0 = pos0 + bufloc ;
   stringstream s ;
   if (pos0.path.length() > 0)
      s << pos0.path << '.' ;
   s << pos0.offset ;
   if (fr)
      {
      bool unknown_lang = true ;
      const char *langname = "UNK" ;
      const char *strength = "" ;
      LanguageIdentifier *langid = params->languageIdentifier() ;
      if (scores && langid)
	 {
	 unsigned id = scores->highestLangID() ;
	 if (id < langid->numLanguages())
	    {
	    langname = langid->languageName(id) ;
	    unknown_lang = false ;
	    if (scores->highestScore() < 1.2)
	       strength = "?" ;
	    }
	 }
      unsigned buflen = 6 * len ;
      FrLocalAlloc(char,line,6*FrMAX_LINE,buflen+1) ;
      if (line && (!unknown_lang || cfg_unknowns))
	 {
	 bool must_convert =
	    (charset != cs_ASCII && charset != cs_UTF8 &&
	     charset != cs_ASCII16 && 
	     params->outputFormat() != OF_Native) ;
	 bool converted = false ;
	 if (must_convert)
	    {
	    bool romanize = (params->romanizeOutput() &&
			     charset->romanizable(buf,len)) ;

	    if (romanize)
	       {
	       memset(line,'\0',buflen) ;
	       converted = charset->convertToUTF8((const char*)buf,len,line,buflen,true) ;
	       fr->printf("%s\t%s%s\t%s [romanized]",s.str().c_str(),langname,strength,line) ;
	       }
	    memset(line,'\0',buflen) ;
	    if (charset->convertToUTF8((const char*)buf,len,line,buflen,false))
	       converted = true ;
	    }
	 if (!converted)
	    {
	    unsigned outlen = 0 ;
	    for (size_t i = 0 ; i < len ; i++)
	       {
	       if (buf[i] == 0)
		  {
		  if (!charset->filterNUL())
		     {
		     memcpy(line+outlen,"\\x00",4) ;
		     outlen += 4 ;
		     }
		  }
	       else if (buf[i] == ',')
		  {
		  memcpy(line+outlen,"\\x2C",4) ;
		  outlen += 4 ;
		  }
	       else if (buf[i] == '\\')
		  {
		  memcpy(line+outlen,"\\x5C",4) ;
		  outlen += 4 ;
		  }
	       else
		  line[outlen++] = buf[i] ;
	       }
	    line[outlen] = '\0' ;
	    }
	 fr->printf("%s\t%s%s\t%s",s.str().c_str(),langname,strength,line) ;
	 FrLocalFree(line) ;
	 }
      }
   feature_recorder *er = params->encodingRecorder() ;
   if (er && charset)
      {
      // record detected encoding
      er->printf("%s	%s",s.str().c_str(),charset->encodingName()) ;
      }
   return true ;
}

//----------------------------------------------------------------------

#if 0
static bool affirmative(const string arg, bool default_value)
{
   if (arg.length() == 0)
      return default_value ;
   char c = arg[0] ;
   if (c == 'Y' || c == 'y' || c == '1' || c == 't' || c == 'T')
      return true ;
   else if (c == 'N' || c == 'n' || c == '0' || c == 'f' || c == 'F')
      return false ;
   return default_value ;
}
#endif

//----------------------------------------------------------------------

static void startup(const class scanner_params &sp)
{
   // ensure the correct format scanner information
   assert(sp.info->si_version == scanner_info::CURRENT_SI_VERSION) ;

   // grab our global configuration variables
   bool cfg_nohist = false ;
   debug = sp.info->config->debug ;
   sp.info->get_config("lastrings_langid",&cfg_langid_db,help_langid) ;
   sp.info->get_config("lastrings_charsetid",&cfg_charset_db,help_charsetid) ;
   sp.info->get_config("lastrings_minstring",&cfg_minstring,help_minstring) ;
   sp.info->get_config("lastrings_threshold",&cfg_threshold,help_threshold) ;
   sp.info->get_config("lastrings_smoothing",&cfg_smoothing,help_smoothing) ;
   sp.info->get_config("lastrings_longlang",&cfg_friendly,help_longlang) ;
   sp.info->get_config("lastrings_unknowns",&cfg_unknowns,help_unknowns) ;
   sp.info->get_config("lastrings_enc",&cfg_printenc,help_printenc) ;
   sp.info->get_config("lastrings_nohist",&cfg_nohist,help_nohist) ;

   // identify the scanner
   sp.info->name 	    = "lastrings" ;
   sp.info->author 	    = "Ralf Brown" ;
   sp.info->description     = "Language-Aware String Extractor" ;
   sp.info->scanner_version = LASTRINGS_VERSION ;
   //sp.info->flags           = scanner_info::SCANNER_FIND_SCANNER ;
   sp.info->feature_names.insert("strings") ;
   if (cfg_printenc)
      {
      sp.info->feature_names.insert("encodings") ;
      }
   if (!cfg_nohist)
      {
      sp.info->histogram_defs.insert(histogram_def("strings","","histogram",0)) ;
      if (cfg_printenc)
	 {
	 sp.info->histogram_defs.insert(histogram_def("encodings","","histogram",0)) ;
	 }
      }

   return ;
}

//----------------------------------------------------------------------

static void thread_finish(void*)
{
   if ((debug & DEBUG_PRINT_STEPS) != 0)
      {
      cerr << "LA-Strings thread_finish()" << endl ;
      }
   delete lang_scores ;
   lang_scores = 0 ;
   delete charset_scores ;
   charset_scores = 0 ;
   return ;
}

//----------------------------------------------------------------------

static void initialize(const class scanner_params &sp)
{
   initialize_FramepaC() ;
   bool run_verbosely = ((debug & DEBUG_INFO) != 0) ;
   if (run_verbosely)
      {
      cerr << "FP initialized" << endl ;
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
   const char *langid_file = cfg_langid_db.c_str() ;
   if (langid_file && *langid_file == '\0')
      langid_file = 0 ;
   const char *charset_file = cfg_charset_db.c_str() ;
   if (charset_file && *charset_file == '\0')
      charset_file = 0 ;
   if (run_verbosely)
      {
      cerr << "loading langid(" << cfg_langid_db << "," << cfg_charset_db
	   << ")" << endl ;
      }
   LanguageIdentifier *langid =
      load_language_database(langid_file,charset_file) ;

   CharacterSetCache *cache = CharacterSetCache::instance() ;
   if (cache)
      {
      cs_ASCII = cache->getCharSet("ASCII") ;
      cs_ASCII16 = cache->getCharSet("ASCII-16LE") ;
      cs_UTF8 = cache->getCharSet("UTF-8") ;
      }

   if (langid)
      {
      langid->useFriendlyName(cfg_friendly) ;
      }

   // set up our parameters:
   double thresh = cfg_threshold / 100 ;
   if (thresh > 0)
      params.setMinimumScore(thresh) ;
   params.setLength(cfg_minstring) ;
   params.wantSmoothedScores(cfg_smoothing) ;
   params.wantEncoding(true) ;
   params.identifyLanguage(true) ;
   params.setMaxLanguages(3) ;
   params.romanizeOutput(true) ;
   //params.outputFormat(OF_Native) ;
   params.outputFormat(OF_UTF8) ;
   params.setSbuf(&sp.sbuf) ;
   params.setOutputFunction(got_hit) ;
   params.setLanguageIdentifier(langid) ;
   params.setCharSets() ;
   if (!langid)
      {
      sp.info->flags |= scanner_info::SCANNER_DISABLED ;
      }
   else if (run_verbosely)
      {
      cerr << "langid successfully loaded." << endl ;
      if (langid)
	 {
	 if (langid->charsetIdentifier() == langid)
	    cerr << "(using main database for encoding identification)" << endl ;
	 else if (langid->charsetIdentifier() == 0)
	    cerr << "(no encoding identifier!)" << endl ;
	 }
      }
   // set up a per-thread destructor to clean up thread-local memory allocations
   FrThread::createKey(scanner_key,thread_finish) ;
   return ;
}

//----------------------------------------------------------------------

static void thread_start()
{
   if (thread_initialized) return ;
   LanguageIdentifier *ident = params.languageIdentifier() ;
   if (ident)
      {
      lang_scores = new LanguageScores(ident->numLanguages()) ;
      ident = ident->charsetIdentifier() ;
      }
   if (ident)
      charset_scores = new LanguageScores(ident->numLanguages()) ;
   thread_initialized = true ;
   // enable the destructor for this thread
   FrThread::setKey(scanner_key,(void*)1) ;
   if ((debug & DEBUG_PRINT_STEPS) != 0)
      {
      cerr << "LA-Strings thread initialized" << endl ;
      }
   return ;
}

//----------------------------------------------------------------------

static void process_buffer(const sbuf_t scanbuf, feature_recorder *fr,
			   feature_recorder *fr_enc)
{
   thread_start() ; // BE 1.4.0beta3 never actually calls PHASE_THREAD_BEFORE_SCAN!
   const uint8_t *buffer_start = scanbuf.buf ;
   const uint8_t *buffer_end = scanbuf.buf + scanbuf.size() ;
   bool run_verbosely = ((debug & DEBUG_INFO) != 0) ;
   if (run_verbosely)
      {
      cerr << "lastrings(" << hex << (uint64_t)buffer_start << ":"
	   << (buffer_end-buffer_start) << ", " << scanbuf.pagesize
	   << ") start" << dec << endl ;
      }
   InputStreamBE is(buffer_start,buffer_end,scanbuf.pagesize) ;
   const char *filename = "" ;
   uint64_t end_offset = (uint64_t)~0LL ;
   ExtractParameters parameters(params) ;
   parameters.setFeatureRecorder(fr) ;
   parameters.setEncodingRecorder(fr_enc) ;
   parameters.setSbuf(&scanbuf) ;
   extract_text(&is,0,filename,end_offset,0,&parameters,run_verbosely,
		lang_scores,charset_scores) ;
   if (run_verbosely)
      {
      cerr << "  lastrings(" << hex << (uint64_t)buffer_start << ":" 
	   << (buffer_end-buffer_start) << ") done" << dec << endl ;
      }
   return ;
}

//----------------------------------------------------------------------

static void cleanup()
{
   params.clearCharSets() ;
   LanguageIdentifier *langid = params.languageIdentifier() ;
   params.setLanguageIdentifier(0) ;
   delete langid ;
   if ((debug & DEBUG_INFO) != 0)
      FrMemoryStats() ;
   return ;
}

//----------------------------------------------------------------------

extern "C" void scan_strings(const class scanner_params &sp,
			     const class recursion_control_block &rcb) ;

void scan_strings(const class scanner_params &sp,
			     const class recursion_control_block &rcb)
{
   (void)rcb ; // not used; keep compiler happy
   // ensure that we get the correct format parameter block
   assert(sp.sp_version == scanner_params::CURRENT_SP_VERSION) ;
//   debug = sp.info->config->debug ;  // causes a segfault after returning to BE!!?!?!
   if ((debug & DEBUG_PRINT_STEPS) != 0)
      {
      cerr << "Invoked scan_strings(), phase = " << sp.phase << endl << flush ;
      }
   switch (sp.phase)
      {
      case scanner_params::PHASE_NONE:
	 // do nothing
	 break ;
      case scanner_params::PHASE_STARTUP:
	 startup(sp) ;
	 break ;
      case scanner_params::PHASE_INIT:
	 initialize(sp) ;
	 break ;
      case scanner_params::PHASE_THREAD_BEFORE_SCAN:
	 thread_start() ;
	 break ;
      case scanner_params::PHASE_SCAN:
	 process_buffer(sp.sbuf,sp.fs.get_name("strings"),
			cfg_printenc ? sp.fs.get_name("encodings") : 0) ;
	 break ;
#if 0
      case scanner_params::PHASE_THREAD_AFTER_SCAN:
	 thread_finish() ;
	 break ;
#endif
      case scanner_params::PHASE_SHUTDOWN:
	 cleanup() ;
	 break ;
      default:
	 fprintf(stderr,"Invalid 'phase' parameter to scan_strings\n") ;
	 break ;
      }
   if ((debug & DEBUG_PRINT_STEPS) != 0 &&
       sp.phase != scanner_params::PHASE_SCAN)
      {
      cerr << "  ==> scan_strings(), phase = " << sp.phase << endl ;
      }
   return ;
}

#endif /* BULK_EXTRACTOR */

// end of file scan_strings.C //
