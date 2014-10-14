/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 2.00beta							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frsymtab.cpp	       class FrSymbolTable			*/
/*  LastEdit: 28oct2013							*/
/*									*/
/*  (c) Copyright 1994,1995,1996,1997,1998,1999,2000,2001,2002,2003,	*/
/*		2004,2005,2006,2009,2010,2011,2013 Ralf Brown		*/
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
#  pragma implementation "frsymtab.h"
#endif

#include <stdlib.h>
#include <string.h>
#include "frassert.h"
#include "frlist.h"
#include "frsymtab.h"
#include "frutil.h"
#include "frpcglbl.h"

//----------------------------------------------------------------------

// force the global constructors for this module to run before those of
// the application program
#ifdef __WATCOMC__
#pragma initialize before library ;
#endif /* __WATCOMC__ */

#if defined(_MSC_VER) && _MSC_VER >= 800
#pragma warning(disable : 4073)    // don't warn about following #pragma ....
#pragma init_seg(lib)
#endif /* _MSC_VER >= 800 */

#if defined(PURIFY) || 1
#  define PURIFYmemcmp(s1,s2,l) (strcmp(s1,s2))
#else
#  define PURIFYmemcmp(s1,s2,l) (memcmp(s1,s2,l))
#endif /* PURIFY */

/************************************************************************/
/*	Types for this module						*/
/************************************************************************/

#define FRAG_SIZE ((FrFOOTER_OFS-64) / sizeof(FrSymbol*))

#ifdef FrSAVE_MEMORY
class FrSymTabPtr
   {
   private:
      FrSymbol ***fragments ;
      size_t num_elts ;
      size_t num_frags ;
   public:
#if defined(PURIFY)
      void *operator new(size_t size) { return ::malloc(size) ; }
      void operator delete(void *obj) { ::free(obj) ; }
#else
      void *operator new(size_t size) { return FrMalloc(size) ; }
      void operator delete(void *obj) { FrFree(obj) ; }
#endif /* PURIFY */
      FrSymTabPtr(size_t size) ;
      ~FrSymTabPtr() ;

      bool expandTo(size_t newsize) ;
      size_t currentSize() const { return num_elts ; }

      FrSymbol *getElt(size_t N) const
	 { return ((fragments[N/FRAG_SIZE])[N%FRAG_SIZE]) ; }
      void setElt(size_t N, FrSymbol *sym)
	    { (fragments[N/FRAG_SIZE])[N%FRAG_SIZE] = sym ; }
   } ;
#endif /* FrSAVE_MEMORY */

/************************************************************************/
/*	Global variables shared with other modules in FramepaC		*/
/************************************************************************/

size_t FrSymbolTable_blocks = 0 ;

_FrFrameInheritanceFunc null_inheritance ;
FrInheritanceType FramepaC_inheritance_type = NoInherit ;
_FrFrameInheritanceFunc *FramepaC_inheritance_function = null_inheritance ;

extern int FramepaC_symbol_blocks ;

void (*FramepaC_destroy_all_symbol_tables)() ;

/************************************************************************/
/*	Global data local to this module				*/
/************************************************************************/

static const char stringISA[] = "IS-A" ;
static const char stringVALUE[] = "VALUE" ;
static const char stringINSTANCEOF[] = "INSTANCE-OF" ;
static const char stringSUBCLASSES[] = "SUBCLASSES" ;
static const char stringINSTANCES[] = "INSTANCES" ;
static const char stringHASPARTS[] = "HAS-PARTS" ;
static const char stringPARTOF[] = "PART-OF" ;

static const char *default_symbols[] =
   {
   //"NIL" is added by create_symbol_table regardless
   "?", stringISA, stringINSTANCEOF, stringSUBCLASSES, stringINSTANCES,
   stringHASPARTS, stringPARTOF, stringVALUE, "SEM", "COMMON", "*EOF*",
   "INHERITS", ".",
   } ;

#ifdef FrSYMBOL_RELATION
static const char *default_relations[][2] =
   {  { stringISA, stringSUBCLASSES },
      { stringINSTANCEOF, stringINSTANCES },
      { stringPARTOF, stringHASPARTS },
   } ;
#endif /* FrSYMBOL_RELATION */

static const char nomem_palloc[]
   = "in palloc()" ;

/************************************************************************/
/*	Global variables local to this module				*/
/************************************************************************/

static FrSymbolTable *symbol_tables = 0 ;
//static FrSymbolTable default_symbol_table(0) ;
static FrSymbolTable *default_symbol_table = 0 ;


static FrSymbolTable *DefaultSymbolTable = 0 ;
FrSymbolTable *ActiveSymbolTable = 0 ;

/************************************************************************/
/*	Forward declarations						*/
/************************************************************************/

