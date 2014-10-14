/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 2.00							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frsarray.cpp	    class FrSparseArray				*/
/*  LastEdit: 23mar2014							*/
/*									*/
/*  (c) Copyright 1997,1998,2001,2002,2004,2006,2008,2009,2012,2013,	*/
/*		2014 Ralf Brown						*/
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

#include <stdlib.h>
#include "framerr.h"
#include "frarray.h"
#include "frassert.h"
#include "frcmove.h"
#include "frnumber.h"
#include "frreader.h"
#include "frpcglbl.h"

#ifdef FrSTRICT_CPLUSPLUS
#  include <iomanip>
#else
#  include <iomanip.h>
#endif

/**********************************************************************/
/*    Manifest constants					      */
/**********************************************************************/

#define MIN_MAX_SIZE (sizeof(FrArrayFrag)/sizeof(FrArrayFrag*))

#define array_frags(n) (((n)+FrARRAY_FRAG_SIZE-1)/FrARRAY_FRAG_SIZE)

/**********************************************************************/
/*    Global variables local to this module			      */
/**********************************************************************/

FrAllocator FrSparseArray::allocator("FrSparseArray",sizeof(FrSparseArray)) ;

static FrObject *read_FrSparseArray(istream &input, const char *) ;
static FrObject *string_to_FrSparseArray(const char *&input, const char *) ;
static bool verify_FrSparseArray(const char *&input, const char *,bool) ;

static FrReader FrSparseArray_reader(string_to_FrSparseArray,
				     read_FrSparseArray, verify_FrSparseArray,
				     FrREADER_LEADIN_LISPFORM,"SpArr") ;

static const char errmsg_bounds[]
      = "array access out of bounds" ;

/**********************************************************************/
/*    Global variables which are visible from other modules	      */
/**********************************************************************/

FrAllocator FrArrayFrag::allocator("FrArrayFrag",sizeof(FrArrayFrag)) ;

/**********************************************************************/
/*    Helper functions						      */
/**********************************************************************/

#define update_arrayhash FramepaC_update_ulong_hash

/************************************************************************/
/*	methods for class FrArrayFrag					*/
/************************************************************************/

void FrArrayFrag::reserve(size_t N)
{
   size_t objs_per_block = allocator.objectsPerPage() ;
   for (size_t i = allocator.objects_allocated() ;
	i < N ;
	i += objs_per_block)
      {
      allocator.preAllocate() ;
      }
   return ;
}

/************************************************************************/
/*	methods for class FrSparseArray				*/
/************************************************************************/

FrSparseArray::FrSparseArray()
{
   max_items = MIN_MAX_SIZE*FrARRAY_FRAG_SIZE ;
   items = (FrArrayFrag**)new FrArrayFrag ;
   return ;
}

//----------------------------------------------------------------------

FrSparseArray::FrSparseArray(size_t max)
{
   max_items = array_frags(max) ;
   if (max_items <= MIN_MAX_SIZE)
      {
      max_items = MIN_MAX_SIZE ;
      items = (FrArrayFrag**)new FrArrayFrag ;
      }
   else
      items = FrNewC(FrArrayFrag*,max_items) ;
   max_items *= FrARRAY_FRAG_SIZE ;
   return ;
}

//----------------------------------------------------------------------

FrSparseArray::FrSparseArray(const FrSparseArray *init, size_t start,
			       size_t stop)
{
   m_length = stop-start+1 ;
   max_items = array_frags(arrayLength()) ;
   if (max_items <= MIN_MAX_SIZE)
      {
      max_items = MIN_MAX_SIZE ;
      items = (FrArrayFrag**)new FrArrayFrag ;
      }
   else
      items = FrNewC(FrArrayFrag*,max_items) ;
   max_items *= FrARRAY_FRAG_SIZE ;
   for (size_t i = start ; i <= stop ; i++)
      {
      FrArrayItem &itm = init->item(i) ;
      uintptr_t index = itm.getIndex() ;
      FrObject *value = itm.getValue() ;
      new (&item(i-start)) FrArrayItem(index, value ? value->deepcopy() : 0) ;
      }
   return ;
}

