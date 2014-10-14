/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC								*/
/*  Version 1.99							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frhsort.cpp		templatized Smoothsort var. of Heapsort */
/*  LastEdit: 16mar10							*/
/*									*/
/*  (c) Copyright 2010 Ralf Brown					*/
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

#include "frhsort.h"

/************************************************************************/
/*	Global data							*/
/************************************************************************/

const uint32_t Fr_leonardo_numbers[] =
   {
      1, 1, 3, 5,
      9, 15, 25, 41,
      67, 109, 177, 287,
      465, 753, 1219, 1973,
      3193, 5167, 8361, 13529,
      21891, 35421, 57313, 92735,
      150049, 242785, 392835, 635621,
      1028457, 1664079, 2692537, 4356617,
      7049155, 11405773, 18454929, 29860703,
      48315633, 78176337, 126491971, 204668309,
      331160281, 535828591, 866988873, 1402817465,
      2269806339, 3672623805  /* , >2**32 */
   } ; 

/************************************************************************/
/************************************************************************/

unsigned FrSmoothSortHeapInfo::trailingZeros(uint64_t value)
{
#if __GNUC__ >= 4
   return value ? __builtin_ctzl(value) : 0 ;
#else
   unsigned count = 0 ;
   while (value && (value & 1) == 0)
      {
      count++ ;
      value >>= 1 ;
      }
   return count ;
#endif
}

// end of file frhsort.cpp //
