/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File: frarray.h	 class FrArray	 				*/
/*  LastEdit: 04nov09							*/
/*									*/
/*  (c) Copyright 1996,1997,1998,1999,2001,2002,2006,2009 Ralf Brown	*/
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

#ifndef __FRARRAY_H_INCLUDED
#define __FRARRAY_H_INCLUDED

#ifndef __FROBJECT_H_INCLUDED
#include "frobject.h"
#endif

#ifndef __FRMEM_H_INCLUDED
#include "frmem.h"
#endif

#if defined(__GNUC__)
#  pragma interface
#endif

/**********************************************************************/
/**********************************************************************/

#define FrARRAY_FRAG_SIZE 16

/**********************************************************************/
/*	declaration of class FrArray				      */
/**********************************************************************/

class FrArray : public FrObject
   {
   private:
      static FrAllocator allocator ;
   protected:
      size_t 	  m_length ;		// number of elements in array
      FrObject **m_array ;		// the actual array elements
   public:
      static bool range_errors ;
   protected: // methods
      FrArray() ;
   public:
      void *operator new(size_t size)
	 { return (size == sizeof(FrArray)) ? allocator.allocate()
					     : FrMalloc(size) ; }
      void operator delete(void *block,size_t size)
	 { if (size == sizeof(FrArray)) allocator.release(block) ;
	   else FrFree(block) ; }
      FrArray(size_t size, const FrList *init = 0) ;
      FrArray(size_t size, const FrList *init, bool copyitems) ;
      FrArray(size_t size, const FrObject **init, bool copyitems = true) ;
      virtual ~FrArray() ;
      virtual FrObjectType objType() const ;
      virtual const char *objTypeName() const ;
      virtual FrObjectType objSuperclass() const ;
      virtual ostream &printValue(ostream &output) const ;
      virtual char *displayValue(char *buffer) const ;
      virtual size_t displayLength() const ;
      virtual FrObject *copy() const ;
      virtual FrObject *deepcopy() const ;
      virtual unsigned long hashValue() const ;
   //virtual member functions for increased performance/ease-of-use
      virtual bool arrayp() const ;
      virtual void freeObject() ;
      virtual size_t length() const ;
      virtual FrObject *car() const ;
      virtual bool equal(const FrObject *obj) const ;
      virtual int compare(const FrObject *obj) const ;
      virtual bool iterateVA(FrIteratorFunc func, va_list) const ;
      virtual FrObject *subseq(size_t start, size_t stop) const ;
      virtual FrObject *reverse() ;
      virtual FrObject *getNth(size_t N) const ;
      virtual bool setNth(size_t N, const FrObject *elt) ;
      virtual size_t locate(const FrObject *item,
			    size_t start = (size_t)-1) const ;
      virtual size_t locate(const FrObject *item,
			    FrCompareFunc func,
			    size_t start = (size_t)-1) const ;
      virtual FrObject *insert(const FrObject *,size_t location,
			        bool copyitem = true) ;
      virtual FrObject *elide(size_t start, size_t end) ;
      virtual FrObject *removeDuplicates() const ;
      virtual FrObject *removeDuplicates(FrCompareFunc) const ;
      virtual bool expand(size_t increment) ;
      virtual bool expandTo(size_t newsize) ;
   //non-virtual member functions
      size_t arrayLength() const { return m_length ; }
      void trim(size_t newsize) { if (newsize<m_length) m_length = newsize ; }
   //overloaded operators
      FrObject *& operator [] (size_t N) ;
      const FrObject *& operator [] (size_t N) const ;
   } ;

//----------------------------------------------------------------------

class FrArrayItem
{
   private:
      uintptr_t 	m_index ;
      FrObject        *m_value ;
   public:
      void *operator new(size_t,void *where) { return where ; }
      FrArrayItem() {}
      FrArrayItem(uintptr_t i) { m_index = i ; m_value = 0 ; }
      FrArrayItem(uintptr_t i, FrObject *val)
         { m_index = i ; m_value = val ; }
      void addOccurrence(size_t cnt)
	 { m_value = (FrObject*)(((size_t)m_value) + cnt) ; }
      void clearValue() { m_value = 0 ; }
      void setValue(FrObject *val) { m_value = val ; }
      uintptr_t getIndex() const { return m_index ; }
      FrObject *getValue() const { return m_value ; }
      FrObject **getValueRef() { return &m_value ; }
} ;