//----------------------------------------------------------------------

FrSparseArray::FrSparseArray(const FrList *initializer)
{
   m_length = initializer->listlength() ;
   max_items = array_frags(arrayLength()) ;
   allocFragmentList() ;
   max_items *= FrARRAY_FRAG_SIZE ;
   return ;
}

//----------------------------------------------------------------------

FrSparseArray::~FrSparseArray()
{
   if (items)
      {
      size_t n = array_frags(arrayLength()) ;
      for (size_t i = 0 ; i < n ; i++)
	 delete items[i] ;
      if (max_items <= MIN_MAX_SIZE*FrARRAY_FRAG_SIZE)
	 {
	 FrArrayFrag *frag = (FrArrayFrag*)items ;
	 delete frag ;
	 }
      else
	 FrFree(items) ;
      m_length = 0 ;
      max_items = 0 ;
      items = 0 ;
      }
   return ;
}

//----------------------------------------------------------------------

const char *FrSparseArray::objTypeName() const
{
   return "FrSparseArray" ;
}

//----------------------------------------------------------------------

FrObjectType FrSparseArray::objType() const
{
   return OT_FrSparseArray ;
}

//----------------------------------------------------------------------

FrObjectType FrSparseArray::objSuperclass() const
{
   return OT_FrArray ;
}

//----------------------------------------------------------------------

unsigned long FrSparseArray::hashValue() const
{
   // the hash value of an array is a combination of the hash values of all
   // its elements
   unsigned long hash = 0 ;
   if (arrayLength() > 0)
      {
      for (size_t pos = 0 ; pos < arrayLength() ; pos++)
	 {
	 FrArrayItem &curritem = item(pos) ;
	 uintptr_t idx = curritem.getIndex() ;
	 FrObject *val = curritem.getValue() ;
	 hash = update_arrayhash(hash,idx) ;
	 hash = update_arrayhash(hash,(uintptr_t)val) ;
	 }
      }
   return hash ;
}

//----------------------------------------------------------------------

void FrSparseArray::freeObject()
{
   delete this ;
   return ;
}

//----------------------------------------------------------------------

FrObject *FrSparseArray::copy() const
{
   return new FrSparseArray(this,0,arrayLength()-1) ;
}

//----------------------------------------------------------------------

FrObject *FrSparseArray::deepcopy() const
{
   return new FrSparseArray(this,0,arrayLength()-1) ;
}

//----------------------------------------------------------------------

bool FrSparseArray::expand(size_t increment)
{
   return expandTo(max_items + increment) ;
}

//----------------------------------------------------------------------

void FrSparseArray::allocFragmentList()
{
   if (max_items <= MIN_MAX_SIZE)
      {
      max_items = MIN_MAX_SIZE ;
      items = (FrArrayFrag**)new FrArrayFrag ;
      memset(items,'\0',sizeof(*items)) ;
      }
   else
      items = FrNewC(FrArrayFrag*,max_items) ;
   return ;
}

//----------------------------------------------------------------------

static FrArrayFrag **realloc_frag_list(FrArrayFrag **fraglist,
				       size_t curr_size, size_t new_size)
{
   if (new_size <= curr_size)
      return fraglist ;
   if (curr_size == MIN_MAX_SIZE)
      {
      FrArrayFrag **newfrags = FrNewN(FrArrayFrag*,new_size) ;
      if (!newfrags)
	 return 0 ;
      memcpy(newfrags,fraglist,curr_size*sizeof(FrArrayFrag*)) ;
      FrArrayFrag *fr = (FrArrayFrag*)fraglist ;
      delete fr ;
      return newfrags ;
      }
   else
      {
      return FrNewR(FrArrayFrag*,fraglist,new_size) ;
      }
}