void _FramepaC_destroy_all_symbol_tables() ;

/************************************************************************/
/*	Utility functions						*/
/************************************************************************/

#if defined(__WATCOMC__) && defined(__386__)
extern unsigned long symboltable_hashvalue(const char *name,int *length) ;
#pragma aux symboltable_hashvalue = \
	"xor  eax,eax" \
	"xor  edx,edx" \
        "push ebx" \
    "l1: ror  eax,5" \
	"mov  dl,[ebx]" \
	"inc  ebx" \
	"add  eax,edx" \
	"test dl,dl" \
	"jne  l1" \
        "pop  edx" \
	"sub  ebx,edx" \
	"mov  [ecx],ebx" \
	parm [ebx][ecx] \
	value [eax] \
	modify exact [eax ebx edx] ;

#elif defined(__GNUC__) && defined(__386__)
inline unsigned long symboltable_hashvalue(const char *name, int *symlength)
{
   unsigned long hashvalue ;
   unsigned long t ;
   __asm__("xor %1,%1\n\t"
	   "xor %2,%2\n\t"
	   "push %3\n"
     ".LBL%=:\n\t"
           "ror $5,%1\n\t"
	   "movb (%3),%b2\n\t"
	   "inc %3\n\t"
	   "add %2,%1\n\t"
	   "test %b2,%b2\n\t"
	   "jne .LBL%=\n\t"
	   "pop %2\n\t"
	   "sub %2,%3"
	   : "=r" (*symlength), "=&r" (hashvalue), "=&q" (t)
	   : "0" (name)
	   : "cc" ) ;
   return hashvalue ;
}

#elif defined(_MSC_VER) && _MSC_VER >= 800
inline unsigned long symboltable_hashvalue(const char *name,int *symlength)
{
   unsigned long hash ;
   _asm {
          mov ebx,name ;
	  xor eax,eax ;
	  xor edx,edx ;
	  push ebx ;
        } ;
     l1:
   _asm {
          ror eax,5
	  mov dl,[ebx]
	  inc ebx
	  add eax,edx
	  test dl,dl
	  jne l1
	  pop edx
          mov ecx,symlength
	  sub ebx,edx
	  mov hash,eax
	  mov [ecx],ebx
        } ;
   return hash ;
}

#else // neither Watcom C++ nor Visual C++ nor GCC on x86

#define rotr5(X) ((sum << 27) | ((sum >> 5) & 0x07FFFFFF))

// BorlandC++ won't inline functions containing while loops
#ifdef __BORLANDC__
static
#else
inline
#endif /* __BORLANDC__ */
unsigned long symboltable_hashvalue(const char *name,int *length)
{
   register unsigned long sum = 0 ;
   const char *name_start = name ;
   int c ;
   do
      {
      c = *(unsigned char*)name++ ;
      sum = rotr5(sum) + c ;
      } while (c) ;
   *length = (name-name_start) ;
   return sum ;
}
#endif /* __WATCOMC__ && __386__ */

//----------------------------------------------------------------------
// Don't do any inheritance at all

/* ARGSUSED */
const FrList *null_inheritance(FrFrame *frame,
				const FrSymbol *slot,
				const FrSymbol *facet)
{
   // avoid compiler warnings by trivially using each argument
   (void)frame ; (void)slot ; (void)facet ;
   return 0 ;
}

/************************************************************************/
/*	methods for class FrSymTabPtr					*/
/************************************************************************/

#ifdef FrSAVE_MEMORY

FrSymTabPtr::FrSymTabPtr(size_t size)
{
   num_elts = 0 ;
   num_frags = 0 ;
   fragments = 0 ;
   expandTo(size) ;
   return ;
}

//----------------------------------------------------------------------

FrSymTabPtr::~FrSymTabPtr()
{
   for (size_t i = 0 ; i < num_frags ; i++)
      {
      (void)VALGRIND_MAKE_MEM_DEFINED(FrFOOTER_PTR(fragments[i]),sizeof(FrMemFooter)) ;
      FrMemoryPool::addFreePage(FrFOOTER_PTR(fragments[i])) ;
      FrSymbolTable_blocks-- ;
      }
   FrFree(fragments) ;
   fragments = 0 ;
   num_elts = 0 ;
   num_frags = 0 ;
   return ;
}

//----------------------------------------------------------------------

