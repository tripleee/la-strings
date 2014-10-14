/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC -- frame manipulation in C++				*/
/*  Version 2.00beta							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frhashe.cpp		class FrHashEntryObject			*/
/*  LastEdit: 28oct2013							*/
/*									*/
/*  (c) Copyright 1994,1995,1996,1997,1998,2000,2001,2003,2008,2009,	*/
/*		2011,2013 Ralf Brown					*/
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

#include "frhash.h"
#include "frutil.h"
#include "frpcglbl.h"

#ifdef FrSTRICT_CPLUSPLUS
#  include <iomanip>
#else
#  include <iomanip.h>
#endif /* FrSTRICT_CPLUSPLUS */

/************************************************************************/
/*    Global data local to this module					*/
/************************************************************************/

static const char errmsg_exp_rparen[]
    = "malformed list (expected right parenthesis)" ;

static const char str_HEntry_intro[] = "#HEntryObj(" ;

/************************************************************************/
/*    Global variables for class FrHashEntryObject			*/
/************************************************************************/

static FrObject *read_HEntryObj(istream &input, const char *) ;
static FrObject *string_to_HEntryObj(const char *&input, const char *) ;
static bool verify_HEntryObj(const char *&input, const char *,bool) ;

FrAllocator FrHashEntryObject::allocator("HashEntryObject",
					 sizeof(FrHashEntryObject)) ;

FrReader FrHashEntryObject::reader(string_to_HEntryObj,read_HEntryObj,
				   verify_HEntryObj,
				   FrREADER_LEADIN_LISPFORM,"HEntryObj") ;

/************************************************************************/
/*    Methods for class FrHashEntryObject			     	*/
/************************************************************************/

FrHashEntryObject::~FrHashEntryObject()
{
   if (obj)
      {
      obj->freeObject() ;
      obj = 0 ;
      }
   return ;
}

//----------------------------------------------------------------------

void FrHashEntryObject::freeObject()
{
   delete this ;
   return ;
}

//----------------------------------------------------------------------

void FrHashEntryObject::reserve(size_t N)
{
   size_t objs_per_block = FrFOOTER_OFS / allocator.objectSize() - 20 ;
   for (size_t i = allocator.objects_allocated() ;
	i < N ;
	i += objs_per_block)
      {
      allocator.preAllocate() ;
      }
   return ;
}

//----------------------------------------------------------------------

FrHashEntryType FrHashEntryObject::entryType() const
{
   return HE_FrObject ;
}

//----------------------------------------------------------------------

int FrHashEntryObject::sizeOf() const
{
   return sizeof(FrHashEntryObject) ;
}

//----------------------------------------------------------------------

FrSymbol *FrHashEntryObject::entryName() const
{
   if (obj)
      {
      if (obj->symbolp())
	 return (FrSymbol*)obj ;
      else if (obj->framep())
	 return ((FrFrame*)obj)->frameName() ;
      }
   return 0 ;
}

//----------------------------------------------------------------------

size_t FrHashEntryObject::hashIndex(int size) const
{
   return obj ? (size_t)(obj->hashValue() % size) : (size_t)0 ;
}

//----------------------------------------------------------------------

bool FrHashEntryObject::prefixMatch(const char *nameprefix,int len) const
{
   if (!obj)
      return false ;
   const char *name ;
   if (obj->symbolp())
      {
      name = ((FrSymbol*)obj)->symbolName() ;
      }
   else if (obj->framep())
      {
      FrSymbol *frname = ((FrFrame*)obj)->frameName() ;
      name = frname->symbolName() ;
      }
   else
      return false ;
   return (bool)(strncmp(name,nameprefix,len) == 0) ;
}

//----------------------------------------------------------------------

int FrHashEntryObject::keycmp(const FrHashEntry *entry) const
{
   if (entry->entryType() != HE_FrObject)
      return -1 ;   // other hash entry types sort after this type
   return obj ? obj->compare(((FrHashEntryObject*)entry)->obj) : -1 ;
}

//----------------------------------------------------------------------

FrObject *FrHashEntryObject::copy() const
{
   FrHashEntryObject *result = new FrHashEntryObject ;

   if (result)
      {
      result->obj = obj ? obj->deepcopy() : 0 ;
      result->user_data = user_data ;
      }
   return result ;
}

//----------------------------------------------------------------------

FrObject *FrHashEntryObject::deepcopy() const
{
   FrHashEntryObject *result = new FrHashEntryObject ;

   if (result)
      {
      result->obj = obj ? obj->deepcopy() : 0 ;
      result->user_data = user_data ;
      }
   return result ;
}

//----------------------------------------------------------------------

ostream &FrHashEntryObject::printValue(ostream &output) const
{
   size_t orig_indent = FramepaC_initial_indent ;
   FramepaC_initial_indent += sizeof(str_HEntry_intro)-1 ;
   output << str_HEntry_intro << obj << ')' ;
   FramepaC_initial_indent = orig_indent ;
   return output ;
}

//----------------------------------------------------------------------

char *FrHashEntryObject::displayValue(char *buffer) const
{
   memcpy(buffer,str_HEntry_intro,11) ;
   buffer += 11 ;
   buffer = obj->print(buffer) ;
   *buffer++ = ')' ;
   *buffer = '\0' ;
   return buffer ;
}

//----------------------------------------------------------------------

size_t FrHashEntryObject::displayLength() const
{
   return 12 + (obj ? obj->displayLength() : 3) ;
}

//----------------------------------------------------------------------

static FrObject *string_to_HEntryObj(const char *&input, const char *)
{
   int c ;
   FrHashEntryObject *he ;

   c = *input++ ;
   if (c == '(')
      {
      FrObject *obj = string_to_FrObject(input) ;
      he = new FrHashEntryObject(obj) ;
      if (FrSkipWhitespace(input) == ')')
	 input++ ;	   // throw away the trailing right parenthesis
      else
	 FrWarning(errmsg_exp_rparen) ;
      }
   else
      he = 0 ;
   return he ;
}

//----------------------------------------------------------------------

static bool verify_HEntryObj(const char *&input, const char *, bool)
{
   int c = *input++ ;
   if (c == '(')
      {
      if (valid_FrObject_string(input,true) && FrSkipWhitespace(input) == ')')
	 {
	 input++ ;			// throw away trailing right paren
	 return true ;			// indicate valid HashEntryObject
	 }
      }
   return false ;
}

//----------------------------------------------------------------------

static FrObject *read_HEntryObj(istream &input, const char *)
{
   int c ;
   FrHashEntryObject *he ;

   c = input.get() ;
   if (c == '(')
      {
      FrObject *obj ;
      input >> obj ;
      he = new FrHashEntryObject(obj) ;
      if (FrSkipWhitespace(input) == ')')
	 input.get() ;	   // throw away the trailing right parenthesis
      else
	 FrWarning(errmsg_exp_rparen) ;
      }
   else
      he = 0 ;
   return he ;
}


// end of file frhashe.cpp //