//----------------------------------------------------------------------

bool FrSparseArray::expandTo(size_t newsize)
{
   if (newsize <= max_items)
      return true ;			// trivially successful
   size_t curr_frags = array_frags(max_items) ;
   size_t new_frags = array_frags(newsize) ;
   if (new_frags <= curr_frags)
      return true ;			// trivially successful
   FrArrayFrag **newfraglist = realloc_frag_list(items,curr_frags,new_frags) ;
   if (newfraglist)
      {
      max_items = new_frags * FrARRAY_FRAG_SIZE ;
      items = newfraglist ;
      return true ;	
      }
   else
      return false ;
}

//----------------------------------------------------------------------

FrObject *FrSparseArray::car() const
{
   return (arrayLength() > 0) ? (FrObject*)item(0).getValue() : 0 ;
}

//----------------------------------------------------------------------

FrObject *FrSparseArray::reverse()
{
   return this ;			// reverse is meaningless for sparse
					// arrays....
}

//----------------------------------------------------------------------

FrObject *FrSparseArray::lookup(uintptr_t index) const
{
   // perform a binary search on the array's items, looking for the index
   size_t hi = arrayLength() ;
   size_t lo = 0 ;
   while (hi > lo)
      {
      size_t mid = (hi + lo) / 2 ;
      FrArrayItem &miditem = item(mid) ;
      uintptr_t mididx = miditem.getIndex() ;
      if (mididx < index)
	 lo = mid + 1 ;
      else if (mididx > index)
	 hi = mid ;
      else // if (mididx == index) // we found the index we wanted
	 return miditem.getValue() ;
      }
   // if we get here, we didn't find the desired index
   return 0 ;
}

//----------------------------------------------------------------------

bool FrSparseArray::createAndFillGap(size_t pos, size_t N,
				      const FrObject *value)
{
   if (arrayLength() >= max_items && !expandTo(2*max_items))
      return false ;
   // if last fragment is already full, allocate another one
   if ((arrayLength() % FrARRAY_FRAG_SIZE) == 0)
      items[arrayLength()/FrARRAY_FRAG_SIZE] = new FrArrayFrag ;
   // create a gap into which to insert the new definition
   FrArrayItem *item0, *item1 ;
   item0 = &item(arrayLength()) ;
   for (size_t i = arrayLength() ; i > pos ; --i)
      {
      item1 = &item(i-1) ;
      *item0 = *item1 ;
      item0 = item1 ;
      }
   // finally, add the new item into the gap we just created
   m_length++ ;
   new (item0) FrArrayItem(N,(FrObject*)value) ;
   return true ;
}

//----------------------------------------------------------------------

bool FrSparseArray::addItem(uintptr_t index, const FrObject *value,
			     int if_exists)
{
   // perform a binary search on the given items, looking for the word
   size_t hi = arrayLength() ;
   size_t lo = 0 ;
   while (hi > lo)
      {
      size_t mid = (hi+lo) / 2 ;
      FrArrayItem &miditem = item(mid) ;
      uintptr_t mididx = miditem.getIndex() ;
      if (mididx < index)
	 lo = mid + 1 ;
      else if (mididx > index)
	 hi = mid ;
      else // if (mididx == index) // we found the item we wanted
	 {
	 if (if_exists == FrArrAdd_INCREMENT)
	    miditem.addOccurrence((uintptr_t)value) ;
	 else if (if_exists == FrArrAdd_REPLACE)
	    {
	    free_object(miditem.getValue()) ;
	    miditem.setValue(value ? value->deepcopy() : 0) ;
	    }
	 else // if (if_exists == FrArrAdd_RETAIN)
	    {
	    // leave as-is
	    }
	 return true ;
	 }
      }
   // if we get here, the desired index was not yet in our list, and it
   // should go into location 'lo' after shifting 'lo' and following up
   // by one position
   return createAndFillGap(lo,index,value) ;
}

