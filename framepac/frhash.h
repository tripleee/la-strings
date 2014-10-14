/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC -- frame manipulation in C++				*/
/*  Version 1.98 							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frhash.h	 class FrHashTable				*/
/*  LastEdit: 04nov09							*/
/*									*/
/*  (c) Copyright 1994,1995,1996,1997,1999,2001,2008,2009 Ralf Brown	*/
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

#ifndef __FRHASH_H_INCLUDED
#define __FRHASH_H_INCLUDED

#ifndef __FRAMERR_H_INCLUDED
#include "framerr.h"
#endif

#ifndef __FROBJECT_H_INCLUDED
#include "frobject.h"
#endif

#ifndef __FRMEM_H_INCLUDED
#include "frmem.h"
#endif

#ifndef __FRREADER_H_INCLUDED
#include "frreader.h"
#endif

#if defined(__GNUC__)
#  pragma interface
#endif

/************************************************************************/
/*    Declarations for class FrHashTable			     	*/
/************************************************************************/

#define DEFAULT_HASHTABLE_SIZE 1023
#define HASHTABLE_FILL_FACTOR  2    /* avg 2 items per bucket */
#define HASHTABLE_MIN_INCREMENT 16
#define MAX_HASHTABLE_SIZE (UINT_MAX / HASHTABLE_FILL_FACTOR / sizeof (FrHashEntry *))

enum FrHashEntryType
   {
   HE_none,
   HE_base,
   HE_FrObject,
   HE_VFrame,
   HE_Server
   } ;

class FrHashEntry : public FrObject
   {
   private:
      //
   public:
      FrHashEntry *next ;
      bool in_use ;
   public:
      void *operator new(size_t size) { return FrMalloc(size) ; }
      void operator delete(void *obj) { FrFree(obj) ; }
      FrHashEntry() { next = 0 ; in_use = false ; }
      virtual ~FrHashEntry() ;
      virtual bool hashp() const ;
      virtual FrObjectType objType() const ;
      virtual const char *objTypeName() const ;
      virtual FrObjectType objSuperclass() const ;
      virtual FrHashEntryType entryType() const ;
      virtual FrSymbol *entryName() const ;
      virtual int sizeOf() const ;
      virtual size_t hashIndex(int size) const ;
      virtual int keycmp(const FrHashEntry *entry) const ;
      virtual bool prefixMatch(const char *nameprefix,int len) const ;
      virtual FrObject *copy() const ;
      virtual FrObject *deepcopy() const ;
   } ;

/************************************************************************/
/************************************************************************/

class FrHashEntryObject : public FrHashEntry
   {
   private:
      static FrAllocator allocator ;
      static FrReader reader ;
   protected:
      FrObject *obj ;
      void *user_data ;
   public:
      void *operator new(size_t) { return allocator.allocate() ; }
      void operator delete(void *s) { allocator.release(s) ; }
      FrHashEntryObject() { obj = 0 ; user_data = 0 ; }
      FrHashEntryObject(const FrObject *object)
	    { obj = object ? object->deepcopy() : 0 ; user_data = 0 ; }
      FrHashEntryObject(const FrObject *object, void *udata)
	    { obj = object ? object->deepcopy() : 0 ; user_data = udata ; }
      FrHashEntryObject(FrObject *object, void *udata, bool)
	    { obj = object ; user_data = udata ; }
      virtual ~FrHashEntryObject() ;
      virtual FrReader *objReader() const { return &reader ; }
      virtual ostream &printValue(ostream &output) const ;
      virtual char *displayValue(char *buffer) const ;
      virtual size_t displayLength() const ;
      virtual void freeObject() ;
      const FrObject *getObject() const { return obj ; }
      void clearObject() { obj = 0 ; } // only w/ (FrObject,void*,bool) ctor
      void *getUserData() const { return user_data ; }
      void setUserData(void *data) { user_data = data ; }
      virtual FrHashEntryType entryType() const ;
      virtual FrSymbol *entryName() const ;
      virtual int sizeOf() const ;
      virtual size_t hashIndex(int size) const ;
      virtual int keycmp(const FrHashEntry *entry) const ;
      virtual bool prefixMatch(const char *nameprefix,int len) const ;
      virtual FrObject *copy() const ;
      virtual FrObject *deepcopy() const ;

      static void reserve(size_t N) ;
   } ;

/************************************************************************/
/************************************************************************/

typedef FrHashEntry *FrHashEntryPtr ;
typedef bool DoHashEntriesFunc(FrHashEntry *,va_list) ;

class FrHashTable : public FrObject
   {
   private:
      static FrReader reader ;
      FrHashEntryPtr *elements ;    // hash array
      size_t size ;		    // size of hash array
      size_t curr_elements ;  	    // number of entries actually in table
      size_t max_elements ;         // number of entries at which to expand tbl
      int fill_factor ;             // avg number of elements per hash bucket
   public:
      void *operator new(size_t size) { return FrMalloc(size) ; }
      void operator delete(void *obj) { FrFree(obj) ; }
      FrHashTable() ;
      FrHashTable(int fill_factor) ;
      virtual ~FrHashTable() ;
      virtual FrObjectType objType() const ;
      virtual const char *objTypeName() const ;
      virtual FrObjectType objSuperclass() const ;
      virtual void freeObject() ;
      virtual FrReader *objReader() const { return &reader ; }
      virtual ostream &printValue(ostream &output) const ;
      virtual char *displayValue(char *buffer) const ;
      virtual size_t displayLength() const ;
      virtual bool hashp() const ;
      virtual FrHashEntryType entryType() const ;
      virtual size_t length() const ;
      virtual bool iterateVA(FrIteratorFunc func, va_list) const ;
      virtual bool expand(size_t increment) ;
      virtual bool expandTo(size_t newsize) ;
      virtual FrObject *deepcopy() const ;
      FrHashEntry *add(const FrHashEntry *entry) ;
      int remove(const FrHashEntry *entry) ;
      FrHashEntry *lookup(const FrHashEntry *entry) const ;
      FrList *prefixMatches(const char *prefix) const ;
      char *completionFor(const char *prefix) const ;
      bool __FrCDECL doHashEntries(bool (*func)(FrHashEntry *,va_list),
				     ...) ;
      // access to internal state
      size_t currentSize() const { return curr_elements ; }
      size_t maxSize() const { return max_elements ; }
      int fillFactor() const { return fill_factor ; }
   } ;

typedef FrHashTable FrHashTable ;

#endif /* !__FRHASH_H_INCLUDED */

// end of file frhash.h //
