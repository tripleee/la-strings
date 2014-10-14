/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 2.00beta							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frsymtab.h	       class FrSymbolTable			*/
/*  LastEdit: 28oct2013							*/
/*									*/
/*  (c) Copyright 1994,1995,1996,1997,1998,2000,2001,2002,2006,2009,	*/
/*		2013 Ralf Brown						*/
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

#ifndef __FRSYMTAB_H_INCLUDED
#define __FRSYMTAB_H_INCLUDED

#ifndef __FRSYMBOL_H_INCLUDED
#include "frsymbol.h"
#endif

#if defined(__GNUC__)
#  pragma interface
#endif

/**********************************************************************/
/*	Types							      */
/**********************************************************************/

enum VFrameNotifyType
   {
   VFNot_CREATE,
   VFNot_DELETE,
   VFNot_UPDATE,
   VFNot_LOCK,
   VFNot_UNLOCK,
   VFNot_PROXYADD,
   VFNot_PROXYDEL
   } ;

typedef void FrSymTabDeleteFunc(FrSymbolTable *) ;

typedef int DoAllSymtabsFunc(FrSymbolTable *, va_list) ;

typedef void VFrameNotifyFunc(VFrameNotifyType type,const FrSymbol *frame) ;
typedef VFrameNotifyFunc *VFrameNotifyPtr ;

typedef bool VFrameProxyFunc(VFrameNotifyType type, const FrSymbol *frame,
			     const FrSymbol *slot, const FrSymbol *facet,
			     const FrObject *filler) ;
typedef VFrameProxyFunc *VFrameProxyPtr ;

typedef void VFrameShutdownFunc(const char *server, int seconds) ;
typedef VFrameShutdownFunc *VFrameShutdownPtr ;

/**********************************************************************/
/*    Declarations for class FrSymbolTable		      	      */
/**********************************************************************/

#define FramepaC_NUM_STD_SYMBOLS 16

#ifdef FrSAVE_MEMORY
class FrSymTabPtr ;
#else
typedef FrSymbol *FrSymTabPtr ;
#endif /* FrSAVE_MEMORY */

class FrSymbolTable
   {
   private:
      FrSymbolTable *next, *prev ;
      char *name ;		// user-supplied name to identify symbol table
      FrSymTabPtr *elements ;		// hash array
      FrMemFooter *palloc_list ;	// buffers for symbol name strings
      FrMemFooter *palloc_pool ;	// pre-alloc bufs for symbol names
      FrSymTabDeleteFunc *delete_hook ; // func to call on symtab deletion
   public:
      FrSymbol *std_symbols[FramepaC_NUM_STD_SYMBOLS] ;
      VFrameInfo *bstore_info ;
      _FrFrameInheritanceFunc *inheritance_function ;
   private:
      unsigned int curr_elements ;	// # of symbols currently in the table
      unsigned int max_elements ;	// max allowed before expanding table
      unsigned int size ;		// size of hash array
   public:
#ifdef FrLRU_DISCARD
      uint32_t LRUclock ;
#endif /* FrLRU_DISCARD */
      FrInheritanceType inheritance_type ;
   private: // member functions
      void init_palloc() ;
      void *palloc(unsigned int numbytes) ;
      void free_palloc() ;
   public: // member functions which should not be called by applications
      int countFrames() const ;
#ifdef FrLRU_DISCARD
      void adjust_LRUclock() ;
      uint32_t currentLRUclock() const { return LRUclock ; }
      uint32_t oldestFrameClock() const ;
#endif /* FrLRU_DISCARD */
   public: // member functions
      void *operator new(size_t size) { return FrMalloc(size) ; }
      void operator delete(void *obj) { FrFree(obj) ; }
      FrSymbolTable(unsigned int max_symbols = 0) ;
      ~FrSymbolTable() ;
      void setName(const char *nam) ;
      bool expand(unsigned int increment) ;
      bool expandTo(unsigned int newsize) ;
      unsigned int tableSize() const { return curr_elements ; }
      FrSymbolTable *select() ;   // make this the current symbol table
      static FrSymbol *add(const char *name) ;  // always adds to current s.t.
      static FrSymbol *add(const char *name,FrSymbolTable *symtab) ;
#ifdef FrSYMBOL_VALUE
      static FrSymbol *add(const char *name,const FrObject *value) ;
      static FrSymbol *add(const char *name,FrSymbolTable *symtab,
			    const FrObject *value) ;
#endif
      FrSymbol *gensym(const char *basename = 0, const char *suffix = 0) ;
      FrSymbol *lookup(const char *name) const ;
      void setNotify(VFrameNotifyType,VFrameNotifyFunc *) ;
      VFrameNotifyPtr getNotify(VFrameNotifyType) const ;
      void setProxy(VFrameNotifyType,VFrameProxyFunc *) ;
      VFrameProxyPtr getProxy(VFrameNotifyType) const ;
      void setShutdown(VFrameShutdownFunc *) ;
      VFrameShutdownPtr getShutdown() const ;
      void setDeleteHook(FrSymTabDeleteFunc *delhook) ;
      FrSymTabDeleteFunc *getDeleteHook() const { return delete_hook ; }
      bool isReadOnly() const ;
      bool __FrCDECL iterate(FrIteratorFunc func, ...) const ;
      bool __FrCDECL iterateFrame(FrIteratorFunc func, ...) const ;
      bool iterateVA(FrIteratorFunc func, va_list) const ;
      bool iterateFrameVA(FrIteratorFunc func, va_list) const ;
      FrList *listRelations() const ;

      // debugging support
      bool checkSymbols() const ;	// verify that all symbols are valid
   //static member functions
      static FrSymbolTable *current() ;  // get currently active symbol table
      static FrSymbolTable *selectDefault() ;
   //friend functions
      friend void __FrCDECL do_all_symtabs(DoAllSymtabsFunc *func,...) ;
      friend FrSymbolTable *select_symbol_table(const char *table_name) ;
      friend void _FramepaC_destroy_all_symbol_tables() ;
      friend FrSymbol *new_FrSymbol(const char *name, size_t len) ;
   } ;

/**********************************************************************/

void name_symbol_table(const char *name) ;
void destroy_symbol_table(FrSymbolTable *symtab) ;
FrSymbolTable *select_symbol_table(const char *table_name) ;

#endif /* !__FRSYMTAB_H_INCLUDED */

// end of frsymtab.h //