bool FrSymTabPtr::expandTo(size_t newsize)
{
   if (newsize > num_elts)
      {
      size_t new_fragcount = (newsize + FRAG_SIZE - 1) / FRAG_SIZE ;
      if (new_fragcount == num_frags)
	 {
	 num_elts = newsize ;
	 return true ;
	 }
      FrSymbol ***new_frags = FrNewR(FrSymbol**,fragments,new_fragcount) ;
      if (!new_frags)
	 return false ;
      bool bad = false ;
      size_t i ;
      for (i = num_frags ; i < new_fragcount ; i++)
	 {
	 FrMemFooter *footer = allocate_block("expanding symbol table") ;
	 FrSymbol **frag = footer ? (FrSymbol**)footer->objectStartPtr() : 0 ;
	 new_frags[i] = frag ;
	 if (frag)
	    {
	    FrSymbolTable_blocks++ ;
	    for (size_t j = 0 ; j < FRAG_SIZE ; j++)
	       frag[j] = 0 ;
	    }
	 else
	    {
	    bad = true ;
	    break ;
	    }
	 }
      if (bad)
	 {
	 for (size_t j = num_frags ; j < i ; j++)
	    {
	    FrMemoryPool::addFreePage((FrMemFooter*)new_frags[j]) ;
	    FrSymbolTable_blocks-- ;
	    }
	 return false ;
	 }
      num_frags = new_fragcount ;
      fragments = new_frags ;
      num_elts = newsize ;
      }
   return true ;
}

#endif /* FrSAVE_MEMORY */

/************************************************************************/
/*	Methods for class FrSymbolTable					*/
/************************************************************************/

FrSymbolTable::FrSymbolTable(unsigned int max_symbols)
{
   int i ;

   name = 0 ;
   curr_elements = 0 ;
   size = 0 ;
   max_elements = 0 ;
   elements = 0 ;
#ifdef FrLRU_DISCARD
   LRUclock = 0 ;
#endif /* FrLRU_DISCARD */
   bstore_info = 0 ;
   inheritance_type = FramepaC_inheritance_type ;
   inheritance_function = FramepaC_inheritance_function ;
   delete_hook = 0 ;
   init_palloc() ;
   if (max_symbols == 0)
      max_symbols = DEFAULT_SYMBOLTABLE_SIZE ;
   else
      {
      if (max_symbols < FramepaC_NUM_STD_SYMBOLS)
	 max_symbols = FramepaC_NUM_STD_SYMBOLS ;
      }
   if (max_symbols > MAX_SYMBOLTABLE_SIZE)
      max_symbols = MAX_SYMBOLTABLE_SIZE ;
   else if ((max_symbols & 1) == 0)   // try to keep table size odd
      max_symbols++ ;
   size = max_symbols ;
   max_elements = size ;
#ifdef FrSAVE_MEMORY
   elements = new FrSymTabPtr(size) ;
   if (!elements || !elements->currentSize())
#else /* !FrSAVE_MEMORY */
   elements = FrNewC(FrSymTabPtr,size) ;
   if (!elements)
#endif /* FrSAVE_MEMORY */
      {
      max_elements = size = 0 ;
      FrNoMemory("while creating symbol table") ;
      return ;
      }
   std_symbols[0] = 0 ;
   std_symbols[0] = add("NIL",this
#ifdef FrSYMBOL_VALUE
			,0
#endif
                        ) ;
   std_symbols[1] = add("T",this
#ifdef FrSYMBOL_VALUE
			,0
#endif
			) ;
#ifdef FrSYMBOL_VALUE
   std_symbols[1].setValue(symbolT,this) ;
#endif /* FrSYMBOL_VALUE */
   for (i = 0 ; i < (int)lengthof(default_symbols) ; i++)
      std_symbols[i+2] = add(default_symbols[i],this) ;
#ifdef FrSYMBOL_RELATION
   for (i = 0 ; i < (int)lengthof(default_relations) ; i++)
      {
      FrSymbol *relation = add(default_relations[i][0],this) ;
      FrSymbol *inverse =  add(default_relations[i][1],this) ;

      if (relation)
	 relation->defineRelation(inverse) ;
      }
#endif /* FrSYMBOL_RELATION */
   prev = 0 ;
   next = symbol_tables ;
   if (symbol_tables)
      symbol_tables->prev = this ;
   if (!FramepaC_destroy_all_symbol_tables)
      FramepaC_destroy_all_symbol_tables = _FramepaC_destroy_all_symbol_tables;
   return ;
}

//----------------------------------------------------------------------

void FramepaC_delete_symbols(FrMemFooter*);

