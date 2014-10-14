/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frevent.h	       event lists				*/
/*  LastEdit: 04nov09							*/
/*									*/
/*  (c) Copyright 1995,1996,1997,2001,2009 Ralf Brown			*/
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

#ifndef __FREVENT_H_INCLUDED
#define __FREVENT_H_INCLUDED

#ifndef __FRCOMMON_H_INCLUDED
#include "frcommon.h"
#endif

#ifdef unix
#include <sys/time.h>
#endif
#include <time.h>

#if defined(__GNUC__)
#  pragma interface
#endif

/************************************************************************/
/************************************************************************/

typedef time_t FrEventFunc(void *client_data) ;
typedef void FrEventCleanupFunc(void *client_data) ; // free client data

class FrEvent ;				// opaque except in frevent.cpp

class FrEventList
   {
   private:
      FrEvent *events ;
   private: // methods
      void insert(FrEvent *event) ;
   public: // methods
      FrEventList() ;
      ~FrEventList() ;

      void executeEvents() ;

      // modifiers
      FrEvent *addEvent(time_t time, FrEventFunc *f, void *client_data,
			bool delta = false,FrEventCleanupFunc *cleanup = 0) ;
      bool removeEvent(FrEvent *event) ;
      void reschedule(FrEvent *event, time_t newtime) ;
      void postpone(FrEvent *event, time_t delta) ;

      // accessors
      bool haveEvents() const { return events != 0 ; }
      FrEvent *findUserData(void *udata) const ;
      void *getUserData(FrEvent *event) const ;
   } ;

#endif /* !__FREVENT_H_INCLUDED */

// end of file frevent.h //