//----------------------------------------------------------------------

FrObject *FrSparseArray::getNth(size_t N) const
{
   size_t pos = findIndexOrHigher(N) ;
   if (pos < arrayLength())
      {
      FrArrayItem &itm = item(pos) ;
      if (itm.getIndex() == N)
	 return itm.getValue() ;
      }
   else if (range_errors)
      FrWarning(errmsg_bounds) ;
   return 0 ;
}

//----------------------------------------------------------------------

bool FrSparseArray::setNth(size_t N, const FrObject *elt)
{
   return addItem(N,elt) ;
}

//----------------------------------------------------------------------

size_t FrSparseArray::findIndex(size_t N)
{
   size_t pos = findIndexOrHigher(N) ;
   if (pos < arrayLength())
      {
      FrArrayItem &itm = item(pos) ;
      if (itm.getIndex() == N)
	 return pos ;
      }
   // if we get here, the desired index was not yet in our list, and it
   // should go into location 'pos' after shifting 'pos' and following up
   // by one position
   return createAndFillGap(pos,N,0) ? pos : (size_t)~0 ;
}

//----------------------------------------------------------------------

size_t FrSparseArray::findIndexOrHigher(size_t N) const
{
   // perform a binary search on the array's items, looking for the index
   size_t hi = arrayLength() ;
   size_t lo = 0 ;
   while (hi > lo)
      {
      size_t mid = (hi+lo) / 2 ;
      FrArrayItem &miditem = item(mid) ;
      uintptr_t mididx = miditem.getIndex() ;
      if (mididx < N)
	 lo = mid + 1 ;
      else if (mididx > N)
	 hi = mid ;
      else // if (mididx == N) // we found the index we wanted
	 return mid ;
      }
   // if we get here, the indicated item did not exist, so return next higher
   return lo ;
}

//----------------------------------------------------------------------

FrObject *&FrSparseArray::operator [] (size_t N)
{
   size_t idx = findIndex(N) ;
   return *item(idx).getValueRef() ;
}

//----------------------------------------------------------------------

size_t FrSparseArray::locate(const FrObject *finditem, size_t start) const
{
   if (!items || arrayLength() == 0)
      return (size_t)-1 ;
   if (start == (size_t)-1)
      start = 0 ;
   else
      start++ ;
   start = findIndexOrHigher(start) ;
   if (finditem && finditem->consp())
      {
      for (size_t pos = start ; pos < arrayLength() ; pos++)
	 {
	 FrArrayItem &itm = item(pos) ;
	 if (itm.getValue() == ((FrList*)finditem)->first())
	    return (size_t)itm.getIndex() ;
	 }
      }
   else
      {
      for (size_t pos = start ; pos < arrayLength() ; pos++)
	 {
	 FrArrayItem &itm = item(pos) ;
	 if (itm.getValue() == finditem)
	    return itm.getIndex() ;
	 }
      }
   return (size_t)-1 ;
}

//----------------------------------------------------------------------

size_t FrSparseArray::locate(const FrObject *finditem, FrCompareFunc cmp,
			      size_t start) const
{
   if (!items || arrayLength() == 0)
      return (size_t)-1 ;
   if (start == (size_t)-1)
      start = 0 ;
   else
      start++ ;
   start = findIndexOrHigher(start) ;
   if (finditem && finditem->consp())
      {
      for (size_t pos = start ; pos < arrayLength() ; pos++)
	 {
	 FrArrayItem &itm = item(pos) ;
	 if (cmp(itm.getValue(),((FrList*)finditem)->first()))
	    return (size_t)itm.getIndex() ;
	 }
      }
   else
      {
      for (size_t pos = start ; pos < arrayLength() ; pos++)
	 {
	 FrArrayItem &itm = item(pos) ;
	 if (cmp(itm.getValue(),finditem))
	    return itm.getIndex() ;
	 }
      }
   return (size_t)-1 ;
}