FrSymbolTable::~FrSymbolTable()
{
   if (DefaultSymbolTable && this == current())
      DefaultSymbolTable->select() ;
   if (delete_hook)
      {
      delete_hook(this) ;
      delete_hook = 0 ;
      }
   if (FramepaC_delete_all_frames)
      FramepaC_delete_all_frames() ;
   FrFree(name) ;
   name = 0 ;
#ifdef FrSAVE_MEMORY
   delete elements ;
#else
   FrFree(elements) ;
#endif /* FrSAVE_MEMORY */
   elements = 0 ;
   curr_elements = max_elements = 0 ;
   free_palloc() ;
   size = 0 ;
   if (this == DefaultSymbolTable)
      DefaultSymbolTable = 0 ;
   if (this == ActiveSymbolTable)
      {
      if (DefaultSymbolTable)
	 DefaultSymbolTable->select() ;
      else if (next)
	 next->select() ;
      else if (prev)
	 prev->select() ;
      else
	 ActiveSymbolTable = 0 ;
      }
   if (next)
      next->prev = prev ;
   if (prev)
      prev->next = next ;
   else
      symbol_tables = next ;
   return ;
}

//----------------------------------------------------------------------

void FrSymbolTable::setName(const char *nam)
{
   FrFree(name) ;
   name = FrDupString(nam) ;
   return ;
}

//----------------------------------------------------------------------

void FrSymbolTable::setDeleteHook(FrSymTabDeleteFunc *delhook)
{
   delete_hook = delhook ;
   return ;
}

//----------------------------------------------------------------------

FrSymbolTable *FrSymbolTable::select()
{
   if (this == 0)
      {
      if (DefaultSymbolTable)
	 return DefaultSymbolTable->select() ;
      else
         return current() ;
      }
   FrSymbolTable *active = current() ;
   if (this != active)
      {
      FrSymbolTable *oldtable = active ;
      ActiveSymbolTable = this ;
      return oldtable ? oldtable : this ;
      }
   else
      return active ;
}

//----------------------------------------------------------------------

FrSymbolTable *FrSymbolTable::selectDefault()
{
   if (!DefaultSymbolTable)
      return current()->select() ;
   return DefaultSymbolTable->select() ;
}

//----------------------------------------------------------------------

FrSymbolTable *FrSymbolTable::current()
{
   if (!ActiveSymbolTable)
      {
      default_symbol_table = new FrSymbolTable(0) ;
      ActiveSymbolTable = default_symbol_table ;
      if (!DefaultSymbolTable)
	 DefaultSymbolTable = ActiveSymbolTable ;
      }
   return ActiveSymbolTable ;
}

//----------------------------------------------------------------------

bool FrSymbolTable::expand(unsigned int increment)
{
   unsigned int oldsize = size ;
   if (size + SYMBOLTABLE_MIN_INCREMENT > MAX_SYMBOLTABLE_SIZE)
      return false ; // don't expand if we'd be forced to exceed the max size
   if (increment < SYMBOLTABLE_MIN_INCREMENT)
      increment = SYMBOLTABLE_MIN_INCREMENT ;
   size += increment ;
   if (size > MAX_SYMBOLTABLE_SIZE)
      size = MAX_SYMBOLTABLE_SIZE ;
   else if ((size & 1) == 0)   // try to keep table size odd
      size++ ;
   if (size == oldsize)
      return false ; // don't need to do anything
   increment = size - oldsize ;
#ifdef FrSAVE_MEMORY
   if (!elements->expandTo(size))
#else /* !FrSAVE_MEMORY */
   FrSymTabPtr *newelts = (FrSymTabPtr*)FrRealloc(elements,
      						  size*sizeof(FrSymTabPtr),
      						  false) ;
   if (!newelts)
#endif /* FrSAVE_MEMORY */
      {
      size = oldsize ;
      FrNoMemory("while expanding symbol table") ;
      return false ;
      }
#ifndef FrSAVE_MEMORY
   elements = newelts ;
#endif
   max_elements = size ;
   unsigned i ;
   // zero out the new array of hash buckets
   for (i = 0 ; i < size ; i++)
      elements->setElt(i,0) ;
   // re-insert the existing symbols into the newly-expanded table
   for (FrMemFooter *symblock=palloc_list ; symblock ; symblock=symblock->next())
      {
      char *base = FrBLOCK_PTR(symblock) ;
      for (i = symblock->objectStart() ; i < FrFOOTER_OFS ; )
	 {
	 FrSymbol *sym = (FrSymbol*)(base + i) ;
	 int len ;
	 unsigned int hash ;
	 hash = (unsigned int)(symboltable_hashvalue(sym->symbolName(),&len) %
			       size) ;
	 i += round_up(Fr_offsetof(*sym,m_name[0]) + len, FrALIGN_SIZE_PTR) ;
	 sym->m_next = elements->getElt(hash) ;
	 elements->setElt(hash,sym) ;
	 }
      }
   // pre-allocate space for the actual symbols to be inserted in the table,
   //   in order to reduce memory fragmentation if the symtab is later deleted
   size_t numbytes = increment * sizeof(FrSymbol) ;
   size_t numblocks = (numbytes + FrFOOTER_OFS-1) / FrFOOTER_OFS ;
   for ( ; numblocks > 0 ; numblocks--)
      {
      FrMemFooter *block = allocate_block("pre-allocating symbol space") ;
      if (!block)
	 break ;
      block->setObjectStart(FrFOOTER_OFS) ;
      block->next(palloc_pool) ;
      palloc_pool = block ;
      FramepaC_symbol_blocks++ ;
      }
//for(i=0;(size_t)i<size;i++){FrSymbol*elt=elements->getElt(i);for(FrSymbol*e=elt;e;e=e->next){assertq(*((int*)e) == *((int*)std_symbols[0]));}}//!!!
   return true ;  // successful
}

