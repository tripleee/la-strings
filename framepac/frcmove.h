/************************************************************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frcmove.h	conditional moves				*/
/*  LastEdit: 04nov09							*/
/*									*/
/*  (c) Copyright 1994,1995,1996,1997,1998,2001 Ralf Brown		*/
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

#ifndef __FRCMOVE_H_INCLUDED
#define __FRCMOVE_H_INCLUDED

#ifndef __FRCONFIG_H_INCLUDED
#include "frconfig.h"
#endif

/**********************************************************************/
/**********************************************************************/

#define FrCompare(val1,val2) (val1==val2 ? 0 : (val1 < val2 ? -1 : 1))

//----------------------------------------------------------------------

#define FrSigNum(val) (val > 0 ? 1 : (val < 0 ? -1 : 0))

//----------------------------------------------------------------------

//#define FrMax(val1,val2) (val1 > val2 ? val1 : val2)
inline int FrMax(int val1,int val2) { return (val1 > val2 ? val1 : val2) ; }

//----------------------------------------------------------------------

//#define FrMin(val1,val2) (val1 < val2 ? val1 : val2)
inline int FrMin(int val1,int val2) { return (val1 < val2 ? val1 : val2) ; }

//----------------------------------------------------------------------

#define FrCMOV(dest,test,iftrue,iffalse) (dest = test ? iftrue : iffalse)

//----------------------------------------------------------------------


#endif /* !__FRCMOVE_H_INCLUDED */

// end of file frcmove.h