//----------------------------------------------------------------------

bool FrSparseArray::iterateVA(FrIteratorFunc func, va_list args) const
{
   for (size_t pos = 0 ; pos < arrayLength() ; pos++)
      {
      FrArrayItem &itm = item(pos) ;
      FrSafeVAList(args) ;
      bool success = func(itm.getValue(),FrSafeVarArgs(args)) ;
      FrSafeVAListEnd(args) ;
      if (!success)
	 return false ;
      }
   return true ;
}

//----------------------------------------------------------------------

ostream &FrSparseArray::printValue(ostream &output) const
{
   output << "#SpArr(" ;
   size_t loc = FramepaC_initial_indent + 3 ;
   size_t orig_indent = FramepaC_initial_indent ;
   for (size_t pos = 0 ; pos < arrayLength() ; pos++)
      {
      FrArrayItem &itm = item(pos) ;
      FramepaC_initial_indent = loc ;
      FrInteger indexnum(itm.getIndex()) ;
      FrObject *value = (FrObject*)itm.getValue() ;
      size_t indexlen = indexnum.displayLength() ;
      size_t valuelen = value ? value->displayLength() : 2 ;
      size_t len = (indexlen + valuelen + 3) ;
      loc += len ;
      if (loc > FramepaC_display_width && pos != 0)
	 {
	 loc = orig_indent+7 ;
	 output << '\n' << setw(loc) << " " ;
	 FramepaC_initial_indent = loc ;
	 loc += len ;
	 }
      output << '(' << indexnum << ' ' ;
      if (value)
	 value->printValue(output) ;
      else
	 output << "()" ;
      output << ')' ;
      if (pos < arrayLength()-1)
	 output << ' ' ;
      }
   FramepaC_initial_indent = orig_indent ;
   return output << ")" ;
}

//----------------------------------------------------------------------

char *FrSparseArray::displayValue(char *buffer) const
{
   memcpy(buffer,"#SpArr(",7) ;
   buffer += 7 ;
   for (size_t pos = 0 ; pos < arrayLength() ; pos++)
      {
      FrArrayItem &itm = item(pos) ;
      *buffer ++ = '(' ;
      FrInteger indexnum(itm.getIndex()) ;
      FrObject *value = (FrObject*)itm.getValue() ;
      buffer = indexnum.displayValue(buffer) ;
      *buffer++ = ' ' ;
      if (value)
	 buffer = value->displayValue(buffer) ;
      else
	 {
	 *buffer++ = '(' ;
	 *buffer++ = ')' ;
	 }
      *buffer++ = ')' ;
      if (pos < arrayLength()-1)
	 *buffer++ = ' ' ;		// separate the elements
      }
   *buffer++ = ')' ;
   *buffer = '\0' ;			// ensure proper string termination
   return buffer ;
}

//----------------------------------------------------------------------

size_t FrSparseArray::displayLength() const
{
   size_t len = 8 ;			// for the #A()
   if (arrayLength() > 1)
      len += (arrayLength()-1) ;		// also count the blanks we add
   for (size_t pos = 0 ; pos < arrayLength() ; pos++)
      {
      FrArrayItem &itm = item(pos) ;
      FrInteger indexnum(itm.getIndex()) ;
      FrObject *value = (FrObject*)itm.getValue() ;
      len += indexnum.displayLength() + 3 ;
      if (value)
	 len += value->displayLength() ;
      else
	 len += 2 ;
      }
   return len ;
}

//----------------------------------------------------------------------

