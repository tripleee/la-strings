/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frpeer.cpp		 peer-to-peer network functions		*/
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

#include "frpeer.h"

/************************************************************************/
/************************************************************************/

VFrameInfoPeer::VFrameInfoPeer(const char *file, bool transactions,
			       bool force_create, const char *password)
	: VFrameInfoFile(file,transactions,force_create,password)
{
   vserver = 0 ; //FIXME: nothing for now
   return ;
}

//----------------------------------------------------------------------

VFrameInfoPeer::~VFrameInfoPeer()
{
   delete vserver ;
   return ;
}

//----------------------------------------------------------------------

BackingStore VFrameInfoPeer::backingStoreType() const
{
   return BS_peer ;
}


// end of file frpeer.cpp //
