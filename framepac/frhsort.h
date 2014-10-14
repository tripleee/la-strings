/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC								*/
/*  Version 1.99							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frhsort.h		templatized Smoothsort var. of Heapsort */
/*  LastEdit: 16mar10							*/
/*									*/
/*  (c) Copyright 2010 Ralf Brown					*/
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

#ifndef __FRHSORT_H_INCLUDED
#define __FRHSORT_H_INCLUDED

#include <stdint.h>
#include <stdlib.h>

/************************************************************************/
/*	Global data							*/
/************************************************************************/

extern const uint32_t Fr_leonardo_numbers[] ;

/************************************************************************/
/*	Helper class to track the sub-heap sizes			*/
/************************************************************************/

// implement the idea of keeping a shifted bitmask as shown in the
//   Wikipedia article (http://en.wikipedia.org/wiki/Smoothsort)

class FrSmoothSortHeapInfo
   {
   private:
      uint64_t m_sizes ;
      unsigned m_shift ;
   public:
      FrSmoothSortHeapInfo() { m_sizes = 1 ; m_shift = 1 ; }
      FrSmoothSortHeapInfo(const FrSmoothSortHeapInfo &orig)
	 { m_sizes = orig.m_sizes >> 1 ; m_shift = orig.m_shift + 1 ; }
      ~FrSmoothSortHeapInfo() {}

      // manipulators
      void shiftRight(unsigned N) { m_sizes >>= N ; m_shift += N ; }
      void shiftLeft(unsigned N) { m_sizes <<= N ; m_shift -= N ; }
      void shiftLeft()
	 { unsigned shft = m_shift ; if (shft > 1) shft-- ; shiftLeft(shft) ; }
      void shiftOutTrailing() { shiftRight(trailingZeros(m_sizes & ~1)) ; }
      void splitBlock() { m_sizes = (m_sizes << 2) ^ 0x07 ; m_shift -= 2 ; }
      void addBlock() { m_sizes |= 1 ; }

      // accessors
      unsigned shift() const { return m_shift ; }
      uint64_t sizes() const { return m_sizes ; }
      bool consecutive() const { return (m_sizes & 3) == 3 ; }
      bool singleBlock() const { return m_shift == 1 && m_sizes == 1 ; }

      static unsigned trailingZeros(uint64_t value) ;
   } ;

/************************************************************************/
/*	Object-array version of SmoothSort				*/
/************************************************************************/

template <class T> void FrSmoothSort_sift(T elts[], unsigned shift,
					  size_t head)
{
   T rootval = elts[head] ;
   // work our way down until we get to a size-1 subheap
   while (shift > 1)
      {
      size_t right = head - 1 ;
      size_t left = head - 1 - Fr_leonardo_numbers[shift - 2] ;
      if (T::compare(rootval,elts[left]) >= 0 &&
	  T::compare(rootval,elts[right]) >= 0)
	 break ;			// invariant is satisfied
      else if (T::compare(elts[left],elts[right]) >= 0)
	 {
	 elts[head] = elts[left] ;
	 head = left ;
	 shift-- ;
	 }
      else // (T::compare(elts[left],elts[right]) < 0)
	 {
	 elts[head] = elts[right] ;
	 head = right ;
	 shift -= 2 ;
	 }
      }
   elts[head] = rootval ;
   return ;
}

//----------------------------------------------------------------------

template <class T> void FrSmoothSort_trinkle(T elts[],
					     FrSmoothSortHeapInfo heapinfo,
					     size_t head, bool trust)
{
   T rootval = elts[head] ;
   // as long as we have multiple subheaps, fix up the sequence
   while (heapinfo.sizes() != 1)
      {
      size_t stepson = head - Fr_leonardo_numbers[heapinfo.shift()] ;
      if (T::compare(elts[stepson],rootval) <= 0)
	 break ;
      if (!trust && heapinfo.shift() > 1)
	 {
	 size_t right = head - 1 ;
	 size_t left = head - 1 - Fr_leonardo_numbers[heapinfo.shift() - 2] ;
	 if (T::compare(elts[right],elts[stepson]) >= 0 ||
	     T::compare(elts[left],elts[stepson]) >= 0)
	    break ; // we've restored the invariant
	 }
      elts[head] = elts[stepson] ;
      head = stepson ;
      heapinfo.shiftOutTrailing() ;
      trust = false ;
      }
   if (!trust)
      {
      elts[head] = rootval ;
      FrSmoothSort_sift(elts, heapinfo.shift(), head) ;
      }
   return ;
}

//----------------------------------------------------------------------

