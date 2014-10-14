/************************************************************************/
/*                                                                      */
/*	LangIdent: long n-gram-based language identification		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     score.C	evaluation of language identification results	*/
/*  Version:  1.24							*/
/*  LastEdit: 04aug2014 						*/
/*                                                                      */
/*  (c) Copyright 2012,2013,2014 Ralf Brown/Carnegie Mellon University	*/
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

#include "FramepaC.h"
#include <iomanip>

/************************************************************************/
/************************************************************************/

class Counts : public FrHashEntryObject
   {
   private:
      static FrAllocator allocator ;
      size_t m_correct ;
      size_t m_incorrect ;
      size_t m_fuzzymatch ;
   public:
      void *operator new(size_t) { return allocator.allocate() ; }
      void operator delete(void *obj) { allocator.release(obj) ; }
      Counts(FrSymbol *key)
	 : FrHashEntryObject(key,0,false)
	 { m_correct = m_incorrect = m_fuzzymatch = 0 ; }
      ~Counts() {}
      virtual FrObject *copy() const ;
      virtual FrObject *deepcopy() const ;

      // accessors
      size_t correct() const { return m_correct ; }
      size_t incorrect() const { return m_incorrect ; }
      size_t fuzzy() const { return m_fuzzymatch ; }

      // modifiers
      void update(bool is_correct, bool fuzzy_match = false)
	 {
	    if (is_correct)
	       m_correct++ ;
	    else
	       m_incorrect++ ;
	    if (fuzzy_match)
	       m_fuzzymatch++ ;
	 }
   } ;

//----------------------------------------------------------------------

FrAllocator Counts::allocator("Counts",sizeof(Counts)) ;

/************************************************************************/
/*	Methods for class Counts					*/
/************************************************************************/

FrObject *Counts::copy() const
{
   Counts *result = new Counts((FrSymbol*)obj) ;
   if (result)
      {
      result->user_data = user_data ;
      result->m_correct = m_correct ;
      result->m_incorrect = m_incorrect ;
      result->m_fuzzymatch = m_fuzzymatch ;
      }
   return result ;
}

//----------------------------------------------------------------------

FrObject *Counts::deepcopy() const
{
   return copy() ;
}

/************************************************************************/
/************************************************************************/

static void usage(const char *argv0)
{

   exit(2) ;
}

//----------------------------------------------------------------------

static void trim_line(char *line, bool strip_multiple)
{
   char *tab = strchr(line,'\t') ;
   if (tab)
      *tab = '\0' ;
   char *nl = strchr(line,'\n') ;
   if (nl)
      *nl = '\0' ;
   if (strip_multiple)
      {
      char *comma = strchr(line,',') ;
      if (comma)
	 *comma = '\0' ;
      }
   return ;
}

//----------------------------------------------------------------------

static void update_stats(FrHashTable &ht, char *key, char *eval, bool show_fuzzy)
{
   FrSymbol *keysym = FrSymbolTable::add(key) ;
   Counts keyentry(keysym) ;
   Counts *entry = (Counts*)ht.add(&keyentry) ;
   if (entry)
      {
      bool correct ;
      bool fuzzy = false ;
      if (show_fuzzy)
	 {
	 correct = (strcmp(key,eval) == 0) ; //FIXME
	 fuzzy = false ; //FIXME
	 }
      else
	 {
	 correct = (strcmp(key,eval) == 0) ;
	 }
      entry->update(correct,fuzzy) ;
      }
   return ;
}

//----------------------------------------------------------------------

static bool collect_stats(FrHashEntry *entry, va_list args)
{
   FrVarArg(FrList**,stats) ;
   if (stats)
      {
      const Counts *counts = (Counts*)entry ;
      FrList *item = new FrList(counts->getObject(),
				new FrInteger(counts->correct()),
				new FrInteger(counts->incorrect()),
				new FrInteger(counts->fuzzy())) ;
      pushlist(item,*stats) ;
      }
   return true ;
}

//----------------------------------------------------------------------

static int compare_languages(const FrObject *o1, const FrObject *o2)
{
   const FrList *l1 = (FrList*)o1 ;
   const FrList *l2 = (FrList*)o2 ;
   FrSymbol *key1 = (FrSymbol*)l1->first() ;
   FrSymbol *key2 = (FrSymbol*)l2->first() ;
   if (key1 && key2)
      return strcmp(key1->symbolName(),key2->symbolName()) ;
   return 0 ;
}

