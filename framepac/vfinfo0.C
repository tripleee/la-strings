/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File vfinfo0.cpp    -- "virtual memory" backing-store (virt. fns)   */
/*  LastEdit: 04nov09							*/
/*									*/
/*  (c) Copyright 1994,1995,1996,1997,2009 Ralf Brown			*/
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

#include "frpcglbl.h"
#include "vfinfo.h"

/************************************************************************/
/*    Member functions for class VFrameInfo			      	*/
/************************************************************************/

FrObjectType VFrameInfo::objType() const
{
   return OT_VFrameInfo ;
}

//----------------------------------------------------------------------

const char *VFrameInfo::objTypeName() const
{
   return "VFrameInfo" ;
}

//----------------------------------------------------------------------

FrObjectType VFrameInfo::objSuperclass() const
{
   return OT_FrObject ;
}

//----------------------------------------------------------------------

bool VFrameInfo::isFrame(const FrSymbol *) const
{
   return false ;
}

//----------------------------------------------------------------------

bool VFrameInfo::isDeletedFrame(const FrSymbol *) const
{
   return false ;
}

//----------------------------------------------------------------------

int VFrameInfo::prefetchFrames(FrList*)
{
   return 0 ;
}

/************************************************************************/
/*    Non-member functions for class VFrameInfo			      	*/
/************************************************************************/

bool store_VFrame(const FrSymbol *name)
{
   if (VFrame_Info)
      return VFrame_Info->storeFrame(name) ;
   else
      return true ;  // never report error if no backing store
}

//----------------------------------------------------------------------

int synchronize_VFrames(frame_update_hookfunc *hook)
{
   if (VFrame_Info)
      return VFrame_Info->syncFrames(hook) ;
   else
      return 0 ;
}