//    requires:  static int T::compare(const T&, const T&) ;
//		 static T &operator = (const T&) ;
template <class T> void FrSmoothSort(T elts[], size_t num_elts)
{
   if (num_elts < 2)
      return ;				// trivially sorted
   FrSmoothSortHeapInfo heapinfo ;
   size_t head = 0 ;
   size_t high = num_elts - 1 ;
   // first phase: heapify
   for ( ; head < high ; head++)
      {
      if (heapinfo.consecutive())
	 {
	 FrSmoothSort_sift(elts, heapinfo.shift(), head) ;
	 heapinfo.shiftRight(2) ;
	 }
      else // !consecutive
	 {
	 if (Fr_leonardo_numbers[heapinfo.shift()-1] >= high - head)
	    FrSmoothSort_trinkle(elts,heapinfo,head,false) ;
	 else
	    FrSmoothSort_sift(elts,heapinfo.shift(),head) ;
	 heapinfo.shiftLeft() ;
	 }
      heapinfo.addBlock() ;
      }
   FrSmoothSort_trinkle(elts, heapinfo, head, false) ;
   // second phase: extract largest elements while maintaining heap invariant
   for ( ; !heapinfo.singleBlock() ; head--)
      {
      if (heapinfo.shift() > 1)
	 {
	 heapinfo.splitBlock() ;
	 FrSmoothSortHeapInfo subheap(heapinfo) ;
	 FrSmoothSort_trinkle(elts, subheap,
			      head-1-Fr_leonardo_numbers[heapinfo.shift()],
			      true) ;
	 FrSmoothSort_trinkle(elts, heapinfo, head - 1, true) ;
	 }
      else
	 {
	 heapinfo.shiftOutTrailing() ;
	 }
      }
   return ;
}

/************************************************************************/
/*	Pointer-array version of SmoothSort				*/
/************************************************************************/

template <class T> void FrSmoothSort_sift(T* elts[], unsigned shift,
					  size_t head)
{
   T* rootval = elts[head] ;
   // work our way down until we get to a size-1 subheap
   while (shift > 1)
      {
      size_t right = head - 1 ;
      size_t left = head - 1 - Fr_leonardo_numbers[shift - 2] ;
      if (T::compare(rootval,elts[left]) >= 0 &&
	  T::compare(rootval,elts[right]) >= 0)
	 break ;			// invariant is satisfied
      else if (T::compare(elts[left],elts[right]) >= 0)
	 {
	 elts[head] = elts[left] ;
	 head = left ;
	 shift-- ;
	 }
      else // (T::compare(elts[left],elts[right]) < 0)
	 {
	 elts[head] = elts[right] ;
	 head = right ;
	 shift -= 2 ;
	 }
      }
   elts[head] = rootval ;
   return ;
}

//----------------------------------------------------------------------

template <class T> void FrSmoothSort_trinkle(T* elts[],
					     FrSmoothSortHeapInfo heapinfo,
					     size_t head, bool trust)
{
   T* rootval = elts[head] ;
   // as long as we have multiple subheaps, fix up the sequence
   while (heapinfo.sizes() != 1)
      {
      size_t stepson = head - Fr_leonardo_numbers[heapinfo.shift()] ;
      if (T::compare(elts[stepson],rootval) <= 0)
	 break ;
      if (!trust && heapinfo.shift() > 1)
	 {
	 size_t right = head - 1 ;
	 size_t left = head - 1 - Fr_leonardo_numbers[heapinfo.shift() - 2] ;
	 if (T::compare(elts[right],elts[stepson]) >= 0 ||
	     T::compare(elts[left],elts[stepson]) >= 0)
	    break ; // we've restored the invariant
	 }
      elts[head] = elts[stepson] ;
      head = stepson ;
      heapinfo.shiftOutTrailing() ;
      trust = false ;
      }
   if (!trust)
      {
      elts[head] = rootval ;
      sift(elts, heapinfo.shift(), head) ;
      }
   return ;
}

//----------------------------------------------------------------------

//    requires:  static int T::compare(const T*, const T*) ;
template <class T> void FrSmoothSort(T* elts[], size_t num_elts)
{
   if (num_elts < 2)
      return ;				// trivially sorted
   FrSmoothSortHeapInfo heapinfo ;
   size_t head = 0 ;
   size_t high = num_elts - 1 ;
   // first phase: heapify
   for ( ; head < high ; head++)
      {
      if (heapinfo.consecutive())
	 {
	 FrSmoothSort_sift(elts, heapinfo.shift(), head) ;
	 heapinfo.shiftRight(2) ;
	 }
      else // !consecutive
	 {
	 if (Fr_leonardo_numbers[heapinfo.shift()-1] >= high - head)
	    FrSmoothSort_trinkle(elts,heapinfo,head,false) ;
	 else
	    FrSmoothSort_sift(elts,heapinfo.shift(),head) ;
	 heapinfo.shiftLeft() ;
	 }
      heapinfo.addBlock() ;
      }
   FrSmoothSort_trinkle(elts, heapinfo, head, false) ;
   // second phase: extract largest elements while maintaining heap invariant
   for ( ; !heapinfo.singleBlock() ; head--)
      {
      if (heapinfo.shift() > 1)
	 {
	 heapinfo.splitBlock() ;
	 FrSmoothSortHeapInfo subheap(heapinfo) ;
	 FrSmoothSort_trinkle(elts, subheap,
			      head-1-Fr_leonardo_numbers[heapinfo.shift()],
			      true) ;
	 FrSmoothSort_trinkle(elts, heapinfo, head - 1, true) ;
	 }
      else
	 {
	 heapinfo.shiftOutTrailing() ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

#endif /* !__FRHSORT_H_INCLUDED */

/* end of file frhsort.h */