//----------------------------------------------------------------------

bool FrSymbolTable::expandTo(unsigned int newsize)
{
   if (newsize > size)
      return expand(newsize-size) ;
   else
      return true ;  // always successful if already large enough
}

//----------------------------------------------------------------------

FrSymbol *FrSymbolTable::add(const char *symname, FrSymbolTable *symtab)
{
   if (!symtab)
      symtab = current() ;
   assertq(symname != 0 && symtab != 0) ;
   int len ;
   register size_t hash = (size_t)(symboltable_hashvalue(symname,&len) %
				   symtab->size) ;
   FrSymbol *elt = symtab->elements->getElt(hash) ;
   for (FrSymbol *e = elt ; e ; e = e->next())
      {
      if (PURIFYmemcmp(symname,e->symbolName(),len) == 0)
	 return e ;
      }
   // if we get here, the requested symbol doesn't yet exist in the symtab
   char *symbuf ;
   unsigned numbytes = round_up(Fr_offsetof(*elt,m_name[0]) + len,
				FrALIGN_SIZE_PTR);
   if (symtab->palloc_list->freecount() >= numbytes)
      {
      symtab->palloc_list->adjObjectStartDown(numbytes) ;
      symbuf = symtab->palloc_list->objectStartPtr() ;
      }
   else
      {
      symbuf = (char*)symtab->palloc(numbytes) ;
      if (!symbuf)
	 return 0 ;
      }
   FrSymbol *newsym = new(symbuf) FrSymbol(symname,len,elt) ;
   symtab->elements->setElt(hash,newsym) ;
   // if the hash table is now "full", expand it by 75%
   if (++symtab->curr_elements > symtab->max_elements)
      symtab->expand(3*symtab->size/4);
   return newsym ;
}

//----------------------------------------------------------------------

FrSymbol *FrSymbolTable::add(const char *symname)
{
   return add(symname,current()) ;
}

//----------------------------------------------------------------------

#ifdef FrSYMBOL_VALUE
FrSymbol *FrSymbolTable::add(const char *name,const FrObject *value)
{
   FrSymbol *sym = FrSymbolTable::add(name) ;
   sym->setValue(value) ;
   return sym ;
}
#endif

//----------------------------------------------------------------------

#ifdef FrSYMBOL_VALUE
FrSymbol *FrSymbolTable::add(const char *name,FrSymbolTable *symtab,
			      const FrObject *value)
{
   FrSymbol *sym = FrSymbolTable::add(name,symtab) ;
   sym->setValue(value) ;
   return sym ;
}
#endif

//----------------------------------------------------------------------