//----------------------------------------------------------------------

static void print_stats(FrHashTable &ht, bool show_fuzzy)
{
   FrList *stats = 0 ;
   ht.doHashEntries(collect_stats,&stats,0) ;
   if (!stats)
      {
      cout << "No statistics" << endl ;
      return ;
      }
   FrList *sorted_stats = stats->sort(compare_languages) ;
   unsigned num_langs = 0 ;
   double global_error_rate = 0.0 ;
   size_t global_incorrect = 0 ;
   size_t global_total = 0 ;
   if (show_fuzzy)
      {
      printf("Rate(%%)\tErrors\tTotal\tFuzzy\tLanguage\n") ;
      printf("=========================================\n") ;
      }
   else
      {
      printf("Rate(%%)\tErrors\tTotal\tLanguage\n") ;
      printf("=================================\n") ;
      }
   while (sorted_stats)
      {
      num_langs++ ;
      stats = (FrList*)poplist(sorted_stats) ;
      size_t correct = stats->second()->intValue() ;
      size_t incorrect = stats->third()->intValue() ;
      size_t fuzzy = stats->nth(3)->intValue() ;
      size_t total = correct + incorrect ;
      const char *lang = ((FrSymbol*)stats->first())->symbolName() ;
      double error_rate = total ? (incorrect / ((double)total)) : 0.0 ;
      if (show_fuzzy)
	 {
	 printf("%7.2f\t%6lu\t%6lu\t%6lu\t%s\n",100.0 * error_rate,incorrect,total,fuzzy,lang) ;
	 }
      else
	 {
	 printf("%7.2f\t%6lu\t%6lu\t%s\n",100.0 * error_rate,incorrect,total,lang) ;
	 }
      free_object(stats) ;
      global_error_rate += error_rate ;
      global_incorrect += incorrect ;
      global_total += total ;
      }
   if (num_langs > 0)
      {
      if (show_fuzzy)
	 printf("=========================================\n") ;
      else
	 printf("=================================\n") ;
      printf("LANGUAGE COUNT = %u\n",num_langs) ;
      printf("AVERAGES:  micro = %7.3f%%\tmacro = %7.3f%%\n",
	     100.0 * (global_incorrect / (double)global_total),
	     100.0 * (global_error_rate / num_langs)) ;
      }
   return ;
}

//----------------------------------------------------------------------

static bool score_results(FILE *keyfp, FILE *evalfp, bool show_fuzzy)
{
   FrHashTable ht ;
   ht.expandTo(2049) ;
   size_t line_count = 0 ;
   while (!feof(keyfp) && !feof(evalfp))
      {
      char key[FrMAX_LINE] ;
      char eval[FrMAX_LINE] ;
      bool got_key = fgets(key,sizeof(key),keyfp) != 0 ;
      bool got_eval = fgets(eval,sizeof(eval),evalfp) != 0 ;
      if (!got_key && !got_eval)
	 break ;
      else if (!got_key || !got_eval)
	 {
	 cerr << "Error reading from file after " << line_count << " lines"
	      << endl ;
	 return false ;
	 }
      line_count++ ;
      trim_line(key,false) ;
      trim_line(eval,!show_fuzzy) ;
      update_stats(ht,key,eval,show_fuzzy) ;
      }
   print_stats(ht,show_fuzzy) ;
   if (!feof(keyfp))
      {
      cerr << "Evaluation output is too short" << endl ;
      return false ;
      }
   else if (!feof(evalfp))
      {
      cerr << "Key file is too short" << endl ;
      return false ;
      }
   return true ;
}

//----------------------------------------------------------------------

int main(int argc, char **argv)
{
   bool show_fuzzy = false ;
   if (argv[1] && strcmp(argv[1],"-f") == 0)
      {
      show_fuzzy = true ;
      argv++ ;
      argc-- ;
      }
   if (argc < 2)
      usage(argv[0]) ;
   const char *keyfile = argv[1] ;
   const char *evalfile = argv[2] ;

   FILE *keyfp = fopen(keyfile,"r") ;
   FILE *evalfp = fopen(evalfile,"r") ;
   bool success = false ;
   if (keyfp && evalfp)
      {
      success = score_results(keyfp,evalfp,show_fuzzy) ;
      }
   if (keyfp)
      fclose(keyfp) ;
   if (evalfp)
      fclose(evalfp) ;
   return success ? 0 : 1 ;
}
