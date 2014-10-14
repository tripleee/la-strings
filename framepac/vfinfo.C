/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File vfinfo.cpp    -- "virtual memory" backing-store support        */
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

#include "frhash.h"
#include "vfinfo.h"

/**********************************************************************/
/*    Member functions for class VFrameInfo			      */
/**********************************************************************/

//----------------------------------------------------------------------

FrList *VFrameInfo::prefixMatches(const char *prefix) const
{
   if (hash)
      return hash->prefixMatches(prefix) ;
   else
      return 0 ;
}

//----------------------------------------------------------------------

char *VFrameInfo::completionFor(const char *prefix) const
{
   if (hash)
      return hash->completionFor(prefix) ;
   else
      return 0 ;
}

// end of file vfinfo.cpp //
