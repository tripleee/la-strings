/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC -- frame manipulation in C++				*/
/*  Version 1.98 							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frsymhsh.h	 class FrSymHashTable				*/
/*  LastEdit: 04nov09							*/
/*									*/
/*  (c) Copyright 1994,1995,1996,1997,1998,1999,2001,2002,2003,2005,	*/
/*		2009 Ralf Brown						*/
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

#ifndef __FRSYMHSH_H_INCLUDED
#define __FRSYMHSH_H_INCLUDED

#ifndef __FRSYMBOL_H_INCLUDED
#include "frsymbol.h"
#endif

#ifndef __FRMEM_H_INCLUDED
#include "frmem.h"
#endif

#if defined(__GNUC__)
#  pragma interface
#endif

/************************************************************************/
/*    Declarations for class FrSymHashTable			     	*/
/************************************************************************/

#define FrHASHTABLE_MIN_INCREMENT 16

class FrSymHashEntry
   {
   private:
      static FrAllocator allocator ;
   protected:
      const FrSymbol *name ;
      void *user_data ;
   public:
      void *operator new(size_t) { return allocator.allocate() ; }
      void *operator new(size_t,void *where) { return where ; }
      void operator delete(void *s) { allocator.release(s) ; }

      // constructors/destructors
      FrSymHashEntry() { name = 0 ; user_data = 0 ; }
      FrSymHashEntry(const FrSymbol *object)
	    { name = object ; user_data = 0 ; }
      FrSymHashEntry(const FrSymbol *object, void *value)
	    { name = object ; user_data = value ; }
      FrSymHashEntry(const FrSymHashEntry &e)
	    { name = e.name ; user_data = e.user_data ; }
      ~FrSymHashEntry() {}

      static void reserve(size_t N) ;
      static void gc() { allocator.gc() ; }

      // variable-setting functions
      void setUserData(void *value) { user_data = value ; }
      void setCount(size_t count) { user_data = (void*)count ; }
      void setValue(FrObject *value) { user_data = (void*)value ; }
      void setList(FrList *list) { user_data = (void*)list ; }

      // access to internal state
      const FrSymbol *getName() const { return name ; }
      void *getUserData() const { return user_data ; }
      size_t getCount() const { return (size_t)user_data ; }
      const FrObject *getValue() const { return (FrObject*)user_data ; }
      FrList *getList() const { return (FrList*)user_data ; }

      static int compare(FrSymHashEntry &e1, FrSymHashEntry &e2)
	 { return e1.name > e2.name ? 1 : (e1.name < e2.name ? -1 : 0) ; }
      static void swap(FrSymHashEntry &e1, FrSymHashEntry &e2)
	 { FrSymHashEntry tmp = e1 ; e1 = e2 ; e2 = tmp ; }

      // friends
      friend class FrSymHashTable ;
   } ;

//----------------------------------------------------------------------

class FrSymHashChain : public FrSymHashEntry
   {
   private:
      static FrAllocator allocator ;
   protected:
      FrSymHashChain *next ;
   public:
      void *operator new(size_t) { return allocator.allocate() ; }
      void *operator new(size_t,void *where) { return where ; }
      void operator delete(void *s) { allocator.release(s) ; }

      FrSymHashChain() { next = 0 ; }
      FrSymHashChain(const FrSymbol *object) : FrSymHashEntry(object)
	    { next = 0 ; }
      FrSymHashChain(const FrSymbol *object, void *value)
	    : FrSymHashEntry(object,value)
	    { next = 0 ; }
      FrSymHashChain(const FrSymHashEntry &e) : FrSymHashEntry(e)
	    { next = 0 ; }
      ~FrSymHashChain() {}

      static void reserve(size_t N) ;
      static void gc() { allocator.gc() ; }
      // accessors
      FrSymHashChain *nextEntry() const { return next ; }

      // friends of this class
      friend class FrSymHashTable ;
   } ;

/************************************************************************/
/************************************************************************/

typedef bool FrSymHashEntriesFunc(FrSymHashEntry *,va_list) ;

class FrSymHashTable			// may actually be a sorted array
{
   private:
      static FrAllocator allocator ;
   protected:
      size_t size ;			// size of hash array
      size_t curr_elements ;		// number of entries actually in table
      FrSymHashChain **elements ;	// hash array
      FrSymHashEntry *array ;		// items stored in table
      FrSymHashEntriesFunc *cleanup_fn ;// invoke on destruction of obj
      bool loading_packed ;		// are we loading directly into array?
      bool loading_complete ;		// have we sorted the array yet?
   public:
      void *operator new(size_t) { return allocator.allocate() ; }
      void operator delete(void *s) { allocator.release(s) ; }
      FrSymHashTable(const FrSymHashTable &ht) ;
      FrSymHashTable(size_t initial_size = 513) ;
      ~FrSymHashTable() ;
      void loadPacked() ;
      void expand(unsigned int increment) ;
      void expandTo(unsigned int newsize) ;
      void pack() ;
      FrSymHashTable *deepcopy() const ;
      FrSymHashEntry *add(const FrSymHashEntry *entry) ;
      FrSymHashEntry *add(const FrSymbol *key, void *udata = 0) ;
      FrSymHashEntry *add(const FrSymbol *key, size_t udata)
	    { return add(key,(void*)udata) ; }
      int remove(const FrSymHashEntry *entry) ;
      FrSymHashEntry *lookup(const FrSymbol *key) const ;
      bool __FrCDECL doHashEntries(FrSymHashEntriesFunc *func,...) ;
      static void gc() { allocator.gc() ; }
      void onDelete(FrSymHashEntriesFunc *func) { cleanup_fn = func ; }

      // access to internal state
      size_t currentSize() const { return curr_elements ; }
      bool isPacked() const { return array != 0 ; }
      FrList *allKeys() const ;
} ;

typedef FrSymHashTable FrSymHashTable ;

#endif /* !__FRSYMHSH_H_INCLUDED */

// end of file frsymhsh.h //
