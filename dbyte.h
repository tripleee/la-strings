/****************************** -*- C++ -*- *****************************/
/*									*/
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File: dbyte.h - representation of a byte or back-reference		*/
/*  Version:  1.00				       			*/
/*  LastEdit: 26aug2011							*/
/*									*/
/*  (c) Copyright 2011 Ralf Brown/CMU					*/
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

#ifndef __DBYTE_H_INCLUDED
#define __DBYTE_H_INCLUDED

#include <cstdio>
#include <stdint.h>

#include <unistd.h>	// for off_t

using namespace std ;

/************************************************************************/
/*	Manifest Constants						*/
/************************************************************************/

#define DEFAULT_UNKNOWN '?'

#define DECODEDBYTE_SIGNATURE "Recovered Lempel-Ziv Data Stream\n\n\n"

#define DBYTE_MASK_LITERAL       0xFFF000
#define DBYTE_RECONSTRUCTED      0xFFE000

#define DBYTE_MASK_CONFIDENCE    0x000F00
#define DBYTE_SHIFT_CONFIDENCE   8
#define DBYTE_CONFIDENCE_USER    0x000F00
#define DBYTE_CONFIDENCE_UNKNOWN 0x000000
#define DBYTE_CONFIDENCE_LEVELS  14   // four bits, less "user" and "unknown"

#define DBYTE_MASK_TYPE          0x001F00 // literal bit + conf = non-ptr type
#define DBYTE_SHIFT_TYPE         8
#define DBYTE_LIT_TYPE(x) (((x) & DBYTE_MASK_TYPE) >> DBYTE_SHIFT_TYPE)

#define MAX_REFERENCE_WINDOW (64 * 1024)

/************************************************************************/
/*	Utility macros							*/
/************************************************************************/

#ifndef lengthof
#define lengthof(x) (sizeof(x) / sizeof(x[0]))
#endif

/************************************************************************/
/*	Types								*/
/************************************************************************/

enum ByteType
   {
      BT_Unknown,
      BT_WildGuess,
      BT_Guessed,
      BT_Reconstructed,
      BT_UserSupplied,
      BT_Literal
   } ;

//----------------------------------------------------------------------

enum WriteFormat
   {
      WFMT_None,
      WFMT_PlainText,
      WFMT_DecodedByte,
      WFMT_HTML,
      WFMT_Listing
   } ;

//----------------------------------------------------------------------

class DecodedByte
   {
   private:
      // we use 24 bits in the file, but there is no convenient standard
      //  type of that size, so use 32 bits in RAM
      uint32_t m_byte_or_pointer ;
      static ByteType s_prev_bytetype ;
      static const ByteType s_confidence_to_type[] ;
      static size_t s_total_bytes ;	// statistics for WFMT_Listing
      static size_t s_known_bytes ;
      static size_t s_original_size ;
      static uint64_t s_global_total_bytes ;
      static uint64_t s_global_known_bytes ;
      static uint64_t s_global_original_size ;
   protected:
      ByteType byteType_raw() const
	 { return s_confidence_to_type[DBYTE_LIT_TYPE(m_byte_or_pointer)] ; }
      ByteType prevByteType() const { return s_prev_bytetype ; }
      static void prevByteType(ByteType bt) { s_prev_bytetype = bt ; }
   public:
      DecodedByte(const DecodedByte &orig)
	 { m_byte_or_pointer = orig.m_byte_or_pointer ; }
      DecodedByte(uint8_t byte) { setByteValue(byte) ; }
      DecodedByte() { m_byte_or_pointer = 0 ; }
      ~DecodedByte() {}

      DecodedByte &operator = (const DecodedByte &orig)
	 { m_byte_or_pointer = orig.m_byte_or_pointer ; return *this ; }
      DecodedByte &operator = (uint8_t byte)
	 { setByteValue(byte) ; return *this ; }
      
      // accessors
      bool isLiteral() const
	 { return m_byte_or_pointer >= DBYTE_RECONSTRUCTED ; }
      ByteType byteType() const
	 { return isLiteral() ? byteType_raw() : BT_Unknown ; }
      unsigned confidence() const
	 { return (m_byte_or_pointer & DBYTE_MASK_CONFIDENCE) >> DBYTE_SHIFT_CONFIDENCE ; }
      uint8_t byteValue() const { return m_byte_or_pointer & 0xFF ; }
      uint32_t originalLocation() const { return m_byte_or_pointer ; }

      static uint64_t globalTotalBytes() { return s_global_total_bytes ; }
      static uint64_t globalKnownBytes() { return s_global_known_bytes ; }
      static uint64_t globalOriginalSize() { return s_global_original_size ; }

      // manipulators
      static void setOriginalSize(size_t size)
	 { s_original_size = size ; s_global_original_size += size ; }
      static void clearCounts() ;
      void setOriginalLocation(uint32_t loc) { m_byte_or_pointer = loc ; }
      void setByteValue(uint8_t byte)
	 { m_byte_or_pointer = DBYTE_MASK_LITERAL | byte ; }
      void setReconstructed(uint8_t byte, unsigned conf)
	 { m_byte_or_pointer = (DBYTE_RECONSTRUCTED
				| ((conf << DBYTE_SHIFT_CONFIDENCE)
				   & DBYTE_MASK_CONFIDENCE)
				| byte) ; }
      void setConfidence(unsigned conf)
	 { m_byte_or_pointer = ((m_byte_or_pointer & ~DBYTE_MASK_CONFIDENCE)
				| (conf << DBYTE_SHIFT_CONFIDENCE)) ; }

      // I/O
      bool read(FILE *infp) ;
      bool write(FILE *outfp, WriteFormat, unsigned char unknown_char) const ;
      static bool writeHeader(WriteFormat, FILE *,
			      DecodedByte *replacements = 0,
			      size_t num_replacements = 0) ;
      static bool writeBuffer(const DecodedByte *buf, size_t n_elem,
			      FILE *outfp, WriteFormat fmt = WFMT_PlainText,
			      unsigned char unknown_char = DEFAULT_UNKNOWN) ;
      static bool writeFooter(WriteFormat, FILE *, const char *filename) ;
   } ;

