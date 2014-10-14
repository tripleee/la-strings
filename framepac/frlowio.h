/************************************************************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frlowio.h		low-level I/O function declarations	*/
/*  LastEdit: 04nov09							*/
/*									*/
/*  (c) Copyright 2001 Ralf Brown					*/
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

#ifndef __FRLOWIO_H_INCLUDED
#define __FRLOWIO_H_INCLUDED

// encapsulate some common conditional compilations in this header....


#ifdef unix
#  include <unistd.h>
#elif defined(__WATCOMC__) || defined(_MSC_VER)
#  include <io.h>
#elif defined(__MSDOS__) || defined(__WINDOWS__) || defined(__NT__)
#  include <io.h>
#endif

#endif /* !__FRLOWIO_H_INCLUDED */

// end of file frlowio.h //
