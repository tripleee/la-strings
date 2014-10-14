/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC  -- frame manipulation in C++				*/
/*  Version 2.00beta							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File frcritsec.h		short-duration critical section mutex	*/
/*  LastEdit: 28oct2013							*/
/*									*/
/*  (c) Copyright 2010,2013 Ralf Brown					*/
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

#ifndef __FRCRITSEC_H_INCLUDED
#define __FRCRITSEC_H_INCLUDED

#include "frconfig.h"
#include "helgrind.h"

#ifdef FrMULTITHREAD
#if defined(FrCPP_ATOMICS) && 0
#  include <atomic>
#elif __GNUC__ >= 4
   // use Gnu-specific intrinsics
#else
#  include <pthread.h>
#endif
#endif /* FrMULTITHREAD */

/************************************************************************/
/************************************************************************/

void FrThreadYield() ;

/************************************************************************/
/************************************************************************/

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
// eliminate warnings about _qzz_res in Valgrind 3.6 //
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

class FrCriticalSection
   {
#ifdef FrMULTITHREAD
#if defined(FrCPP_ATOMICS) && 0
   // using C++0x atomic<T>
#elif __GNUC__ >= 4 && !defined(HELGRIND)
   // using Gnu-specific intrinsics
   private:
      static size_t s_collisions ;
      bool m_mutex ;
   protected:
      void backoff_acquire() ;
   public:
      FrCriticalSection() { m_mutex = false ; }
      ~FrCriticalSection() {}
      void acquire()
	 { ANNOTATE_HAPPENS_BEFORE(&m_mutex) ;
	   if (__sync_lock_test_and_set(&m_mutex,true))
	      backoff_acquire() ;
	 }
      void release()
	 { __sync_lock_release(&m_mutex) ;
	   ANNOTATE_HAPPENS_AFTER(&m_mutex) ; }
      static void memoryBarrier() { __sync_synchronize() ; }
      template <typename T> static T swap(T& var, const T value)
	 { return (T)__sync_lock_test_and_set(&var,value) ; }
      template <typename T> static T swap(T* var, const T value)
	 { return (T)__sync_lock_test_and_set(var,value) ; }
      template <typename T> static bool compareAndSwap(T* var, const T oldval,
						const T newval)
	 { return __sync_bool_compare_and_swap(var,oldval,newval) ; }
      template <typename T> static T increment(T& var)
	 { return (T)__sync_fetch_and_add(&var,1) ; }
      template <typename T> static T increment(T& var, const T incr)
	 { return (T)__sync_fetch_and_add(&var,incr) ; }
      template <typename T> static T decrement(T& var)
	 { return (T)__sync_fetch_and_sub(&var,1) ; }
      template <typename T> static T decrement(T& var, const T decr)
	 { return (T)__sync_fetch_and_sub(&var,decr) ; }
      // generic functionality built on top of the atomic primitives
      template <typename T> static void push(T** var, T *node)
	 {
	    T *list ;
	    do {
	       list = *var ;
	       node->next(list) ;
	    } while (!compareAndSwap(var,list,node)) ;
	 }
      template <typename T> static void push(T* var, T node)
	 {
	    T list ;
	    do {
	       list = *var ;
	       node.next(list) ;
	    } while (!__sync_bool_compare_and_swap(var->valuePtr(),list.value(),node.value())) ;
	 }
      template <typename T> static void push(T** var, T* nodes, T* tail)
	 {
	    T *list ;
	    do {
	       list = *var ;
	       tail->next(list) ;
	    } while (!compareAndSwap(var,list,nodes)) ;
	 }
      template <typename T> static T* pop(T** var)
	 {
	 T *head ;
	 T *rest ;
	 do {
	    head = *var ;
	    if (!head)
	       return head ;
	    rest = head->next() ;
	    } while (!compareAndSwap(var,head,rest)) ;
	 head->next((T*)0) ;
	 return head ;
	 }
#elif defined(__GNUC__) && defined(__386__) && !defined(HELGRIND)
   // using Gnu inline assembler
   private:
      static size_t s_collisions ;
      bool m_mutex ;
   public:
      FrCriticalSection() { m_mutex = false ; }
      ~FrCriticalSection() {}
      void acquire()
	 {
	    ANNOTATE_HAPPENS_BEFORE(&m_mutex) ;
	    __asm__("# CRITSECT ACQUIRE\n"
		    "1:\n\t"
		    "cmp $0,%0\n\t"  // wait until lock is free
		    "jz 2f\n\t"
		    "pause\n\t"	     // stall thread for an instant
		    "jmp 1b\n\t"     // before checking again
		    "2:\n\t"
		    "mov 1,%%al\n\t"  // lock is free, attempt to grab it
		    "lock; xchg %%al,%0\n\t"
		    "test %%al,%%al\n\t"
		    "jnz 1b"
		    : : "m" (m_mutex) : "%al", "memory", "cc") ;
	 }
      void release()
	 {
	    __asm__("# CRITSECT RELEASE\n\t"
		    "movb $0,%0"
		    : "=m" (m_mutex) : : ) ;
	    ANNOTATE_HAPPENS_AFTER(&m_mutex) ;
	 }
      static void memoryBarrier()
	 {
	    ANNOTATE_HAPPENS_BEFORE(&m_mutex) ;
	    __asm__("mfence" : : : "memory") ;
	    ANNOTATE_HAPPENS_AFTER(&m_mutex) ;
	 }
      template <typename T> static T swap(T& var, const T value)
	 {
	    ANNOTATE_HAPPENS_BEFORE(&m_mutex) ;
	    T orig ;
	    __asm__("# ATOMIC SWAP\n\t"
		    "lock; xchg %0,%1\n\t"
		    : "=r" (orig) : "0" (value), "m" (var) : ) ;
	    ANNOTATE_HAPPENS_AFTER(&m_mutex) ;
	    return orig ;
	 } ;
      template <typename T> static T swap(T* var, const T value)
	 {
	    ANNOTATE_HAPPENS_BEFORE(&m_mutex) ;
	    T orig ;
	    __asm__("# ATOMIC SWAP\n\t"
		    "lock; xchg %0,%1\n\t"
		    : "=0" (orig) : "r" (value), "m" (*var) : ) ;
	    ANNOTATE_HAPPENS_AFTER(&m_mutex) ;
	    return orig ;
	 } ;
      template <typename T> static bool compareAndSwap(T* var, const T oldval,
						const T newval)
	 {
	    ANNOTATE_HAPPENS_BEFORE(&m_mutex) ;
	    bool success ;
	    if (sizeof(T) == 4)
	       {
	       __asm__("# CRITSECT CAS\n"
		       "mov %0,%%eax\n\t"
		       "lock; cmpxchg %0,%1\n\t"
		       "sete al\n\t"
		       : "=a" (success) : "m" (oldval), "m" (*var), "r" (newval) : ) ;
	       }
	    else if (sizeof(T) == 8)
	       {
	       __asm__("# CRITSECT CAS\n"
		       "mov %0,%%rax\n\t"
		       "lock; cmpxchg8b %0,%1\n\t"
		       "sete al\n\t"
		       : "=a" (success) : "m" (oldval), "m" (*var), "r" (newval) : ) ;
	       }
	    else
	       abort() ;

	    bool success = __sync_bool_compare_and_swap(var,oldval,newval) ; //FIXME
	    ANNOTATE_HAPPENS_AFTER(&m_mutex) ;
	    return success ;
	 }
      template <typename T> static T increment(T& var)
	 {
	    T orig ;
	    __asm__("# ATOMIC INCR BY 1, RETURN ORIG VALUE\n\t"
		    "mov $1,%0\n\t"
		    "lock; xadd %0,%1\n\t"
		    : "=r" (orig) : "m" (var) : "cc" ) ;
	    return orig ;
	 }
      template <typename T> static T increment(T& var, const T incr)
	 {
	    T orig ;
	    __asm__("# ATOMIC INCR, RETURN ORIG VALUE\n\t"
		    "lock; xadd %0,%1\n\t"
		    : "=r" (orig) : "0" (incr), "m" (var) : "cc" ) ;
	    return orig ;
	 }
      template <typename T> static T decrement(T& var)
	 {
	    T orig ;
	    __asm__("# ATOMIC DECR BY 1, RETURN ORIG VALUE\n\t"
		    "mov $1,%0\n\t"
		    "lock; xsub %0,%1\n\t"
		    : "=r" (orig) : "m" (var) : "cc" ) ;
	    return orig ;
	 }
      template <typename T> static T decrement(T& var, const T incr)
	 {
	    T orig ;
	    __asm__("# ATOMIC DECR, RETURN ORIG VALUE\n\t"
		    "lock; xsub %0,%1\n\t"
		    : "=r" (orig) : "0" (incr), "m" (var) : "cc" ) ;
	    return orig ;
	 }
      // generic functionality built on top of the atomic primitives
      template <typename T> static void push(T** var, T *node)
	 {
	    T *list ;
	    do {
	       list = *var ;
	       node->next(list) ;
	    } while (!compareAndSwap(var,list,node)) ;
	 }
      template <typename T> static void push(T** var, T* nodes, T* tail)
	 {
	    T *list ;
	    do {
	       list = *var ;
	       tail->next(list) ;
	    } while (!compareAndSwap(var,list,nodes)) ;
	 }
      template <typename T> static T* pop(T** var)
	 {
	 T *head ;
	 T *rest ;
	 do {
	    head = *var ;
	    if (!head)
	       return head ;
	    rest = head->next() ;
	    } while (!compareAndSwap(var,head,rest)) ;
	 head->next((T*)0) ;
	 return head ;
	 }
#else
   // default version using Posix-thread mutex object
   private:
      static size_t s_collisions ;
      pthread_mutex_t m_mutex ;
   public:
      FrCriticalSection() { pthread_mutex_init(&m_mutex,0) ; }
      ~FrCriticalSection() { pthread_mutex_destroy(&m_mutex) ; }
      void acquire()
	 { 
	    ANNOTATE_HAPPENS_BEFORE(&m_mutex) ;
	    pthread_mutex_lock(&m_mutex) ; 
	 }
      void release()
	 { 
	    pthread_mutex_unlock(&m_mutex) ; 
	    ANNOTATE_HAPPENS_AFTER(&m_mutex) ;
	 }
      static void memoryBarrier() {} ;//FIXME
      template <typename T> T swap(T& var, const T value)
	 { acquire() ;
	   T old = var ;
	   var = value ;
	   release() ;
	   return old ;
	 }
      template <typename T> T swap(T* var, const T value)
	 { acquire() ;
	   T old = *var ;
	   *var = value ;
	   release() ;
	   return old ;
	 }
      template <typename T> bool compareAndSwap(T* var, const T oldval,
						const T newval)
	 { acquire() ;
	   bool match = (*var == oldval) ;
	   if (match) *var = newval ;
	   release() ;
	   return match ; }
      template <typename T> T increment(T& var)
	 { acquire() ;
	   var++ ;
	   T newval = var ;
	   release() ;
	   return newval ; }
      template <typename T> T increment(T& var, const T incr)
	 { acquire() ;
	   var += incr ;
	   T newval = var ;
	   release() ;
	   return newval ; }
      template <typename T> T decrement(T& var)
	 { acquire() ;
	   var-- ;
	   T newval = var ;
	   release() ;
	   return newval ; }
      template <typename T> T decrement(T& var, const T decr)
	 { acquire() ;
	   var -= decr ;
	   T newval = var ;
	   release() ;
	   return newval ; }
      // generic functionality built on top of the atomic primitives
      template <typename T> static void push(T** var, T *node)
	 { acquire() ;
	   node->next(*var) ;
	   *var = node ;
	   release() ;
	 }
      template <typename T> static void push(T** var, T* nodes, T* tail)
	 { acquire() ;
	   tail->next(*var) ;
	   *var = nodes ;
	   release() ;
	 }
      template <typename T> static T* pop(T** var)
	 { acquire() ;
	   T *node = *var ;
	   *var = node->next() ;
	   release() ;
	   return node ;
	 }
#endif /* __GNUC__ */

#else
   private:
      // no data members
   public:
      FrCriticalSection() {}
      ~FrCriticalSection() {}
      void acquire() {}
      void release() {}
      static void memoryBarrier() {} ;
      template <typename T> static T swap(T& var, const T value)
	 { T old = var ;
	   var = value ;
	   return old ;
	 }
      template <typename T> static T swap(T* var, const T value)
	 { T old = *var ;
	   *var = value ;
	   return old ;
	 }
      template <typename T> static bool compareAndSwap(T* var, const T oldval,
						       const T newval)
	 { bool match = (*var == oldval) ;
	   if (match) *var = newval ;
	   return match ; }
      template <typename T> static T increment(T& var)
	 { T orig = var++ ;
	   return orig ; }
      template <typename T> static T increment(T& var, const T incr)
	 { T orig = var ;
	   var += incr ;
	   return orig ; }
      template <typename T> static T decrement(T& var)
	 { T orig = var-- ;
	   return orig ; }
      template <typename T> static T decrement(T& var, const T decr)
	 { T orig = var ;
	   var -= decr ;
	   return orig ; }
      // generic functionality built on top of the atomic primitives
      template <typename T> static void push(T** var, T *node)
	 { node->next(*var) ;
	   *var = node ;
	 }
      template <typename T> static void push(T* var, T node)
	 { node.next(*var) ;
	   *var = node.value() ;
	 }
      template <typename T> static void push(T** var, T* nodes, T* tail)
	 { tail->next(*var) ;
	   *var = nodes ;
	 }
      template <typename T> static T* pop(T** var)
	 { T *node = *var ;
	   *var = node->next() ;
	   return node ;
	 }
#endif
      // single-location memory writes are always atomic on x86
      void set(bool *var) { (*var) = true ; }
      void clear(bool *var) { (*var) = false ; }
   } ;

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#  pragma GCC diagnostic pop
#endif

/************************************************************************/
/************************************************************************/

class FrScopeCriticalSection : public FrCriticalSection
   {
   public:
      FrScopeCriticalSection() { acquire() ; }
      ~FrScopeCriticalSection() { release() ; }
   } ;

#endif /* !__FRCRITSEC_H_INCLUDED */

// end of file frcritsec.h //