FrObject *FrSparseArray::insert(const FrObject *itm, size_t pos,
				  bool copyitem)
{
   if (!items)
      {
      if (range_errors)
	 FrWarning(errmsg_bounds) ;
      return this ;			// can't insert!
      }
   pos = findIndexOrHigher(pos) ;
   size_t numitems ;
   if (itm && itm->consp())
      {
      numitems = ((FrList*)itm)->listlength() ;
      if (numitems == 1)
	 itm = ((FrList*)itm)->first() ;
      }
   else
      numitems = 1 ;
#if 0  //FIXME: need to do a proper insertion that takes possible overlap in
   // indices into account
   expandTo(arrayLength()+numitems) ;
   m_length += numitems ;
   // make room for the new items
   if (arrayLength() > 0)
      {
      for (size_t i = arrayLength()-1 ; i >= pos+numitems  ; i--)
	 array[i] = array[i-numitems] ;
      }
   // and now copy them into the hole we just made
   if (numitems == 1)
      array[pos] = (copyitem && itm) ? itm->deepcopy() : (FrObject*)itm ;
   else
      {
      for (size_t i = pos ; i < pos+numitems ; i++)
	 {
	 array[i] = ((FrList*)itm)->first() ;
	 itm = ((FrList*)itm)->rest() ;
	 }
      }
#else
   (void)copyitem ;
#endif /* 0 */
   return this ;
}

//----------------------------------------------------------------------

FrObject *FrSparseArray::elide(size_t start, size_t end)
{
   if (end < start)
      end = start ;
   start = findIndexOrHigher(start) ;
   end = findIndexOrHigher(end) ;
   if (start >= arrayLength())
      {
      if (range_errors)
	 FrWarning(errmsg_bounds) ;
      return this ;			// can't elide that!
      }
   if (end >= arrayLength())
      end = arrayLength()-1 ;
   size_t elide_size = end-start+1 ;
   // remove the requested section of the array
   size_t i ;
   for (i = start ; i <= end ; i++)
      {
      FrArrayItem &itm = item(i) ;
      FrObject *value = itm.getValue() ;
      if (value)
	 value->freeObject() ;
      }
   for (i = start ; i < arrayLength()-elide_size ; i++)
      item(i) = item(i+elide_size) ;
   // resize our storage downward, freeing any fragments which are no longer
   // in use
   size_t newlength = arrayLength() - elide_size ;
   size_t frags = array_frags(arrayLength()) ;
   size_t newfrags = array_frags(newlength) ;
   m_length = newlength ;
   for (i = frags ; i < newfrags ; i++)
      {
      delete items[i] ;
      items[i] = 0 ;
      }
   return this ;
}

//----------------------------------------------------------------------

FrObject *FrSparseArray::subseq(size_t start, size_t stop) const
{
   if (stop < start)
      stop = start ;
   start = findIndexOrHigher(start) ;
   stop = findIndexOrHigher(stop) ;
   if (stop >= arrayLength())
      stop = arrayLength()-1 ;
   if (items && start < arrayLength())
      return new FrSparseArray(this,start,stop) ;
   else
      {
      if (range_errors)
	 FrWarning(errmsg_bounds) ;
      return 0 ;			// can't take that subsequence!
      }
}

//----------------------------------------------------------------------

bool FrSparseArray::equal(const FrObject *obj) const
{
   if (obj == this)
      return true ;
   else if (!obj || !obj->arrayp())
      return false ;			// can't be equal to non-array
   else if (obj->objType() == OT_FrSparseArray)
      {
      FrSparseArray *arr = (FrSparseArray*)obj ;
      if (arrayLength() != arr->arrayLength())
	 return false ;
      for (size_t i = 0 ; i < arrayLength() ; i++)
	 {
	 FrArrayItem &itm1 = item(i) ;
	 FrArrayItem &itm2 = arr->item(i) ;
	 if (itm1.getIndex() != itm2.getIndex() ||
	     !equal_inline(itm1.getValue(),itm2.getValue()))
	    return false ;
	 }
      return true ;
      }
   else
      {
      return false ; //!!! for now
      }
}

//----------------------------------------------------------------------

