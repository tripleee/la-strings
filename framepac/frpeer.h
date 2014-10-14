/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frpeer.h		 peer-to-peer network functions		*/
/*  LastEdit: 04nov09							*/
/*									*/
/*  (c) Copyright 1996,1997,2009 Ralf Brown				*/
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

#ifndef __FRPEER_H_INCLUDED
#define __FRPEER_H_INCLUDED

#include "frdatfil.h"
#include "frnetwrk.h"

/**********************************************************************/
/*      Declaration of class VFrameInfoPeer                           */
/**********************************************************************/

class VFrameInfoPeer : public VFrameInfoFile
   {
   private:
      VFrameInfoServer *vserver ;
   private: // methods

   public: // methods
      VFrameInfoPeer(const char *file, bool transactions = false,
		     bool force_create = true, const char *password = 0) ;
      virtual ~VFrameInfoPeer() ;
      virtual BackingStore backingStoreType() const ;

      //!!!
   } ;

#endif /* !__FRPEER_H_INCLUDED */

// end of file frpeer.h //