FrSymbol *FrSymbolTable::gensym(const char *basename, const char *user_suffix)
{
   static unsigned long gensym_counter = 0 ;

   char newname[FrMAX_SYMBOLNAME_LEN+1] ;
   char suffix[FrMAX_SYMBOLNAME_LEN+1] ;

   if (!basename || !*basename)
      basename = "GS" ;
   size_t baselen = strlen(basename) ;
   if (baselen > sizeof(newname) - FrMAX_ULONG_STRING - 3)
      baselen = sizeof(newname) - FrMAX_ULONG_STRING - 3 ;
   memcpy(newname,basename,baselen) ;
   if (baselen > 0 && Fr_isdigit(newname[baselen-1]))
      newname[baselen++] = '_' ;
   if (baselen > FrMAX_SYMBOLNAME_LEN-FrMAX_ULONG_STRING)
      baselen = FrMAX_SYMBOLNAME_LEN-FrMAX_ULONG_STRING ;
   size_t suffixlen = 0 ;
   if (user_suffix && *user_suffix)
      {
      memcpy(suffix,user_suffix,sizeof(suffix)) ;
      suffixlen = strlen(user_suffix) ;
      // ensure that we don't overflow our buffer
      if (suffixlen > FrMAX_SYMBOLNAME_LEN-FrMAX_ULONG_STRING-baselen)
	 suffix[FrMAX_SYMBOLNAME_LEN-FrMAX_ULONG_STRING-baselen] = '\0' ;
      }
   char *digits = newname + baselen ;
   int len ;
   register size_t hash ;
   FrSymbol *elt ;
   do {
      ultoa(gensym_counter++,digits,10) ;
      if (suffixlen > 0)
	 strcat(digits,suffix) ;
      hash = (size_t)(symboltable_hashvalue(newname,&len) % size) ;
      elt = elements->getElt(hash) ;
      for ( ; elt ; elt = elt->next())
	 {
	 if (PURIFYmemcmp(newname,elt->symbolName(),len) == 0)
	    break ;
	 }
      } while (elt) ;
   // OK, we now have a name which doesn't exist yet, so add it to the symtab
   char *symbuf ;
   unsigned numbytes = round_up(Fr_offsetof(*elt,m_name[0]) + len,
				FrALIGN_SIZE_PTR);
   if (palloc_list->freecount() >= numbytes)
      {
      palloc_list->adjObjectStartDown(numbytes) ;
      symbuf = palloc_list->objectStartPtr() ;
      }
   else
      {
      symbuf = (char*)palloc(numbytes) ;
      if (!symbuf)
	 return 0 ;			// out of memory!
      }
   FrSymbol *newsym = new(symbuf) FrSymbol(newname,len,
					     elements->getElt(hash)) ;
   elements->setElt(hash,newsym) ;
   if (++curr_elements > max_elements)   	// if hash table is "full",
      expand(3*size/4) ;			// expand table by 75%
   return newsym ;
}

//----------------------------------------------------------------------

FrSymbol *FrSymbolTable::lookup(const char *symname) const
{
   assert(symname != 0) ;
   int len ;
   unsigned int hash = (unsigned int)(symboltable_hashvalue(symname,&len) %
				      size) ;
   register FrSymbol *elt = elements->getElt(hash) ;
   for ( ; elt ; elt = elt->next())
      {
      if (PURIFYmemcmp(symname,elt->symbolName(),len) == 0)
         return elt ;
      }
   return 0 ;	// not found
}

//----------------------------------------------------------------------

void FrSymbolTable::setNotify(VFrameNotifyType type, VFrameNotifyFunc *func)
{
   if (VFrame_Info)
      VFrame_Info->setNotify(type,func) ;
   return ;
}

//----------------------------------------------------------------------

VFrameNotifyPtr FrSymbolTable::getNotify(VFrameNotifyType type) const
{
   return (VFrame_Info) ? VFrame_Info->getNotify(type) : 0 ;
}

//----------------------------------------------------------------------

bool FrSymbolTable::isReadOnly() const
{
   return (VFrame_Info) ? VFrame_Info->isReadOnly() : false ;
}

//----------------------------------------------------------------------