//----------------------------------------------------------------------

struct FrArrayFrag
{
   private:
      static FrAllocator allocator ;
   public:
      FrArrayItem items[FrARRAY_FRAG_SIZE] ;
   public:
      void *operator new(size_t) { return allocator.allocate() ; }
      void operator delete(void *blk) { allocator.release(blk) ; }
      FrArrayFrag() {}
      ~FrArrayFrag() {}
      static void reserve(size_t N) ;
} ;

//----------------------------------------------------------------------

#define FrArrAdd_RETAIN		0
#define FrArrAdd_INCREMENT	1
#define FrArrAdd_REPLACE	2

class FrSparseArray : public FrArray
{
   private:
      static FrAllocator allocator ;
   protected: // data
      size_t   max_items ;		// size of buffer for candidate trans
      FrArrayFrag **items ;		// two-level array of corr. target wrds
   protected: // methods
      void allocFragmentList() ;
   public:
      void *operator new(size_t) { return allocator.allocate() ; }
      void operator delete(void *blk) { allocator.release(blk) ; }
      FrSparseArray() ;
      FrSparseArray(size_t max) ;
      FrSparseArray(const FrList *initializer) ;
      ~FrSparseArray() ;
      virtual FrObjectType objType() const ;
      virtual const char *objTypeName() const ;
      virtual FrObjectType objSuperclass() const ;
      virtual void freeObject() ;
      virtual FrObject *car() const ;
      virtual bool equal(const FrObject *obj) const ;
      virtual int compare(const FrObject *obj) const ;
      virtual bool iterateVA(FrIteratorFunc func, va_list) const ;
      virtual FrObject *subseq(size_t start, size_t stop) const ;
      virtual FrObject *reverse() ;
      virtual ostream &printValue(ostream &output) const ;
      virtual char *displayValue(char *buffer) const ;
      virtual size_t displayLength() const ;
      virtual FrObject *copy() const ;
      virtual FrObject *deepcopy() const ;
      virtual unsigned long hashValue() const ;

      virtual FrObject *getNth(size_t N) const ;
      virtual bool setNth(size_t N, const FrObject *elt) ;
      virtual size_t locate(const FrObject *item,
			    size_t start = (size_t)-1) const ;
      virtual size_t locate(const FrObject *item,
			    FrCompareFunc func,
			    size_t start = (size_t)-1) const ;
      virtual FrObject *insert(const FrObject *value,size_t location,
				bool copyitem = true) ;
      virtual FrObject *elide(size_t start, size_t end) ;
      virtual FrObject *removeDuplicates() const ;
      virtual FrObject *removeDuplicates(FrCompareFunc) const ;
      virtual bool expand(size_t increment) ;
      virtual bool expandTo(size_t newsize) ;

      FrArrayItem &item(size_t N) const
	    { return items[N/FrARRAY_FRAG_SIZE]->items[N%FrARRAY_FRAG_SIZE];}
      FrObject *lookup(uintptr_t index) const ;
      bool addItem(uintptr_t index, const FrObject *value,
		   int if_exists = FrArrAdd_REPLACE) ;

   //overloaded operators
      FrObject *& operator [] (size_t N) ;

   protected: // methods
      FrSparseArray(const FrSparseArray *init, size_t start, size_t stop) ;
      size_t findIndex(size_t start) ;
      size_t findIndexOrHigher(size_t start) const ;
      bool createAndFillGap(size_t pos, size_t index, const FrObject *value) ;
      void newItem(FrArrayItem *newitem)
	 {
	 // if last fragment is already full, allocate another one
	 if ((m_length % FrARRAY_FRAG_SIZE) == 0)
	    items[m_length/FrARRAY_FRAG_SIZE] = new FrArrayFrag ;
	 item(m_length++) = *newitem ;
	 }
      void newItem(uintptr_t idx, FrObject *val)
	 {
	 // if last fragment is already full, allocate another one
	 if ((m_length % FrARRAY_FRAG_SIZE) == 0)
	    items[m_length/FrARRAY_FRAG_SIZE] = new FrArrayFrag ;
	 new (&item(m_length++)) FrArrayItem(idx,val) ;
	 }
} ;

typedef FrSparseArray FrSparseArray ;

//----------------------------------------------------------------------

#endif /* !__FRARRAY_H_INCLUDED */

// end of file frarray.h //
