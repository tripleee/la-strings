/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frmmap.h		memory-mapped files			*/
/*  LastEdit: 04nov09							*/
/*									*/
/*  (c) Copyright 1997,2000,2004,2009 Ralf Brown			*/
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

#ifndef __FRMMAP_H_INCLUDED
#define __FRMMAP_H_INCLUDED

#include "frcommon.h"

class FrFileMapping ;

enum FrMapMode { FrM_READONLY, FrM_READWRITE, FrM_COPYONWRITE } ;

enum FrMemUseAdvice { FrMADV_NORMAL, FrMADV_RANDOM, FrMADV_SEQUENTIAL } ;

FrFileMapping *FrMapFile(const char *filename, FrMapMode mode) ;
void *FrMappedAddress(const FrFileMapping *fmap) ;
size_t FrMappingSize(const FrFileMapping *fmap) ;
void FrTouchMappedMemory(FrFileMapping *fmap) ;
bool FrSyncMappedFile(FrFileMapping *fmap) ;
bool FrUnmapFile(FrFileMapping *fmap) ;

bool FrAdviseMemoryUse(void *start, size_t length, FrMemUseAdvice) ;
bool FrAdviseMemoryUse(FrFileMapping *fmap, FrMemUseAdvice) ;
bool FrWillNeedMemory(void *start, size_t length) ;
bool FrDontNeedMemory(void *start, size_t length, bool never_again=false) ;

#endif /* !__FRMMAP_H_INCLUDED */

// end of file frmmap.cpp //