bool FrSymbolTable::iterateVA(FrIteratorFunc func, va_list args) const
{
   if (this == current())
      {
      // force updates to be copied from our global static buffer into the
      // symbol table
      FrSymbolTable *old_symtab = FrSymbolTable::selectDefault() ;
      old_symtab->select() ;
      }
   if (size > 0)
      {
      for (size_t i = 0 ; i < size ; i++)
	 {
	 register FrSymbol *elt = elements->getElt(i) ;
	 for ( ; elt ; elt = elt->next())
	    {
	    FrSafeVAList(args) ;
	    bool success = func(elt,FrSafeVarArgs(args)) ;
	    FrSafeVAListEnd(args) ;
	    if (!success)
	       return false ;
	    }
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

bool __FrCDECL FrSymbolTable::iterate(FrIteratorFunc func, ...) const
{
   va_list args ;
   va_start(args,func) ;
   bool success = iterateVA(func,args) ;
   va_end(args) ;
   return success ;
}

//----------------------------------------------------------------------

bool FrSymbolTable::iterateFrameVA(FrIteratorFunc func, va_list args) const
{
   if (this == current())
      {
      // force updates to be copied from our global static buffer into the
      // symbol table
      FrSymbolTable *old_symtab = FrSymbolTable::selectDefault() ;
      old_symtab->select() ;
      }
   if (size > 0)
      {
      for (size_t i = 0 ; i < size ; i++)
	 {
	 register FrSymbol *elt = elements->getElt(i) ;
	 for ( ; elt ; elt = elt->next())
	    {
	    FrFrame *frame = (FrFrame*)elt->symbolFrame() ;
	    if (frame)
	       {
	       FrSafeVAList(args) ;
	       bool success = func(frame,FrSafeVarArgs(args)) ;
	       FrSafeVAListEnd(args) ;
	       if (!success)
		  return false ;
	       }
	    }
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

bool __FrCDECL FrSymbolTable::iterateFrame(FrIteratorFunc func, ...) const
{
   va_list args ;
   va_start(args,func) ;
   bool success = iterateFrameVA(func,args) ;
   va_end(args) ;
   return success ;
}

//----------------------------------------------------------------------

static bool list_relations(const FrObject *obj, va_list args)
{
   const FrSymbol *sym = (const FrSymbol*)obj ;
   FrVarArg(FrList **,relations) ;
   FrSymbol *inv = sym->inverseRelation() ;
   if (inv && !(*relations)->assoc(inv))
      pushlist(new FrList(sym,inv),*relations) ;
   return true ;	// continue iterating
}

//----------------------------------------------------------------------

FrList *FrSymbolTable::listRelations() const
{
   if (this == current())
      {
      // force updates to be copied from our global static buffer into the
      // symbol table
      FrSymbolTable *old_symtab = FrSymbolTable::selectDefault() ;
      old_symtab->select() ;
      }
   FrList *relations = 0 ;
   (void)FrSymbolTable::iterate(list_relations,&relations) ;
   return relations ;
}

//----------------------------------------------------------------------

void FrSymbolTable::setProxy(VFrameNotifyType type, VFrameProxyFunc *func)
{
   if (VFrame_Info)
      VFrame_Info->setProxy(type,func) ;
   return ;
}

//----------------------------------------------------------------------

VFrameProxyPtr FrSymbolTable::getProxy(VFrameNotifyType type) const
{
   return (VFrame_Info) ? VFrame_Info->getProxy(type) : 0 ;
}

//----------------------------------------------------------------------

void FrSymbolTable::setShutdown(VFrameShutdownFunc *func)
{
   if (VFrame_Info)
      VFrame_Info->setShutdown(func) ;
   return ;
}

//----------------------------------------------------------------------

VFrameShutdownPtr FrSymbolTable::getShutdown() const
{
   return (VFrame_Info) ? VFrame_Info->getShutdown() : 0 ;
}

//----------------------------------------------------------------------

#ifdef FrLRU_DISCARD
void FrSymbolTable::adjust_LRUclock()
{
   if (this == current())
      {
      // force updates to be copied from our global static buffer into the
      // symbol table
      FrSymbolTable *old_symtab = FrSymbolTable::selectDefault() ;
      old_symtab->select() ;
      }
   for (size_t i = 0 ; i < size ; i++)
      {
      register FrSymbol *elt = elements->getElt(i) ;
      for ( ; elt ; elt = elt->next())
	 {
	 FrFrame *frame = elt->symbolFrame() ;
         if (frame)
	    frame->adjust_LRUclock() ;
	 }
      }
   return ;
}
#endif /* FrLRU_DISCARD */

//----------------------------------------------------------------------

int FrSymbolTable::countFrames() const
{
   if (this == current())
      {
      // force updates to be copied from our global static buffer into the
      // symbol table
      FrSymbolTable *old_symtab = FrSymbolTable::selectDefault() ;
      old_symtab->select() ;
      }
   int count = 0 ;
   for (size_t i = 0 ; i < size ; i++)
      {
      register FrSymbol *elt = elements->getElt(i) ;
      for ( ; elt ; elt = elt->next())
	 {
	 if (elt->symbolFrame())
	    count++ ;
	 }
      }
   return count ;
}

//----------------------------------------------------------------------

#ifdef FrLRU_DISCARD
uint32_t FrSymbolTable::oldestFrameClock() const
{
   if (this == current())
      {
      // force updates to be copied from our global static buffer into the
      // symbol table
      FrSymbolTable *old_symtab = FrSymbolTable::selectDefault() ;
      old_symtab->select() ;
      }
   uint32_t oldest = UINT32_MAX ;
   for (size_t i = 0 ; i < size ; i++)
      {
      register FrSymbol *elt = elements->getElt(i) ;
      for ( ; elt ; elt = elt->next())
	 {
	 FrFrame *frame = elt->symbolFrame() ;
	 if (frame)
	    {
	    uint32_t clock = frame->getLRUclock() ;
	    if (clock < oldest)
	       oldest = clock ;
	    }
	 }
      }
   return oldest ;
}
#endif /* FrLRU_DISCARD */

//----------------------------------------------------------------------

bool FrSymbolTable::checkSymbols() const
{
   // scan all symbol objects, checking that they have the correct VMT
   for (FrMemFooter *symblock=palloc_list ; symblock ; symblock=symblock->next())
      {
      char *base = FrBLOCK_PTR(symblock) ;
      for (FrSubAllocOffset i = symblock->objectStart() ; i < FrFOOTER_OFS ; )
	 {
	 FrSymbol *sym = (FrSymbol*)(base + i) ;
	 if (*((int*)sym) != *((int*)std_symbols[0]))
	    return false ;
	 size_t len = strlen(sym->symbolName()) + 1 ;
	 i += round_up(Fr_offsetof(*sym,m_name[0]) + len, FrALIGN_SIZE_PTR) ;
	 }
      }
   return true ;
}

/************************************************************************/
/*      Member functions for class FrSymbolTable                        */
/************************************************************************/

void FrSymbolTable::init_palloc()
{
   palloc_pool = 0 ;
   palloc_list = allocate_block("initializing symbol table") ;
   if (palloc_list)
      {
      FramepaC_symbol_blocks++ ;
      palloc_list->next(0) ;
      palloc_list->setObjectStart(FrFOOTER_OFS) ;
      }
   return ;
}

//----------------------------------------------------------------------
//  make small, "permanent" (never freed unless symbol table is destroyed)
//  memory allocations for symbol names

void *FrSymbolTable::palloc(unsigned int numbytes)
{
   register FrMemFooter *newpalloc ;
   if (palloc_pool)
      {
      newpalloc = palloc_pool ;
      palloc_pool = palloc_pool->next() ;
      }
   else
      {
      newpalloc = allocate_block(nomem_palloc) ;
      if (newpalloc)
	 FramepaC_symbol_blocks++ ;
      else
	 return 0 ;
      }
   newpalloc->next(palloc_list) ;
   newpalloc->setObjectStart(FrFOOTER_OFS - numbytes) ;
   palloc_list = newpalloc ;
   return newpalloc->objectStartPtr() ;
}

//----------------------------------------------------------------------

void FrSymbolTable::free_palloc()
{
   FrMemFooter *block ;
   while (palloc_list)
      {
      block = palloc_list ;
      palloc_list = palloc_list->next() ;
      FrMemoryPool::addFreePage(block) ;
      FramepaC_symbol_blocks-- ;
      }
   while (palloc_pool)
      {
      block = palloc_pool ;
      palloc_pool = palloc_pool->next() ;
      FrMemoryPool::addFreePage(block) ;
      FramepaC_symbol_blocks-- ;
      }
   return ;
}

/**********************************************************************/
/*	Multiple FrSymbol Table Support				      */
/**********************************************************************/

void name_symbol_table(const char *name)
{
   FrSymbolTable *symt = FrSymbolTable::current() ;
   if (symt)
      symt->setName(name) ;
   return ;
}

//----------------------------------------------------------------------

FrSymbolTable *select_symbol_table(const char *table_name)
{
   if (!table_name)
      return 0 ;
   for (FrSymbolTable *symtab = symbol_tables ; symtab ; symtab = symtab->next)
      {
      if (symtab->name && strcmp(symtab->name,table_name) == 0)
	 return symtab->select() ;
      }
   return 0 ;
}

//----------------------------------------------------------------------

void destroy_symbol_table(FrSymbolTable *symtab)
{
   if (symtab && symtab != DefaultSymbolTable)
      {
      if (symtab == FrSymbolTable::current())
	 {
	 if (DefaultSymbolTable)
	    DefaultSymbolTable->select() ;
         else
	    FrError("unable to switch to default symbol table in "
		    "destroy_symbol_table()") ;
         }
      delete symtab ;
      }
   return ;
}

//----------------------------------------------------------------------

void _FramepaC_destroy_all_symbol_tables()
{
   FrSymbolTable *prev = 0 ;

   FrSymbolTable::selectDefault() ;
   // loop until no more symbol tables exist, or the only one is the
   // default symbol table (which never gets destroyed)
   while (symbol_tables && symbol_tables != prev)
      {
      prev = symbol_tables ;
      if (symbol_tables != DefaultSymbolTable)
	 destroy_symbol_table(symbol_tables) ;
      else
	 symbol_tables = symbol_tables->next ;
      }
   return ;
}

//----------------------------------------------------------------------

void __FrCDECL do_all_symtabs(DoAllSymtabsFunc *func, ...)
{
   FrSymbolTable *symtab, *next ;
   FrSymbolTable *orig = FrSymbolTable::current() ;

   symtab = symbol_tables ;
   while (symtab && symtab->next != symtab)
      {
      next = symtab->next ;
      symtab->select() ;
      // note: under Watcom C++, use of the va_list inside the called
      // function affects its value here!  So, we have to reset it each
      // time through the loop
      va_list args ;
      va_start(args, func) ;
      func(symtab,args) ;
      va_end(args) ;
      symtab = next ;
      }
   orig->select() ;
   return ;
}

//----------------------------------------------------------------------

void FramepaC_shutdown_symboltables()
{
   delete default_symbol_table ;
   default_symbol_table = 0 ;
   return ;
}

// end of file frsymtab.cpp //