int FrSparseArray::compare(const FrObject *obj) const
{
   if (!obj)
      return 1 ;			// anything is gt NIL / empty-list
   else if (!obj->arrayp())
      return -1 ;			// non-arrays come after arrays
   else if (obj->objType() == OT_FrSparseArray)
      {
      FrSparseArray *arr = (FrSparseArray*)obj ;
      size_t min_length = FrMin(arr->arrayLength(),arrayLength()) ;
      for (size_t i = 0 ; i < min_length ; i++)
	 {
	 FrArrayItem &itm1 = item(i) ;
	 FrArrayItem &itm2 = arr->item(i) ;
	 uintptr_t idx1 = itm1.getIndex() ;
	 uintptr_t idx2 = itm2.getIndex() ;
	 if (idx1 < idx2)		// first list has nonzero entry
	    return 1 ;			//    where second has NIL
	 else if (idx1 > idx2)		// second list has nonzero entry
	    return -1 ;			//    where first has NIL
	 int cmp = itm1.getValue()->compare(itm2.getValue()) ;
	 if (cmp)
	    return cmp ;
	 }
      // if we got here, the common portion is equal
      if (arrayLength() == arr->arrayLength())
	 return 0 ;
      else
	 return arrayLength() > arr->arrayLength() ? 1 : -1 ; // longer array comes later
      }
   else
      {
      return -1 ;			// non-arrays come after arrays
      }
}

//----------------------------------------------------------------------

FrObject *FrSparseArray::removeDuplicates() const
{
   FrSparseArray *result = new FrSparseArray(arrayLength()) ;
   if (result)
      {
      if (arrayLength() > 0 && items)
	 {
	 size_t j = 0 ;
	 for (size_t i = 0 ; i < arrayLength() ; i++)
	    {
	    FrArrayItem &itm = item(i) ;
	    FrObject *value = itm.getValue() ;
	    if (result->locate(value) != (size_t)-1)
	       {
	       new (&result->item(j++))
		     FrArrayItem(itm.getIndex(),value ? value->deepcopy() : 0);
	       }
	    }
	 }
      }
   else
      FrNoMemory("in FrSparseArray::removeDuplicates") ;
   return result ;
}

//----------------------------------------------------------------------

FrObject *FrSparseArray::removeDuplicates(FrCompareFunc cmp) const
{
   FrSparseArray *result = new FrSparseArray(arrayLength()) ;
   if (result)
      {
      if (arrayLength() > 0 && items)
	 {
	 size_t j = 0 ;
	 for (size_t i = 0 ; i < arrayLength() ; i++)
	    {
	    FrArrayItem &itm = item(i) ;
	    FrObject *value = itm.getValue() ;
	    if (result->locate(value,cmp) != (size_t)-1)
	       {
	       new (&result->item(j++))
		     FrArrayItem(itm.getIndex(),value ? value->deepcopy() : 0);
	       }
	    }
	 }
      }
   else
      FrNoMemory("in FrSparseArray::removeDuplicates") ;
   return result ;
}

/************************************************************************/
/************************************************************************/

static FrObject *read_FrSparseArray(istream &input, const char *)
{
   FrObject *initializer = read_FrObject(input) ;
   FrArray *array ;
   if (initializer && initializer->consp())
      array = new FrSparseArray((FrList*)initializer) ;
   else
      array = new FrSparseArray ;
   free_object(initializer) ;
   return array ;
}

//----------------------------------------------------------------------

static FrObject *string_to_FrSparseArray(const char *&input, const char *)
{
   FrObject *initializer = string_to_FrObject(input) ;
   FrArray *array ;
   if (initializer && initializer->consp())
      array = new FrSparseArray((FrList*)initializer) ;
   else
      array = new FrSparseArray ;
   free_object(initializer) ;
   return array ;
}

//----------------------------------------------------------------------

static bool verify_FrSparseArray(const char *&input, const char *,
				   bool strict)
{
   return valid_FrObject_string(input,strict) ;
}

//----------------------------------------------------------------------

// end of file frsarray.cpp //
