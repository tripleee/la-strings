/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC -- frame manipulation in C++				*/
/*  Version 2.00beta 							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frsymhsh.cpp		class FrSymHashTable			*/
/*  LastEdit: 28oct2013							*/
/*									*/
/*  (c) Copyright 1994,1995,1996,1997,1998,1999,2000,2001,2004,2005,	*/
/*		2006,2008,2009,2013 Ralf Brown				*/
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
#  pragma implementation "frsymhsh.h"
#endif

#include "framerr.h"
#include "frqsort.h"
#include "frsymhsh.h"

/************************************************************************/
/*    Global data local to this module					*/
/************************************************************************/

#define MAX_HT_SIZE (UINT_MAX / sizeof (FrSymHashEntry *))

/************************************************************************/
/*    Global variables for class FrSymHashEntry				*/
/************************************************************************/

FrAllocator FrSymHashEntry::allocator("FrSymHashEntry", sizeof(FrSymHashEntry));
FrAllocator FrSymHashChain::allocator("FrSymHashChain", sizeof(FrSymHashChain));
FrAllocator FrSymHashTable::allocator("FrSymHashTable", sizeof(FrSymHashTable));

/************************************************************************/
/*    Methods for class FrSymHashEntry					*/
/************************************************************************/

void FrSymHashEntry::reserve(size_t N)
{
   size_t objs_per_block = allocator.objectsPerPage() ;
   // note: the count is approximate, but that's OK because we can always
   //   allocate more later
   for (size_t i = allocator.objects_allocated() ;
	i < N ;
	i += objs_per_block)
      {
      allocator.preAllocate() ;
      }
   return ;
}

/************************************************************************/
/*    Methods for class FrSymHashChain					*/
/************************************************************************/

void FrSymHashChain::reserve(size_t N)
{
   size_t objs_per_block = allocator.objectsPerPage() ;
   // note: the count is approximate, but that's OK because we can always
   //   allocate more later
   for (size_t i = allocator.objects_allocated() ;
	i < N ;
	i += objs_per_block)
      {
      allocator.preAllocate() ;
      }
   return ;
}

/************************************************************************/
/*    Methods for class FrSymHashTable					*/
/************************************************************************/

FrSymHashTable::FrSymHashTable(size_t initial_size)
{
   cleanup_fn = 0 ;
   loading_packed = false ;
   loading_complete = false ;
   if (initial_size < 53)
      initial_size = 53 ;
   else if (initial_size > MAX_HT_SIZE)
      initial_size = MAX_HT_SIZE ;
   if ((initial_size & 1) == 0)		// try to keep size odd
      initial_size++ ;
   if ((initial_size % 3) == 0)		//   and also not a multiple of 3
      initial_size += 2 ;
   curr_elements = 0 ;
   array = 0 ;
   elements = FrNewC(FrSymHashChain*,initial_size) ;
   size = initial_size ;
   if (!elements)
      {
      FrNoMemory("creating FrSymHashTable") ;
      size = 0 ;
      }
   return ;
}

//----------------------------------------------------------------------

FrSymHashTable::FrSymHashTable(const FrSymHashTable &ht)
{
   if (&ht == 0)
      return ;
   size = ht.size ;
   curr_elements = ht.curr_elements ;
   cleanup_fn = ht.cleanup_fn ;
   loading_packed = ht.loading_packed ;
   loading_complete = ht.loading_complete ;
   if (ht.array)
      {
      elements = 0 ;
      array = FrNewN(FrSymHashEntry,size) ;
      if (array)
	 memcpy(array,ht.array,size*sizeof(FrSymHashEntry)) ;
      else
	 {
	 size = 0 ;
	 curr_elements = 0 ;
	 }
      }
   else
      {
      array = 0 ;
      elements = FrNewN(FrSymHashChain*,size) ;
      if (elements)
	 {
	 for (size_t i = 0 ; i < size ; i++)
	    {
	    if (ht.elements[i])
	       {
	       FrSymHashChain *rentry = 0 ;
	       FrSymHashChain *entry = ht.elements[i] ;
	       for ( ; entry ; entry = entry->next)
		  {
		  FrSymHashChain *newentry = new FrSymHashChain(*entry) ;
		  if (rentry)
		     rentry->next = newentry ;
		  else
		     elements[i] = newentry ;
		  rentry = newentry ;
		  }
	       rentry->next = 0 ;
	       }
	    else
	       elements[i] = 0 ;
	    }
	 }
      else
	 {
	 size = 0 ;
	 curr_elements = 0 ;
	 }
      }
}

