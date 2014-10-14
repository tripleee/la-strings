/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC								*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File: frtfidf.cpp	      TF*IDF term weights			*/
/*  LastEdit: 04nov09							*/
/*									*/
/*  (c) Copyright 1999,2000,2001,2002,2003,2009 Ralf Brown		*/
/*	This program is free software; you can redistribute it and/or	*/
/*	modify it under the terms of the GNU General Public License as	*/
/*	published by the Free Software Foundation, version 3.		*/
/*									*/
/*	This program is distributed in the hope that it will be		*/
/*	useful, but WITHOUT ANY WARRANTY; without even the implied	*/
/*	warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR		*/
/*	PURPOSE.  See the GNU General Public License for more details.	*/
/*									*/
/*	You should have received a copy of the GNU General Public	*/
/*	License (file COPYING) along with this program.  If not, see	*/
/*	http://www.gnu.org/licenses/					*/
/*									*/
/************************************************************************/

#include <math.h>
#include <stdlib.h>
#include "framerr.h"
#include "frfilutl.h"
#include "frreader.h"
#include "frsymhsh.h"
#include "frsymtab.h"
#include "frtrmvec.h"
#include "frutil.h"

/************************************************************************/
/*	Manifest Constants						*/
/************************************************************************/

#define DEFAULT_IDF 1000.0

/************************************************************************/
/************************************************************************/

/************************************************************************/
/*	Global Variables for this module				*/
/************************************************************************/

FrAllocator FrTFIDFrecord::allocator("FrTFIDFrecord",sizeof(FrTFIDFrecord)) ;

/************************************************************************/
/*	Helper functions						*/
/************************************************************************/

/************************************************************************/
/*	Methods for class FrTFIDFrecord					*/
/************************************************************************/

double FrTFIDFrecord::invDocFrequency(size_t total_docs) const
{
   if (doc_freq)
      return ::log(total_docs / (double)doc_freq) ;
   else
//      return DEFAULT_IDF ;
      return ::log(total_docs + 1) ;	// assume it occurs in next doc
}

//----------------------------------------------------------------------

double FrTFIDFrecord::TF_IDF(size_t total_docs) const
{
   if (doc_freq)
      {
      if (doc_freq > total_docs + 1)
	 return 0.0 ;			// this is a stopword
      else
	 return term_freq * ::log(total_docs / (double)doc_freq) / doc_freq ;
      }
   else
//      return DEFAULT_IDF ;
      return ::log(total_docs + 1) ;	// assume it occurs once in next doc
}

/************************************************************************/
/*	Methods for class FrTFIDF					*/
/************************************************************************/

FrTFIDF::FrTFIDF()
{
   total_docs = 0 ;
   ht = new FrSymHashTable ;
   ht->expand(1000) ;
   markClean() ;
   return ;
}

//----------------------------------------------------------------------

static bool copy_tfidf_record(FrSymHashEntry *entry, va_list)
{
   FrTFIDFrecord *rec = (FrTFIDFrecord*)entry->getUserData() ;
   if (rec)
      entry->setUserData(new FrTFIDFrecord(rec)) ;
   return true ;			// continue iterating
}

//----------------------------------------------------------------------

FrTFIDF::FrTFIDF(const FrTFIDF *orig)
{
   if (orig)
      {
      if (orig->ht)
	 {
	 ht = orig->ht->deepcopy() ;
	 ht->doHashEntries(copy_tfidf_record) ;
	 }
      else
	 {
	 ht = new FrSymHashTable ;
	 ht->expand(1000) ;
	 }
      total_docs = orig->total_docs ;
      markClean() ;
      }
   else
      {
      total_docs = 0 ;
      ht = new FrSymHashTable ;
      ht->expand(1000) ;
      markClean() ;
      }
   return ;
}

//----------------------------------------------------------------------

FrTFIDF::FrTFIDF(const char *filename)
{
   total_docs = 0 ;
   ht = 0 ;
   load(filename) ;
   if (!ht)
      {
      // put in a small dummy hash table
      ht = new FrSymHashTable ;
      ht->expand(100) ;
      }
   markClean() ;
   return ;
}

//----------------------------------------------------------------------

bool FrTFIDF::load(const char *filename)
{
   if (filename && *filename)
      {
      FrITextFile wt(filename) ;
      if (!wt.good())
	 {
	 FrWarningVA("unable to open term weights file '%s'",filename) ;
	 return false ;
	 }
      delete ht ;
      ht = new FrSymHashTable ;
      FrSymbol *symEOF = FrSymbolTable::add("*EOF*") ;
      char *line = wt.getline() ;
      bool expanded = false ;
      if (line && strncmp(line,"!!! ",4) == 0)
	 {
	 char *end = 0 ;
	 total_docs = (size_t)strtol(line+4,&end,10) ;
	 if (end && end != line+4)
	    {
	    char *tmp = end ;
	    size_t vocab_size = (size_t)strtol(tmp,&end,10) ;
	    if (vocab_size > 0 && end && end != tmp)
	       {
	       ht->expand(vocab_size+100) ;
	       expanded = true ;
	       }
	    }
	 }
      if (!expanded)			// ensure some reasonable starting size
	 ht->expand(5000) ;
      while ((line = wt.getline()) != 0)
	 {
	 if (FrSkipWhitespace(line) == ';' || *line == '\0')
	    continue ;
	 const char *origline = line ;
	 FrSymbol *term = (FrSymbol*)string_to_FrObject(line) ;
	 if (term == symEOF || !term || !term->symbolp())
	    {
	    FrWarning("invalid line in term-weights file") ;
	    free_object(term) ;
	    continue ;
	    }
	 char *end = 0 ;
	 size_t term_freq = strtol(line,&end,10) ;
	 if (end && end != line)
	    {
	    line = end ;
	    size_t doc_freq = strtol(line,&end,10) ;
	    if (end != line)
	       {
	       if (doc_freq > 0 && term_freq > 0)
		  {
		  FrSymHashEntry *entry = tfidfRecord(term) ;
		  FrTFIDFrecord *rec = new FrTFIDFrecord(term_freq,doc_freq) ;
		  if (entry)
		     {
		     delete (FrTFIDFrecord*)entry->getUserData() ;
		     entry->setUserData(rec) ;
		     }
		  else
		     ht->add(term,(void*)rec) ;
		  continue ;
		  }
	       FrWarning("invalid data in term-weights file -- both term\n"
			 "\tand document frequencies must be nonzero") ;
	       free_object(term) ;
	       continue ;
	       }
	    }
	 FrWarningVA("expected two integers following the term '%s'; line was\n"
		     "\t%s", term->symbolName(), origline) ;
	 free_object(term) ;
	 }
      return true ;
      }
   return false ;
}

