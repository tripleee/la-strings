/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 2.00							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File: frcpfile.cpp	 	file-copy utility functions		*/
/*  LastEdit: 21nov2013							*/
/*									*/
/*  (c) Copyright 2008,2009,2013 Ralf Brown				*/
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

#include "frfilutl.h"
#ifdef unix
#  include <unistd.h> // for read()
#endif

/************************************************************************/
/************************************************************************/

bool FrCopyFile(const char *srcname, FILE *destfp)
{
   if (!srcname || !*srcname)
      return false ;
   FILE *srcfp = fopen(srcname,"rb") ;
   if (srcfp)
      {
      char buffer[4*FrMAX_LINE] ;
      int count ;
      int srcfd = fileno(srcfp) ;
      while ((count = ::read(srcfd,buffer,sizeof(buffer))) > 0)
	 {
	 if (!Fr_fwrite(buffer,count,destfp))
	    break ;
	 }
      fclose(srcfp) ;
      fflush(destfp) ;
      fseek(destfp,0L,SEEK_END) ;
      return true ;
      }
   else
      return false ;
}

//----------------------------------------------------------------------

bool FrCopyFile(const char *srcname, const char *destname)
{
   if (!srcname || !*srcname || !destname || !*destname)
      return false ;
   FILE *srcfp = fopen(srcname,"rb") ;
   FILE *destfp = fopen(destname,"wb") ;
   if (srcfp && destfp)
      {
      char buffer[4*FrMAX_LINE] ;
      int count ;
      int srcfd = fileno(srcfp) ;
      int destfd = fileno(destfp) ;
      while ((count = ::read(srcfd,buffer,sizeof(buffer))) > 0)
	 {
	 if (!Fr_write(destfd,buffer,count))
	    break ;
	 }
      fclose(srcfp) ;
      fclose(destfp) ;
      return true ;
      }
   else
      {
      if (srcfp)
	 fclose(srcfp) ;
      if (destfp)
	 fclose(destfp) ;
      return false ;
      }
}

// end of file frcpfile.cpp //


