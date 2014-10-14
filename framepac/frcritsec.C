/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 2.00beta							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frcritsec.C		short-duration critical section mutex	*/
/*  LastEdit: 24feb2014							*/
/*									*/
/*  (c) Copyright 2010,2013,2014 Ralf Brown				*/
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

#include "frcritsec.h"

/************************************************************************/
/************************************************************************/

#ifdef __486__
#  define _mm_pause() __asm__("pause" : : : "memory") ;
#else
#  define _mm_pause()
#endif

/************************************************************************/
/************************************************************************/

#ifdef FrMULTITHREAD
#if __GNUC__ >= 4 && !defined(HELGRIND)
void FrCriticalSection::backoff_acquire()
{
   s_collisions++ ;
   if (!__sync_lock_test_and_set(&m_mutex,true))
      return ;
   for (size_t i = 0 ; i < 50 ; i++)
      {
      _mm_pause() ;
      if (!__sync_lock_test_and_set(&m_mutex,true))
	 return ;
      s_collisions++ ;
      }
   while (__sync_lock_test_and_set(&m_mutex,true))
      {
      s_collisions++ ;
      FrThreadYield() ;
      }
   return ;
}
#endif
#endif /* FrMULTITHREAD */

// end of file frcritsec.cpp //
