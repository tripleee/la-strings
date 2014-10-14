/************************************************************************/
/*									*/
/*  FramepaC								*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File FramepaC.h							*/
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

#ifndef __FROVRRID_H_INCLUDED
#define __FROVRRID_H_INCLUDED

template <typename<T> >
class _FrOverrideVar
   {
   private:
      T *origvar ;
      T origval ;
   public:
      _FrOverrideVar(T& var)		// preserve var, value changed elsewh.
	 { origvar = &var ; origval = var ; }
      _FrOverrideVar(T& var, T val)	// set new value, restore at end of blk
	 { origvar = &var ; origval = var ; var = val ; }
      ~_FrOverrideVar() { *origvar = origval ; }
   } ;

//----------------------------------------------------------------------

#define FrOverrideVarName(base,uniq) base ## uniq

#define FrOverrideVar(var,val) FrOverrideVar2(var,val,__LINE__)
#define FrOverrideVar2(var,val,line) \
	_FrOverrideVar<typeof(var)> FrOverrideVarName(preserve_var_,line) (var,var)
#define FrPreserveVar(var) FrPreserveVar2(var,__LINE__)
#define FrPreserveVar2(var,line) \
	_FrOverrideVar<typeof(var)> FrOverrideVarName(preserve_var_,line) (var)

#endif /* !__FROVRRID_H_INCLUDED */

// end of file frovrrid.h //
