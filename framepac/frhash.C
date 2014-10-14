/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC -- frame manipulation in C++				*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frhash.cpp		class FrHashTable 			*/
/*  LastEdit: 23aug11							*/
/*									*/
/*  (c) Copyright 1994,1995,1996,1997,1998,2000,2001,2003,2004,2009,    */
/*		2011 Ralf Brown						*/
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

#if defined(__GNUC__)
#  pragma implementation "frhash.h"
#endif

#include "frcmove.h"
#include "frhash.h"
#include "frutil.h"
#include "frpcglbl.h"

#ifdef FrSTRICT_CPLUSPLUS
#  include <cstdlib>
#  include <iomanip>
#else
#  include <iomanip.h>
#  include <stdlib.h>
#endif /* FrSTRICT_CPLUSPLUS */

/************************************************************************/
/*    Global data local to this module					*/
/************************************************************************/

static const char str_NIL[] = "NIL" ;
static const char str_hash_intro[] = "#H(" ;

static size_t warn_count = 0 ;

/************************************************************************/
/*    Global variables for class FrHashTable				*/
/************************************************************************/

static FrObject *read_HashTable(istream &input, const char *digits) ;
static FrObject *string_to_HashTable(const char *&input, const char *digits) ;
static bool verify_HashTable(const char *&input, const char *digits,bool) ;

FrReader FrHashTable::reader(string_to_HashTable,read_HashTable,
			      verify_HashTable,
			      FrREADER_LEADIN_LISPFORM,"H") ;

/************************************************************************/
/*    Methods for class FrHashEntry					*/
/************************************************************************/

FrHashEntry::~FrHashEntry()
{
   if (in_use)
      FrProgError("deleted FrHashEntry which was still in use") ;
   return ;
}

//----------------------------------------------------------------------

bool FrHashEntry::hashp() const
{
   return true ;
}

//----------------------------------------------------------------------

const char *FrHashEntry::objTypeName() const
{
   return "FrHashEntry" ;
}

//----------------------------------------------------------------------

FrObjectType FrHashEntry::objType() const
{
   return OT_FrHashEntry ;
}

//----------------------------------------------------------------------

FrObjectType FrHashEntry::objSuperclass() const
{
   return OT_FrObject ;
}

//----------------------------------------------------------------------

FrHashEntryType FrHashEntry::entryType() const
{
   return HE_base ;
}

//----------------------------------------------------------------------

FrSymbol *FrHashEntry::entryName() const
{
   return 0 ;
}

//----------------------------------------------------------------------

int FrHashEntry::sizeOf() const
{
   return sizeof(FrHashEntry) ;
}

//----------------------------------------------------------------------

size_t FrHashEntry::hashIndex(int size) const
{
   return (size_t)((unsigned long)this % size) ;
}

//----------------------------------------------------------------------

int FrHashEntry::keycmp(const FrHashEntry *entry) const
{
   return FrCompare(this,entry) ;
}

//----------------------------------------------------------------------

bool FrHashEntry::prefixMatch(const char *nameprefix,int len) const
{
   (void)nameprefix ; (void)len ;  // avoid compiler warning
   return false ;
}

//----------------------------------------------------------------------

FrObject *FrHashEntry::copy() const
{
   FrHashEntry *result = new FrHashEntry ;

   if (result)
      {
      }
   return result ;
}

//----------------------------------------------------------------------

FrObject *FrHashEntry::deepcopy() const
{
   FrHashEntry *result = new FrHashEntry ;

   if (result)
      {
      }
   return result ;
}

/************************************************************************/
/*    Methods for class FrHashTable					*/
/************************************************************************/

FrHashTable::FrHashTable()
{
   curr_elements = max_elements = size = 0 ;
   elements = 0 ;
   fill_factor = HASHTABLE_FILL_FACTOR ;
   return ;
}

//----------------------------------------------------------------------

FrHashTable::FrHashTable(int fill_fact)
{
   curr_elements = max_elements = size = 0 ;
   elements = 0 ;
   fill_factor = (fill_fact > 0) ? fill_fact : HASHTABLE_FILL_FACTOR ;
   return ;
}

//----------------------------------------------------------------------

FrHashTable::~FrHashTable()
{
   if (elements)
      {
      for (FrHashEntry **elt = &elements[size-1] ; elt >= elements ; elt--)
	 {
	 FrHashEntry *entry = *elt ;
	 while (entry)
	    {
	    FrHashEntry *tmp = entry ;
	    entry->in_use = false ;
	    entry = entry->next ;
	    delete tmp ;
	    }
	 }
      FrFree(elements) ;
      elements = 0 ;
      }
   curr_elements = max_elements = size = 0 ;
   return ;
}

//----------------------------------------------------------------------

void FrHashTable::freeObject()
{
   delete this ;
   return ;
}

