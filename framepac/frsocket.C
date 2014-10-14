/****************************** -*- C++ -*- *****************************/
/*									*/
/*  Socket Utility Functions						*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frsocket.cpp							*/
/*  LastEdit: 04nov09							*/
/*									*/
/*  (c) Copyright 2000,2001,2006,2009 Ralf Brown			*/
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

#include <stdio.h>
#include <stdlib.h>
#include "frmswin.h"
#if defined(__WINDOWS__) || defined(__NT__) || defined(_WIN32)
#include <winsock.h>
#endif /* __WINDOWS__ || __NT__ || _WIN32 */
#include "frprintf.h"
#include "frsckstr.h"

#ifdef unix
#include <sys/socket.h>
#include <netinet/in.h>
#endif /* unix */

#ifdef __WATCOMC__
#include <io.h>
#endif /* __WATCOMC__ */

#ifdef _MSC_VER
#include <io.h>

# ifndef dup
#  define dup _dup
# endif
#endif /* _MSC_VER */

/************************************************************************/
/*	Manifest Constants						*/
/************************************************************************/

/************************************************************************/
/*	Global data for this module					*/
/************************************************************************/

/************************************************************************/
/*    Utility functions							*/
/************************************************************************/

const char *FrGetPeerName(FrSocket s)
{
   if (s == (FrSocket)INVALID_SOCKET)
      return 0 ;
   struct sockaddr_in sa ;
   int sa_size = sizeof(sa) ;
   if (getpeername(s,(sockaddr*)&sa,(socklen_t*)&sa_size) == 0)
      {
      static char peer[256] ;
      // we could get fancy and do a hostname lookup, but a dotted quad
      //   is entirely sufficient
      unsigned char *addr = (unsigned char*)&sa.sin_addr.s_addr ;
      Fr_sprintf(peer,sizeof(peer),"%u.%u.%u.%u%c",
		 addr[0],addr[1],addr[2],addr[3],0) ;
      return peer ;
      }
   return 0 ;
}

// end of file frsocket.cpp //
