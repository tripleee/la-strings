/************************************************************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 1.98.11							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frdblock.h	    multi-user database locking code		*/
/*  LastEdit: 04nov09							*/
/*									*/
/*  (c) Copyright 1995,1997 Ralf Brown					*/
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

#ifndef __FRDBLOCK_H_INCLUDED
#define __FRDBLOCK_H_INCLUDED


class FrDatabaseLock
   {
   private:

   public:
      FrDatabaseLock(const char *lockfile) ;
   } ;

#endif /* !__FRDBLOCK_H_INCLUDED */

// end of file frdblock.h //