//----------------------------------------------------------------------

FrSymHashTable::~FrSymHashTable()
{
   if (cleanup_fn)
      this->doHashEntries(cleanup_fn) ;
   if (elements)
      {
      for (FrSymHashChain **elt = &elements[size-1] ; elt >= elements ; elt--)
	 {
	 FrSymHashChain *entry = *elt ;
	 while (entry)
	    {
	    FrSymHashChain *tmp = entry ;
	    entry = entry->next ;
	    delete tmp ;
	    }
	 }
      FrFree(elements) ;
      elements = 0 ;
      }
   if (array)
      {
      FrFree(array) ;
      array = 0 ;
      loading_packed = false ;
      }	
   loading_complete = false ;
   curr_elements = size = 0 ;
   return ;
}

//----------------------------------------------------------------------

FrSymHashTable *FrSymHashTable::deepcopy() const
{
   return new FrSymHashTable(*this) ;
}

//----------------------------------------------------------------------

void FrSymHashTable::expand(unsigned int increment)
{
   unsigned int oldsize = size ;
   unsigned int i ;

   if (size + FrHASHTABLE_MIN_INCREMENT > MAX_HT_SIZE)
      return ; // don't expand if we'd be forced to exceed the maximum size
   if (increment < FrHASHTABLE_MIN_INCREMENT)
      increment = FrHASHTABLE_MIN_INCREMENT ;
   size += increment ;
   if (size >= MAX_HT_SIZE)
      size = MAX_HT_SIZE ;
   else if ((size & 1) == 0)		// try to keep size odd
      size++ ;
   if ((size % 3) == 0)			//   and also not a multiple of 3
      size += 2 ;
   if (size == oldsize)
      return ;	     // nothing to do
   if (array)
      {
      FrSymHashEntry *newarray = FrNewR(FrSymHashEntry,array,size) ;
      if (newarray)
	 {
	 array = newarray ;
	 // clear the remainder of the expanded table
	 for (i = oldsize ; i < size ; i++)
	    new (&array[i]) FrSymHashEntry ;
	 }
      else
	 {
	 size = oldsize ;
	 FrWarning("unable to expand hash table--will attempt to continue") ;
	 }
      }
   else
      {
      FrSymHashChain *oldentries = 0 ;
      FrSymHashChain **newelts = FrNewR(FrSymHashChain*,elements,size) ;
      if (newelts)
	 {
	 elements = newelts ;
	 for (i = 0 ; i < oldsize ; i++)
	    {
	    FrSymHashChain *element = elements[i] ;
	    if (element)
	       {
	       FrSymHashChain *tail = element ;
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
	    FrSymHashChain *old_elt = oldentries ;
	    const FrSymbol *key = old_elt->getName() ;
	    size_t pos = (size_t)(((uintptr_t)key) % size) ;
	    oldentries = oldentries->next ;
	    old_elt->next = elements[pos] ;
	    elements[pos] = old_elt ;
	    }
	 }
      else
	 {
	 size = oldsize ;
	 FrWarning("unable to expand hash table--will continue") ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

void FrSymHashTable::expandTo(unsigned int newsize)
{
   if (newsize > size)
      expand(newsize-size) ;
   return ;
}

//----------------------------------------------------------------------

FrSymHashEntry *FrSymHashTable::add(const FrSymHashEntry *entry)
{
   const FrSymbol *key = entry->getName() ;
   if (array)
      {
      size_t elt ;
      if (loading_complete)
	 {
	 // find the proper insertion point, and shift all the rest down by one
	 size_t lo = 0 ;
	 size_t hi = curr_elements ;
	 // perform a binary search
	 while (hi > lo)
	    {
	    size_t mid = (lo + hi) / 2 ;
	    const FrSymbol *midname = array[mid].getName() ;
	    if (key < midname)
	       hi = mid ;
	    else if (key > midname)
	       lo = mid + 1 ;
	    else // key == midname
	       {
	       // found it!
	       return &array[mid] ;
	       }
	    }
	 // if we get to this point, the item is not yet in the table
	 elt = lo ;
	 // shift the rest of the items by one position
	 for (size_t i = curr_elements ; i > elt ; i--)
	    array[i] = array[i-1] ;
	 }
      else
	 elt = curr_elements ;
      curr_elements++ ;			// remember that we've added an item
      if (curr_elements >= size)	// make room if necessary
	 expand(size/2) ;
      new (&array[elt]) FrSymHashEntry(*entry) ;
      return &array[elt] ;
      }
   else
      {
      register size_t pos = (size_t)(((uintptr_t)key) % size) ;
      FrSymHashChain *new_elt = elements[pos] ;
      while (new_elt)
	 if (new_elt->getName() == key)
	    return new_elt ;
	 else
	    new_elt = new_elt->next ;
      new_elt = new FrSymHashChain(*entry) ;
      if (new_elt)
	 {
	 new_elt->next = elements[pos] ;
	 elements[pos] = new_elt ;
	 if (++curr_elements >= size)
	    expand(3*size/4) ;   // expand hash table by 75%
	 }
      return new_elt ;
      }
}

//----------------------------------------------------------------------

FrSymHashEntry *FrSymHashTable::add(const FrSymbol *key, void *udata)
{
   if (array)
      {
      size_t elt ;
      if (loading_complete)
	 {
	 // find the proper insertion point, and shift all the rest down by one
	 size_t lo = 0 ;
	 size_t hi = curr_elements ;
	 // perform a binary search
	 while (hi > lo)
	    {
	    size_t mid = (lo+hi) / 2 ;
	    const FrSymbol *midname = array[mid].getName() ;
	    if (key < midname)
	       hi = mid ;
	    else if (key > midname)
	       lo = mid + 1 ;
	    else // key == midname
	       {
	       // found it!
	       return &array[mid] ;
	       }
	    }
	 // if we get to this point, the item is not yet in the table
	 elt = lo ;
	 // shift the rest of the items by one position
	 for (size_t i = curr_elements ; i > elt ; i--)
	    array[i] = array[i-1] ;
	 }
      else
	 elt = curr_elements ;
      curr_elements++ ;			// remember that we've added an item
      if (curr_elements >= size)	// make room if necessary
	 expand(size/2) ;
      new (&array[elt]) FrSymHashEntry(key,udata) ;
      return &array[elt] ;
      }
   else
      {
      register size_t pos = (size_t)(((uintptr_t)key) % size) ;
      FrSymHashChain *new_elt = elements[pos] ;
      while (new_elt)
	 if (new_elt->getName() == key)
	    return new_elt ;
	 else
	    new_elt = new_elt->next ;
      new_elt = new FrSymHashChain(key,udata) ;
      if (new_elt)
	 {
	 new_elt->next = elements[pos] ;
	 elements[pos] = new_elt ;
	 if (++curr_elements >= size)
	    expand(3*size/4) ;   // expand hash table by 75%
	 }
      return new_elt ;
      }
}

//----------------------------------------------------------------------

int FrSymHashTable::remove(const FrSymHashEntry *entry)
{
   const FrSymbol *key = entry->getName() ;
   if (array)
      {
      if (loading_packed && !loading_complete)
	 return -1 ;			// can't remove until after packing!
      // find the proper item to be removed, and shift all the rest up by one
      size_t lo = 0 ;
      size_t hi = curr_elements ;
      // perform a binary search
      while (hi > lo)
	 {
	 size_t mid = (lo+hi) / 2 ;
	 const FrSymbol *midname = array[mid].getName() ;
	 if (key < midname)
	    hi = mid ;
	 else if (key > midname)
	    lo = mid + 1 ;
	 else // key == midname
	    {
	    // found the item, so remove it
	    curr_elements-- ;
	    // shift the rest of the items by one position
	    for (size_t i = mid ; i < curr_elements ; i++)
	       array[i] = array[i+1] ;
	    return 0 ;			// successfully removed
	    }
	 }
      }
   else
      {
      size_t pos = (size_t)(((uintptr_t)key) % size) ;
      register FrSymHashChain *elt, *prev ;

      elt = elements[pos] ;
      prev = 0 ;
      while (elt)
	 {
	 if (elt->getName() == key)
	    {
	    if (prev)
	       prev->next = elt->next ;
	    else
	       elements[pos] = elt->next ;
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
      }
   // if we get to this point, we couldn't find the element
   return -1 ;
}

//----------------------------------------------------------------------

void FrSymHashTable::loadPacked()
{
   if (!loading_packed && !array)
      {
      // copy any items which are already in the hash buckets into an array
      if (size == 0)
	 size = 53 ;
      FrFree(array) ;
      array = FrNewN(FrSymHashEntry,size) ;
      size_t numelts = 0 ;
      if (array)
	 {
	 for (FrSymHashChain **elt=&elements[size-1] ; elt >= elements ; elt--)
	    {
	    FrSymHashChain *entry = *elt ;
	    while (entry)
	       {
	       FrSymHashChain *next = entry->next ;
	       new (&array[numelts++]) FrSymHashEntry(*entry) ;
	       delete entry ;
	       entry = next ;
	       }
	    }
	 if (numelts != curr_elements)
	    FrProgError("hash table corrupted (found while changing to packed)") ;
	 FrFree(elements) ;
	 elements = 0 ;
	 size = curr_elements+1 ;
	 loading_packed = true ;
	 }
      }
}

//----------------------------------------------------------------------

void FrSymHashTable::pack()
{
   if (elements && !loading_packed)
      {
      // copy the items in the hash buckets into an array
      FrFree(array) ;
      array = FrNewN(FrSymHashEntry,curr_elements+1) ;
      size_t numelts = 0 ;
      if (array)
	 {
	 for (size_t i = 0 ; i < size ; i++)
	    {
	    FrSymHashChain *entry = elements[i] ;
	    while (entry)
	       {
	       FrSymHashChain *next = entry->next ;
	       new (&array[numelts++]) FrSymHashEntry(*entry) ;
	       delete entry ;
	       entry = next ;
	       }
	    }
	 if (numelts != curr_elements)
	    FrProgError("hash table corrupted (discovered while packing)") ;
	 FrFree(elements) ;
	 elements = 0 ;
	 size = curr_elements+1 ;
	 loading_packed = true ;
	 }
      else
	 return ;
      }
   FrQuickSort(array,curr_elements) ;
   loading_complete = true ;
   return ;
}

//----------------------------------------------------------------------

FrSymHashEntry *FrSymHashTable::lookup(const FrSymbol *key) const
{
   if (array)
      {
      if (curr_elements == 0)		// if nothing in hash table, then the
	 return 0 ;			//   key can't possibly be found
      if (loading_packed && !loading_complete)
	 {
	 // unsorted, so we have to do a linear search
	 for (size_t i = 0 ; i < curr_elements ; i++)
	    if (array[i].getName() == key)
	       return &array[i] ;
	 return 0 ;			// not found!
	 }
      size_t lo = 0 ;
      size_t hi = curr_elements ;
      // perform a binary search
      while (hi > lo)
	 {
	 size_t mid = (lo+hi) / 2 ;
	 const FrSymbol *midname = array[mid].getName() ;
	 if (key < midname)
	    hi = mid ;
	 else if (key > midname)
	    lo = mid + 1 ;
	 else // key == midname
	    return &array[mid] ;
	 }
      }
   else
      {
      register FrSymHashChain *elt ;
      elt = elements[(size_t)(((uintptr_t)key) % size)] ;
      while (elt)
	 {
	 if (elt->getName() == key)
	    return elt ;
	 else
	    elt = elt->next ;
	 }
      }
   return 0 ;	// not found
}

//----------------------------------------------------------------------

bool __FrCDECL FrSymHashTable::doHashEntries(FrSymHashEntriesFunc *func,...)
{
   va_list args ;
   va_start(args,func) ;
   bool success = true ;
   if (array)
      {
      for (size_t i = 0 ; i < curr_elements && success ; i++)
	 {
	 FrSafeVAList(args) ;
	 success = func(&array[i],FrSafeVarArgs(args)) ;
	 FrSafeVAListEnd(args) ;
	 }
      }
   else if (size > 0)
      {
      for (FrSymHashChain **elt = &elements[size-1] ; elt >= elements ; elt--)
	 {
	 FrSymHashChain *entry = *elt ;
	 while (entry)
	    {
	    FrSymHashChain *next = entry->next ;
	    FrSafeVAList(args) ;
	    success = func(entry,FrSafeVarArgs(args)) ;
	    FrSafeVAListEnd(args) ;
	    if (!success)
	       {
	       va_end(args) ;
	       return false ;
	       }
	    entry = next ;
	    }
	 }
      }
   va_end(args) ;
   return success ;
}

//----------------------------------------------------------------------

FrList *FrSymHashTable::allKeys() const
{
   FrList *keys = 0 ;
   if (array)
      {
      for (size_t i = 0 ; i < curr_elements ; i++)
	 pushlist(array[i].getName(),keys) ;
      }
   else
      {
      for (FrSymHashChain **elt = &elements[size-1] ; elt >= elements ; elt--)
	 {
	 FrSymHashChain *entry = *elt ;
	 while (entry)
	    {
	    pushlist(entry->getName(),keys) ;
	    entry = entry->next ;
	    }
	 }
      }
   return keys ;
}

/************************************************************************/
/************************************************************************/

// end of file frsymhsh.cpp //