//----------------------------------------------------------------------

static bool erase_tfidf_records(FrSymHashEntry *entry, va_list)
{
   FrTFIDFrecord *rec = (FrTFIDFrecord*)entry->getUserData() ;
   delete rec ;
   return true ;
}

//----------------------------------------------------------------------

FrTFIDF::~FrTFIDF()
{
   if (ht)
      {
      ht->doHashEntries(erase_tfidf_records) ;
      delete ht ;
      ht = 0 ;
      }
   total_docs = 0 ;
   return ;
}

//----------------------------------------------------------------------

static bool write_tfidf(FrSymHashEntry *entry, va_list args)
{
   if (entry)
      {
      FrVarArg(FILE*,fp) ;
      const FrSymbol *term = entry->getName() ;
      FrTFIDFrecord *rec = (FrTFIDFrecord*)entry->getUserData() ;
      if (term && rec && rec->termFrequency() > 0 && rec->docFrequency() > 0)
	 {
	 FrVarArg2(bool,int,verbosely) ;
	 char *termname = term->print() ;
	 if (verbosely)
	    {
	    FrVarArg(size_t,total_docs) ;
	    fprintf(fp,"%s %ld %ld %g\n",termname,
		    (unsigned long)rec->termFrequency(),
		    (unsigned long)rec->docFrequency(),
		    rec->TF_IDF(total_docs)) ;
	    }
	 else
	    fprintf(fp,"%s %ld %ld\n",termname,
		    (unsigned long)rec->termFrequency(),
		    (unsigned long)rec->docFrequency()) ;
	 FrFree(termname) ;
	 }
      }
   return true ;			// continue iterating
}

//----------------------------------------------------------------------

bool FrTFIDF::save(FILE *fp, bool verbosely)
{
   if (dirty() && fp)
      {
      fprintf(fp,"!!! %ld\t%ld\t\tTF*IDF data file\n",
	      (unsigned long)total_docs,
	      (unsigned long)(ht ? ht->currentSize() : 0)) ;
      bool success = true ;
      if (ht)
	 success = ht->doHashEntries(write_tfidf,fp,verbosely,total_docs) ;
      if (success)
	 markClean() ;
      return success ;
      }
   else
      return false ;
}

//----------------------------------------------------------------------

bool FrTFIDF::save(const char *filename, bool verbosely)
{
   if (dirty() && filename && *filename)
      {
      FILE *fp = fopen(filename,"w") ;
      if (fp)
	 {
	 save(fp,verbosely) ;
	 fclose(fp) ;
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

void FrTFIDF::incr(FrSymbol *term, size_t tf)
{
   FrSymHashEntry *entry = tfidfRecord(term) ;
   if (entry)
      {
      FrTFIDFrecord *rec = (FrTFIDFrecord*)entry->getUserData() ;
      if (rec)
	 rec->incr(tf) ;
      else
	 entry->setUserData(new FrTFIDFrecord(tf,1)) ;
      }
   else
      {
      entry = ht->add(term,(void*)0) ;
      entry->setUserData(new FrTFIDFrecord(tf,1)) ;
      }
   return ;
}

//----------------------------------------------------------------------

double FrTFIDF::defaultIDF() const
{
   return DEFAULT_IDF ;
}

//----------------------------------------------------------------------

double FrTFIDF::tf_idf(FrSymbol *term) const
{
   if (term)
      {
      FrSymHashEntry *entry = tfidfRecord(term) ;
      if (entry)
	 {
	 FrTFIDFrecord *rec = (FrTFIDFrecord*)entry->getUserData() ;
	 if (rec)
	    return rec->TF_IDF(total_docs) ;
	 }
      }
   return defaultIDF() ;
}

//----------------------------------------------------------------------

double FrTFIDF::invDocFreq(FrSymbol *term) const
{
   if (term)
      {
      FrSymHashEntry *entry = tfidfRecord(term) ;
      if (entry)
	 {
	 FrTFIDFrecord *rec = (FrTFIDFrecord*)entry->getUserData() ;
	 if (rec)
	    return rec->invDocFrequency(total_docs) ;
	 }
      }
   return defaultIDF() ;
}

//----------------------------------------------------------------------

// end of file frtfidf.cpp //