//----------------------------------------------------------------------

class DecodeBuffer
   {
   private:
      DecodedByte   m_buffer[MAX_REFERENCE_WINDOW] ;
      DecodedByte  *m_replacements ;
      unsigned      m_bufptr ;
      size_t        m_numreplacements ;
      size_t        m_numbytes ;
      FILE         *m_infp ;
      FILE         *m_outfp ;
      const char   *m_filename ;	// filename for WFMT_Listing
      off_t	    m_datastart ;
      WriteFormat   m_format ;
      unsigned char m_unknown ;
   public:
      DecodeBuffer(FILE *fp = 0,
		   WriteFormat = WFMT_PlainText,
		   unsigned char unk = DEFAULT_UNKNOWN,
		   const char *friendly_filename = 0) ;
      ~DecodeBuffer() ;

      // accessors
      FILE *inputFile() const { return m_infp ; }
      FILE *outputFile() const { return m_outfp ; }
      unsigned offset() const { return m_bufptr ; }
      WriteFormat writeFormat() const { return m_format ; }
      unsigned char unknownChar() const { return m_unknown ; }
      const DecodedByte *replacements() const { return m_replacements ; }
      bool haveReplacement(size_t which) const
	 { return replacements() && which <= numReplacements()
	       ? replacements()[numReplacements()-which].isLiteral() : false ; }
      size_t numReplacements() const { return m_numreplacements ; }
      size_t totalBytes() const { return m_numbytes ; }
      DecodedByte *copyReplacements() const ;
      const char *friendlyFilename() const { return m_filename ; }
      DecodedByte *loadBytes(bool sentinel = false) ;

      // manipulators
      void rewind() { m_bufptr = 0 ; }
      void rewindInput() ;
      bool openInputFile(FILE *fp) ;
      bool setOutputFile(FILE *fp, WriteFormat fmt, unsigned char unk = '?',
			 const char *friendlyfile = 0) ;
      bool initReplacements() ;
      bool setReplacements(const DecodedByte *repl, size_t num_repl) ;
      bool setReplacement(size_t which, const DecodedByte &repl) ;
      bool setReplacement(size_t which, uint8_t c, unsigned confidence) ;
      bool addByte(DecodedByte b) ;
      bool addByte(unsigned char b) ;
      bool addString(const char *s) ;
      bool copyString(unsigned length, unsigned offset) ;
      bool applyReplacements() ;
      bool applyReplacement(DecodedByte &db) const ;
   } ;

#endif /* !__DBYTE_H_INCLUDED */

// end of file dbyte.h //