//----------------------------------------------------------------------

const char *FrHashTable::objTypeName() const
{
   return "FrHashTable" ;
}

//----------------------------------------------------------------------

FrObjectType FrHashTable::objType() const
{
   return OT_FrHashTable ;
}

//----------------------------------------------------------------------

FrObjectType FrHashTable::objSuperclass() const
{
   return OT_FrObject ;
}

//----------------------------------------------------------------------

bool FrHashTable::hashp() const
{
   return true ;
}

//----------------------------------------------------------------------

FrHashEntryType FrHashTable::entryType() const
{
   return HE_none ;
}

//----------------------------------------------------------------------

size_t FrHashTable::length() const
{
   return curr_elements ;
}

//----------------------------------------------------------------------

bool FrHashTable::iterateVA(FrIteratorFunc func, va_list args) const
{
   for (FrHashEntry **elt = &elements[size-1] ; elt >= elements ; elt--)
      {
      FrHashEntry *entry = *elt ;
      while (entry)
         {
	 FrHashEntry *next = entry->next ;
	 FrSafeVAList(args) ;
	 bool success = true ;
	 if (entry->entryType() == HE_FrObject)
	    {
	    if (!func(((FrHashEntryObject*)entry)->getObject(),
		      FrSafeVarArgs(args)))
	       success = false ;
	    }
	 else
	    {
	    if (!func(entry,FrSafeVarArgs(args)))
	       success = false ;
	    }
	 FrSafeVAListEnd(args) ;
	 if (!success)
	    return false ;
	 entry = next ;
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

bool FrHashTable::expand(size_t increment)
{
   size_t oldsize = size ;
   size_t i ;

   if (size + HASHTABLE_MIN_INCREMENT > MAX_HASHTABLE_SIZE)
      return false ; // don't expand if we'd be forced to exceed the max size
   if (increment < HASHTABLE_MIN_INCREMENT)
      increment = HASHTABLE_MIN_INCREMENT ;
   size += increment ;
   if (size >= MAX_HASHTABLE_SIZE)
      size = MAX_HASHTABLE_SIZE ;
   else if ((size & 1) == 0)		// try to keep size odd
      size++ ;
   if ((size % 3) == 0)			//   and also not a multiple of 3
      size += 2 ;
   if (size == oldsize)
      return true ;			// successful -- nothing to do
   max_elements = size * fill_factor ;
   FrHashEntry *oldentries = 0 ;
   FrHashEntryPtr *newelts = FrNewR(FrHashEntryPtr,elements,size) ;
   if (newelts)
      {
      elements = newelts ;
      for (i = 0 ; i < oldsize ; i++)
	 {
	 FrHashEntry *element = elements[i] ;
	 if (element)
	    {
	    FrHashEntry *tail = element ;

	    while (tail->next)
	       tail = tail->next ;
	    tail->next = oldentries ;
	    oldentries = element ;
	    }
	 }
      // clear the expanded table
      for (i = 0 ; i < size ; i++)
	 elements[i] = 0 ;
      // re-insert existing hash table entries into the newly-expanded table
      while (oldentries)
	 {
	 FrHashEntry *old_elt = oldentries ;
	 size_t pos = old_elt->hashIndex(size) ;
	 oldentries = oldentries->next ;
	 old_elt->next = elements[pos] ;
	 elements[(size_t)pos] = old_elt ;
	 }
      return true ;
      }
   else
      {
      size = oldsize ;
      if (warn_count < 100)
	 {
	 FrWarning("unable to expand hash table--will continue") ;
	 warn_count++ ;
	 }
      return false ;
      }
}

//----------------------------------------------------------------------

bool FrHashTable::expandTo(size_t newsize)
{
   if (newsize > size)
      return expand(newsize-size) ;
   else
      return true ;			// trivially successful
}

//----------------------------------------------------------------------

FrObject *FrHashTable::deepcopy() const
{
   FrHashTable *result = new FrHashTable(fill_factor) ;
   if (result)
      {
      result->size = size ;
      result->max_elements = max_elements ;
      result->curr_elements = curr_elements ;
      result->elements = FrNewN(FrHashEntryPtr,size) ;
      if (result->elements)
	 {
	 for (size_t i = 0 ; i < size ; i++)
	    {
	    if (elements[i])
	       {
	       FrHashEntry *rentry = 0 ;
	       FrHashEntry *entry = elements[i] ;
	       for ( ; entry ; entry = entry->next)
		  {
		  FrHashEntry *newentry = (FrHashEntry*)entry->deepcopy() ;
		  if (rentry)
		     rentry->next = newentry ;
		  else
		     result->elements[i] = newentry ;
		  rentry = newentry ;
		  }
	       rentry->next = 0 ;
	       }
	    else
	       result->elements[i] = 0 ;
	    }
	 }
      else
	 {
	 result->size = 0 ;
	 result->curr_elements = 0 ;
	 result->max_elements = 0 ;
	 }
      }
   return result ;
}

//----------------------------------------------------------------------

FrHashEntry *FrHashTable::add(const FrHashEntry *entry)
{
   register size_t pos = entry->hashIndex(size) ;
   FrHashEntry *new_elt = elements[pos] ;
   while (new_elt)
      if (entry->keycmp(new_elt) == 0)
         return new_elt ;
      else
         new_elt = new_elt->next ;
   if ((new_elt = (FrHashEntry*)entry->copy()) == 0)
      return 0 ;
   new_elt->next = elements[pos] ;
   new_elt->in_use = true ;
   elements[pos] = new_elt ;
   if (++curr_elements >= max_elements)
      expand(3*size/4) ;   // expand hash table by 75%
   return new_elt ;
}

//----------------------------------------------------------------------

int FrHashTable::remove(const FrHashEntry *entry)
{
   size_t pos = entry->hashIndex(size) ;
   register FrHashEntry *elt, *prev ;

   elt = elements[pos] ;
   prev = 0 ;
   while (elt)
      {
      if (entry->keycmp(elt) == 0)
         {
         if (prev)
	    prev->next = elt->next ;
         else
            elements[pos] = elt->next ;
	 elt->in_use = false ;
         curr_elements-- ;
	 delete elt ;
	 return 0 ;
         }
      else
         {
         prev = elt ;
         elt = elt->next ;
	 }
      }
   return -1 ;
}

//----------------------------------------------------------------------

FrHashEntry *FrHashTable::lookup(const FrHashEntry *entry) const
{
   if (size)
      {
      register FrHashEntry *elt ;
      size_t pos = entry->hashIndex(size) ;

      elt = elements[pos] ;
      while (elt)
         {
         if (entry->keycmp(elt) == 0)
	    return elt ;
         else
            elt = elt->next ;
         }
      }
   return 0 ;	// not found
}

//----------------------------------------------------------------------

FrList *FrHashTable::prefixMatches(const char *prefix) const
{
   FrList *matches = 0 ;
   int prefixlen = prefix ? strlen(prefix) : 0 ;

   // iterate over the hash entries
   for (FrHashEntry **elt = &elements[size-1] ; elt >= elements ; elt--)
      {
      FrHashEntry *entry = *elt ;
      while (entry)
         {
	 if (prefixlen == 0 || entry->prefixMatch(prefix,prefixlen))
	    pushlist((FrObject*)entry->entryName(),matches) ;
	 entry = entry->next ;
	 }
      }
   return matches ;
}

//----------------------------------------------------------------------

char *FrHashTable::completionFor(const char *prefix) const
{
   char *match = 0 ;
   int prefixlen = prefix ? strlen(prefix) : 0 ;

   // iterate over the hash entries
   for (FrHashEntry **elt = &elements[size-1] ; elt >= elements ; elt--)
      {
      FrHashEntry *entry = *elt ;
      while (entry)
         {
         if (entry->entryName() &&
             (prefixlen == 0 || entry->prefixMatch(prefix,prefixlen)))
            {
	    const char *name = entry->entryName()->symbolName() ;
	    if (match)
               {
	       int i ;
	       for (i = 0 ; match[i] && match[i] == name[i] ; i++)
	          ;
	       match[i] = '\0' ;
               }
            else
               {
	       match = FrDupString(name) ;
	       }
            }
	 entry = entry->next ;
         }
      }
   return match ;
}

//----------------------------------------------------------------------

bool __FrCDECL FrHashTable::doHashEntries(DoHashEntriesFunc *func,...)
{
   for (FrHashEntry **elt = &elements[size-1] ; elt >= elements ; elt--)
      {
      FrHashEntry *entry = *elt ;
      while (entry)
         {
	 FrHashEntry *next = entry->next ;
	 // note: under Watcom C++, use of the va_list inside the called
	 // function affects its value here!  So, we have to reset it each
	 // time through the loop
	 va_list args ;
	 va_start(args,func) ;
	 if (!func(entry,args))
	    {
	    va_end(args) ;
	    return false ;
	    }
	 va_end(args) ;
	 entry = next ;
	 }
      }
   return true ;
}

/************************************************************************/
/*    I/O functions for class FrHashTable				*/
/************************************************************************/

static bool print_hash_entry(const FrObject *obj, va_list args)
{
   FrVarArg(ostream *,output) ;
   FrVarArg(size_t *,loc) ;
   FrVarArg(bool *,first) ;
   size_t len = obj ? obj->displayLength()+1 : 3 ;
   *loc += len ;
   if (*loc > FramepaC_display_width && !*first)
      {
      (*output) << '\n' << setw(FramepaC_initial_indent) << " " ;
      *loc = FramepaC_initial_indent + len ;
      }
   (*output) << obj << ' ' ;
   *first = false ;
   return true ;
}

//----------------------------------------------------------------------

ostream &FrHashTable::printValue(ostream &output) const
{
   if ((FrObject*)this == &NIL)
      return output << str_NIL ;
   else
      {
      output << str_hash_intro ;
      size_t orig_indent = FramepaC_initial_indent ;
      FramepaC_initial_indent += sizeof(str_hash_intro)-1 ;
      size_t loc = FramepaC_initial_indent ;
      bool first = true ;
      iterate(print_hash_entry,&output,&loc,&first) ;
      FramepaC_initial_indent = orig_indent ;
      return output << ')' ;
      }
}

//----------------------------------------------------------------------

static bool display_hash_entry(const FrObject *ent, va_list args)
{
   FrHashEntry *entry = (FrHashEntry*)ent ;
   FrVarArg(char **,buffer) ;
   if (entry)
      {
      *buffer = entry->displayValue(*buffer) ;
      strcat(*buffer," ") ;
      (*buffer)++ ;
      }
   return true ;
}

//----------------------------------------------------------------------

char *FrHashTable::displayValue(char *buffer) const
{
   if ((FrObject*)this == &NIL)
      {
      memcpy(buffer,str_NIL,3) ;
      return buffer+3 ;
      }
   else
      {
      strcpy(buffer,str_hash_intro) ;
      iterate(display_hash_entry,&buffer) ;
      *buffer++ = ')' ;
      *buffer = '\0' ;
      return buffer ;
      }
}

//----------------------------------------------------------------------

static bool dlength_hash_entry(const FrObject *ent, va_list args)
{
   FrHashEntry *entry = (FrHashEntry*)ent ;
   FrVarArg(int *,length) ;
   if (entry)
      *length += entry->displayLength() ;
   return true ;
}

//----------------------------------------------------------------------

size_t FrHashTable::displayLength() const
{
   if ((FrObject*)this == &NIL)
      return 3 ;
   else
      {
      int len = 4 ;
      iterate(dlength_hash_entry,&len) ;
      return len ;
      }
}

//----------------------------------------------------------------------

static FrObject *string_to_HashTable(const char *&input, const char *digits)
{
   FrHashTable *ht ;
   int table_size ;
   if (digits)
      {
      table_size = atol(digits) ;
      if (table_size < 10)
	 table_size = 10 ;
      }
   else
      table_size = 10 ;
   int c = *input++ ;
   if (c == '(')
      {
      ht = new FrHashTable ;
      ht->expand(table_size) ;
      c = FrSkipWhitespace(input) ;
      while (c && c != ')')
	 {
	 FrObject *obj = string_to_FrObject(input) ;
	 if (obj && obj->hashp() && obj->objType() != OT_FrHashTable)
	    {
	    ht->add((FrHashEntry*)obj) ;
	    obj->freeObject() ;
	    }
	 else if (obj)
	    {
	    FrHashEntryObject entry(obj) ;
	    ht->add(&entry) ;
	    }
	 c = FrSkipWhitespace(input) ;
	 }
      }
   else
      ht = 0 ;
   return ht ;
}

//----------------------------------------------------------------------

static bool verify_HashTable(const char *&input, const char *, bool)
{
   int c = FrSkipWhitespace(input) ;
   if (c == '(')
      {
      input++ ;				// consume the left parenthesis
      c = FrSkipWhitespace(input) ;
      while (c && c != ')')
	 {
	 if (!valid_FrObject_string(input,true))
	    return false ;
	 c = FrSkipWhitespace(input) ;
	 }
      if (c == ')')
	 {
	 input++ ;			// skip terminating right paren
	 return true ;			//   and indicate success
	 }
      }
   return false ;
}

//----------------------------------------------------------------------

static FrObject *read_HashTable(istream &input, const char *digits)
{
   FrHashTable *ht ;
   int table_size ;
   if (digits)
      {
      table_size = atol(digits) ;
      if (table_size < 10)
	 table_size = 10 ;
      }
   else
      table_size = 10 ;
   int c = input.get() ;
   if (c == '(')
      {
      ht = new FrHashTable ;
      ht->expand(table_size) ;
      c = FrSkipWhitespace(input) ;
      while (c && c != ')')
	 {
	 FrObject *obj ;
	 input >> obj ;
	 if (obj && obj->hashp() && obj->objType() != OT_FrHashTable)
	    {
	    ht->add((FrHashEntry*)obj) ;
	    obj->freeObject() ;
	    }
	 else if (obj)
	    {
	    FrHashEntryObject entry(obj) ;
	    ht->add(&entry) ;
	    }
	 c = FrSkipWhitespace(input) ;
	 }
      }
   else
      ht = 0 ;
   return ht ;
}

// end of file frhash.cpp //
