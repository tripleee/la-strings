/****************************** -*- C++ -*- *****************************/
/*									*/
/*  MikroKARAT Inverse Indexer						*/
/*  Version 2.00beta							*/
/*     by Henry Robertson                                               */
/*									*/
/*  File inv.C	  	inverse indexing routines			*/
/*  LastEdit: 28oct2013 by Ralf Brown					*/
/*    (converted from streams to unbuffered I/O by Ralf, 4/95)		*/
/*    (fixes for new GCC 2.7.0 warnings, 5/96)				*/
/*    (fixes for additional pedantic GCC warnings, 4/09)		*/
/*    (converted FrNewN to FrAllocLocal where appropriate, 5/09)	*/
/*    (fixed for GCC 4.3 warnings, 8/09)				*/
/*									*/
/*  (c) Copyright 1994,1995,1996,1997,2001,2004,2006,2009,2010,2011,	*/
/*		2013 Language Techn. Inst.				*/
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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frstring.h"
#include "frfilutl.h"
#include "inv.h"

#if defined(__MSDOS__) || defined(__WATCOMC__) || defined(_MSC_VER)
#  include <io.h>
#endif /* __MSDOS__ || __WATCOMC__ || _MSC_VER */

#ifdef unix
#  include <unistd.h>
#endif /* unix */

//----------------------------------------------------------------------

// set next line to 1 to enable debugging output
#if 0
#  define DBGOUT(x) (cout << x) ;
#else
#  define DBGOUT(x)
#endif

/************************************************************************/
/*	Manifest constants						*/
/************************************************************************/

#ifdef __MSDOS__
#  define CACHE_SIZE 200
#else
#  define CACHE_SIZE 1600
#endif

#define PATCH_SIZE 10

/************************************************************************/
/*	Global data for this module					*/
/************************************************************************/

static const char *stop_words[] =
			   { "a", "an", "the", "and", "or", "is", "in", "as", "on", "of", "at",
			   "to", "it", "for", "up", "from",
			   } ;

static int shift_counts[16] = { 0, 16, 4, 12, 8, 1, 5, 13, 17, 2, 6, 10, 14, 3, 7, 11 } ;

/************************************************************************/
/*	External and forward declarations				*/
/************************************************************************/

#ifdef CMU
extern "C" int open(char *path, int, mode_t);
extern "C" char* strtok(char*, const char*);
#endif /* CMU */

static FrList* framelist(FrFrame*, int);
static int df(long int, char*, int fd, long int, long int);
static long int add_patch(long int, int fd);
static int del_name(long int, long int &, long int &);
static int check_last(long int &);
static long int hash_value(char*);
static int add_name(long int, int fd, long int);
static int add_node(long int, char*, int fd, long int &);
static void af(long int, char*, int fd, long int, long int);


/************************************************************************/
/*	Types for this module						*/
/************************************************************************/

typedef char LONGbuf[4] ;

struct patch {
   LONGbuf ids[PATCH_SIZE] ;
   LONGbuf offset_next ;
   };

//----------------------------------------------------------------------

struct DiskIndexNode
   {
   LONGbuf hash, left, right, first, last, n;
   void *operator new(size_t size) { return FrMalloc(size) ; }
   void operator delete(void *blk) { FrFree(blk) ; }
   } ;

//----------------------------------------------------------------------

class CacheTable
   {
   protected:
      CacheTable *next, *prev ;
      long int address ;
      int fd ;
      bool dirty ;
   protected:  // methods
      virtual ~CacheTable() {}
      virtual int flush() ;
      friend class IndexCache ;
      virtual int objsize() {return 0;}
   public:
      void *operator new(size_t size) { return FrMalloc(size) ; }
      void operator delete(void *blk) { FrFree(blk) ; }
   } ;

//----------------------------------------------------------------------

class NodeTable : public CacheTable
   {
   private:
      static FrAllocator allocator ;
   public:
      char *frname;
      long int hash, left, right, first, last, n ;
   public:  // methods
      void *operator new(size_t) { return allocator.allocate() ; }
      void operator delete(void *blk,size_t) { allocator.release(blk) ; }
      NodeTable(int fd = -1) ;
      NodeTable(int fd, char *name) ;
      NodeTable(int fd, long int addr, DiskIndexNode *node, char *name) ;
      virtual ~NodeTable() ;
      virtual int flush() ;
      virtual int objsize() {return sizeof(DiskIndexNode)+(int)n+1;}
      char *name() const { return frname ; }
      friend class NodeCache ;
      static void init() ;
   } ;

//----------------------------------------------------------------------

class PatchTable : public CacheTable
   {
   private:
      static FrAllocator allocator ;
   public:
      LONGbuf ids[PATCH_SIZE] ;
      LONGbuf offset_next ;
   public:  // methods
      void *operator new(size_t) { return allocator.allocate() ; }
      void operator delete(void *blk,size_t) { allocator.release(blk) ; }
      PatchTable(int fd = -1) ;
      PatchTable(int fd, long int addr, patch *pat) ;
      virtual ~PatchTable() ;
      virtual int flush() ;
      virtual int objsize() {return sizeof(patch);}
      friend class PatchCache ;
      static void init() ;
   } ;

//----------------------------------------------------------------------

class IndexCache
   {
   protected:
      CacheTable *first, *last ;
   public:
      long int *filesize;
   protected:
      int max_size, cur_size ;
      int fd ;
   protected:  // methods
      void add(CacheTable *item) ;
      IndexCache()
	    { first = last = 0 ; cur_size = max_size = 0 ; filesize = 0; }
   public:  // methods
      IndexCache(int fd, int maxsize = CACHE_SIZE) ;
      ~IndexCache() ;
      CacheTable *find(long int offset) ;
      int write(CacheTable *elt, long int offset) ;
      int write(CacheTable *elt) { return write(elt,elt->address) ; }
      void *operator new(size_t size) { return FrMalloc(size) ; }
      void operator delete(void *blk) { FrFree(blk) ; }
   } ;

//----------------------------------------------------------------------

class NodeCache : public IndexCache
   {
   public:
      NodeCache(int fd, long int &fsize, int maxsize = CACHE_SIZE) ;
      NodeTable *read(long int offset) ;
   } ;

//----------------------------------------------------------------------

class PatchCache : public IndexCache
   {
   public:
      PatchCache(int fd, long int &fsize, int maxsize = CACHE_SIZE) ;
      PatchTable *read(long int offset) ;
   } ;

//----------------------------------------------------------------------

class StreamTable
   {
   private:
      StreamTable *next ;
      NodeCache *cache ;
      PatchCache *patches ;
      long int filesize;
      int fd ;
   public:
      StreamTable(int fd, int max_size) ;
      ~StreamTable() ;
      NodeCache *find(int fd) ;
      PatchCache *findpatch(int fd) ;
      StreamTable *add(int fd, int max_size) ;
      StreamTable *remove(int fd) ;
      void *operator new(size_t size) { return FrMalloc(size) ; }
      void operator delete(void *blk) { FrFree(blk) ; }
   } ;

/************************************************************************/
/*	Global variables limited to this file				*/
/************************************************************************/

static bool Initialized = false ;
static FrList *stoplist ;

static FrMemoryPool *NodeName_Pool ;

static StreamTable *streams = 0 ;
static NodeCache *curr_cache ;
static PatchCache *curr_patches ;

/************************************************************************/
/*    Global variables for class NodeTable				*/
/************************************************************************/

FrAllocator NodeTable::allocator("NodeTable",sizeof(NodeTable)) ;

/************************************************************************/
/*    Global variables for class PatchTable				*/
/************************************************************************/

FrAllocator PatchTable::allocator("PatchTable",sizeof(PatchTable)) ;

/************************************************************************/
/*	Methods for class CacheTable					*/
/************************************************************************/

int CacheTable::flush()
{
   // nothing to do
   return 0 ;
}

/************************************************************************/
/*	Methods for struct NodeTable					*/
/************************************************************************/

void NodeTable::init()
{
   NodeTable *n = new NodeTable(-1) ; // force a memory block to be allocated
   delete n ;
   return ;
}

//----------------------------------------------------------------------

NodeTable::NodeTable(int filedesc)
{
   dirty = false ;
   next = prev = 0 ;
   address = 0 ;
   frname = 0 ;
   fd = filedesc ;
   hash = left = right = first = last = n = 0 ;
   return ;
}

//----------------------------------------------------------------------

NodeTable::NodeTable(int filedesc, char *nodename)
{
   dirty = false ;
   next = prev = 0 ;
   fd = filedesc ;
   address = 0 ;
   if (nodename)
      {
      n = strlen(nodename) ;
      frname = (char*)NodeName_Pool->allocate((int)(n+1)) ;
      memcpy(frname,nodename,n+1) ;
      }
   else
      {
      n = 0 ;
      frname = 0 ;
      }
   hash = left = right = first = last = 0 ;
   return ;
}

//----------------------------------------------------------------------

NodeTable::NodeTable(int filedesc, long int addr, DiskIndexNode *node,
		     char *nm)
{
   dirty = false ;
   next = prev = 0 ;
   fd = filedesc ;
   address = addr ;
   int nmlen = strlen(nm) + 1 ;
   frname = (char*)NodeName_Pool->allocate(nmlen) ;
   memcpy(frname,nm,nmlen) ;
   hash = FrLoadLong(node->hash) ;
   left = FrLoadLong(node->left) ;
   right = FrLoadLong(node->right) ;
   first = FrLoadLong(node->first) ;
   last = FrLoadLong(node->last) ;
   n = FrLoadLong(node->n) ;
   return ;
}

//----------------------------------------------------------------------

NodeTable::~NodeTable()
{
   if (dirty)
      flush() ;
   NodeName_Pool->release(frname) ;
   return ;
}

//----------------------------------------------------------------------

int NodeTable::flush()
{
   DiskIndexNode node;

   // convert to on-disk version of node
   FrStoreLong(hash,node.hash);
   FrStoreLong(left,node.left);
   FrStoreLong(right,node.right);
   FrStoreLong(first,node.first);
   FrStoreLong(last,node.last);
   FrStoreLong(n,node.n);
   // write node to disk
   DBGOUT("writing to fd " << fd << " at " << address << endl) ;
   lseek(fd,address,SEEK_SET) ;
   if (!Fr_write(fd,(char*)&node,sizeof(DiskIndexNode)))
      return -1 ;
   int result = 0 ;
   if (frname)
      {
      if (!Fr_write(fd,frname,(int)(n+1)))
	 result = -1 ;
      }
   else
      {
      char temp[1] = { '\0' } ;
      if (!Fr_write(fd,temp,1))
	 result = -1 ;
      }
   if (!result)
      dirty = false ;
   return result ;
}

/************************************************************************/
/*	Methods for class IndexCache					*/
/************************************************************************/

IndexCache::IndexCache(int filedesc, int maxsize)
{
   filesize = 0;
   fd = filedesc ;
   max_size = maxsize ;
   cur_size = 0 ;
   first = last = 0 ;
   return ;
}

//----------------------------------------------------------------------

IndexCache::~IndexCache()
{
   CacheTable *n, *cur ;

   while (first)
      {
      n = first->next ;
      cur = first ;
      first = n ;
      delete cur ;
      }
   return ;
}

//----------------------------------------------------------------------
// locate an element in the cache (if present), and make it the least-
// recently used element in the cache.  Returns 0 if the node at the
// indicated offset is not currently in the cache

CacheTable *IndexCache::find(long int offset)
{
   CacheTable *cur = first ;

   while (cur && cur->address != offset)
      cur = cur->next ;
   if (cur && cur != first)
      {
      // make this cache element the least-recently used one
      if (cur == last)
	 last = cur->prev ;
      CacheTable *n = cur->next ;
      CacheTable *p = cur->prev ;
      if (n) n->prev = p ;
      if (p) p->next = n ;
      cur->prev = 0 ;
      cur->next = first ;
      first -> prev = cur;
      first = cur ;
      }
   return cur ;
}

//----------------------------------------------------------------------

void IndexCache::add(CacheTable *n)
{
   if (cur_size >= max_size)
      {
      // remove the least-recently-used element of the cache
      CacheTable *elt = last ;
      last = last->prev ;
      last->next = 0 ;		// no elements after new 'last' element
      delete elt ;
      }
   // and link it into the cache's list as the least-recently-use element
   n->next = first ;
   n->prev = 0 ;
   if (first) first->prev = n ;
   if (!last) last = n ;
   first = n ;
   cur_size++ ;
   return ;
}

//----------------------------------------------------------------------

int IndexCache::write(CacheTable *elt, long int offset)
{
   elt->address = offset;
   elt->dirty = true ;
   long int new_end = offset + elt->objsize() ;
   if (new_end > *filesize)
      *filesize = new_end ;
   if (!find(offset))
      {
      add(elt);		// not yet in cache, so put it there
      //      return elt->flush() ;    // and make sure it exists on disk
      }
   //   else
   return 0 ;  // successful
}

/************************************************************************/
/*	Methods for class NodeCache					*/
/************************************************************************/

NodeCache::NodeCache(int filedesc, long int &fsize, int maxsize)
{
   filesize = &fsize;
   fd = filedesc ;
   max_size = maxsize ;
   return ;
}

//----------------------------------------------------------------------

NodeTable *NodeCache::read(long int offset)
{
   NodeTable *n = (NodeTable*)find(offset) ;
   DiskIndexNode temp;

   if (!n)
      {
      // read the node from disk and add it to the cache
      lseek(fd,offset,SEEK_SET) ;
      int size = (int)sizeof(DiskIndexNode) ;
      if (::read(fd,(char*)&temp,size) < size)
	 return 0 ;
      int len = (int)FrLoadLong(temp.n) ;
      FrLocalAlloc(char,name,1024,len+1);
      if (::read(fd,name,len) < len)
	 {
	 FrLocalFree(name) ;
	 return 0 ;
	 }
      name[len] = '\0' ;
      n = new NodeTable(fd,offset,&temp,name);
      add(n);
      FrLocalFree(name) ;
      }
   DBGOUT("Read " << n << endl) ;
   return n ;
}

/************************************************************************/
/*	Methods for struct PatchTable					*/
/************************************************************************/

void PatchTable::init()
{
   PatchTable *p = new PatchTable(-1) ; // force a memory block to be allocated
   delete p ;
   return ;
}

//----------------------------------------------------------------------

PatchTable::PatchTable(int filedesc)
{
   dirty = false ;
   next = prev = 0 ;
   address = 0 ;
   fd = filedesc ;
   for (int i = 0 ; i < PATCH_SIZE ; i++)
      FrStoreLong(-1,ids[i]) ;
   FrStoreLong(0,offset_next) ;
   return ;
}

//----------------------------------------------------------------------

PatchTable::PatchTable(int filedesc, long int addr, patch *pat)
{
   dirty = false ;
   next = prev = 0 ;
   fd = filedesc ;
   address = addr ;
   memcpy(ids,pat->ids,sizeof(ids)) ;
   memcpy(offset_next,pat->offset_next,sizeof(offset_next));
   return ;
}

//----------------------------------------------------------------------

PatchTable::~PatchTable()
{
   if (dirty)
      flush() ;
   return ;
}

//----------------------------------------------------------------------

int PatchTable::flush()
{
   // write patch to disk
   DBGOUT("writing to fd " << fd << " at " << address << endl) ;
   lseek(fd,address,SEEK_SET) ;
   int result = 0 ;
   char buf[sizeof(ids)+sizeof(LONGbuf)] ;
   memcpy(buf,ids,sizeof(ids)) ;
   memcpy(buf+sizeof(ids),offset_next,(int)sizeof(LONGbuf)) ;
   if (!Fr_write(fd,(char*)buf,sizeof(buf)))
      result = -1 ;
   if (!result)
      dirty = false ;
   return result ;
}

/************************************************************************/
/*	Methods for class PatchCache					*/
/************************************************************************/

PatchCache::PatchCache(int filedesc, long int &fsize, int maxsize)
{
   filesize = &fsize;
   fd = filedesc ;
   max_size = maxsize ;
   return ;
}

//----------------------------------------------------------------------

PatchTable *PatchCache::read(long int offset)
{
   PatchTable *p = (PatchTable*)find(offset) ;
   patch temp;

   if (!p)
      {
      // read the patch from disk and add it to the cache
      lseek(fd,offset,SEEK_SET) ;
      if (::read(fd,(char*)&temp,(int)sizeof(patch)) < (int)sizeof(patch))
	 return 0 ;
      p = new PatchTable(fd,offset,&temp);
      add(p);
      }
   DBGOUT("Read patch " << p << endl) ;
   return p ;
}

/************************************************************************/
/* 	Methods for class StreamTable					*/
/************************************************************************/

StreamTable::StreamTable(int filedesc, int max_size)
{
   fd = filedesc ;
   filesize = lseek(filedesc,0,SEEK_END) ;
   cache = new NodeCache(filedesc,filesize, max_size) ;
   patches = new PatchCache(filedesc, filesize, max_size) ;
   return ;
}

//----------------------------------------------------------------------

StreamTable::~StreamTable()
{
   delete cache ;
   delete patches ;
   return ;
}

//----------------------------------------------------------------------

NodeCache *StreamTable::find(int filedesc)
{
   const StreamTable *st = this ;

   while (st && st->fd != filedesc)
      st = st->next ;
   return st ? st->cache : 0 ;
}

//----------------------------------------------------------------------

PatchCache *StreamTable::findpatch(int filedesc)
{
   const StreamTable *st = this ;

   while (st && st->fd != filedesc)
      st = st->next ;
   return st ? st->patches : 0 ;
}

//----------------------------------------------------------------------

StreamTable *StreamTable::add(int filedesc, int max_size)
{
   if (!find(filedesc))
      {
      StreamTable *st = new StreamTable(filedesc, max_size) ;
      st->next = (StreamTable *)this ;
      return st ;
      }
   else
      return this ;
}

//----------------------------------------------------------------------

StreamTable *StreamTable::remove(int filedesc)
{
   StreamTable *st = (StreamTable*)this ;
   StreamTable *prev = 0 ;
   StreamTable *newhead ;

   while (st && st->fd != filedesc)
      {
      prev = st ;
      st = st->next ;
      }
   if (!st)		// if not found, don't change anything
      return (StreamTable*)this ;
   if (prev)
      {
      prev->next = st->next ;
      newhead = (StreamTable*)this ;
      }
   else
      newhead = st->next ;
   delete st ;
   return newhead ;
}

/************************************************************************/
/************************************************************************/

inline FrObject *read_object_from_file(int filedesc,int size)
{
   FrLocalAlloc(char,buf,1024,size+1) ;
   if (!buf)
      return 0 ;
   FrObject *o = 0 ;
   if (read(filedesc,buf,size) == size)	       // read field name
      {
      buf[size] = 0;
      const char *bufptr = buf ;
      o = string_to_FrObject(bufptr);
      }
   FrLocalFree(buf) ;
   return o ;
}

//----------------------------------------------------------------------

inline NodeTable *read_node(int filedesc, long int offset)
{
   NodeCache *cache = streams->find(filedesc) ;
   if (cache)
      {
      DBGOUT("Read offset: " << offset << endl) ;
      return cache->read(offset) ;
      }
   else
      {
      FrWarning("trying to read from unknown file descriptor");
      return 0 ;
      }
}

//----------------------------------------------------------------------

inline NodeTable *read_node(long int offset)
{
   return(curr_cache->read(offset));
}

//----------------------------------------------------------------------

inline PatchTable *read_patch(int filedesc, long int offset)
{
   PatchCache *cache = streams->findpatch(filedesc) ;
   if (cache)
      {
      DBGOUT("Read offset: " << offset << endl) ;
      return cache->read(offset) ;
      }
   else
      {
      FrWarning("trying to read from unknown file descriptor");
      return 0 ;
      }
}

//----------------------------------------------------------------------

inline PatchTable *read_patch(long int offset)
{
   return(curr_patches->read(offset));
}

//----------------------------------------------------------------------

inline int write_node(int filedesc, long int offset, NodeTable *node)
{
   NodeCache *cache = streams->find(filedesc) ;
   return cache->write(node,offset) ;
}

//----------------------------------------------------------------------

inline int write_node(long int offset, NodeTable *node)
{
   return curr_cache->write(node,offset);
}

//----------------------------------------------------------------------

inline int write_node(NodeTable *node)
{
   return curr_cache->write(node);
}

//----------------------------------------------------------------------

inline int write_patch(int filedesc, long int offset, PatchTable *pat)
{
   PatchCache *cache = streams->findpatch(filedesc) ;
   return cache->write(pat,offset) ;
}

//----------------------------------------------------------------------

inline int write_patch(long int offset, PatchTable *pat)
{
   return curr_patches->write(pat,offset);
}

//----------------------------------------------------------------------

inline int write_patch(PatchTable *pat)
{
   return curr_patches->write(pat);
}

//----------------------------------------------------------------------

inline long int seek_end(int filedesc)
{
   IndexCache *abc = streams -> find(filedesc);
   return *abc -> filesize;
}

//----------------------------------------------------------------------

inline void find_stream(int filedesc)
{
   if (streams)
      {
      curr_cache = streams->find(filedesc) ;
      curr_patches = streams->findpatch(filedesc) ;
      }
   else
      FrProgError("called update function before open_index()");
   DBGOUT("Current cashe: " << curr_cache << endl) ;
   return ;
}

//----------------------------------------------------------------------

void open_index(int filedesc, int maxsize)
{
   if (maxsize <= 0)
      maxsize = CACHE_SIZE;
   streams = streams->add(filedesc,maxsize) ;
   return ;
}

//----------------------------------------------------------------------

void close_index(int filedesc)
{
   streams = streams->remove(filedesc) ;
   return ;
}

//----------------------------------------------------------------------
// get_node_list: low-level function to get list of frame names.

static FrList* get_node_list(long int offset)
{
   FrList* result = 0;
   PatchTable *pat;
   long int id,next = offset ;
   long int newnext;

   DBGOUT("Entering get_node_list" << endl) ;
   while (next)
      {
      DBGOUT("Next patch's offset: " << next << endl) ;
      pat = read_patch(next) ;
      newnext = FrLoadLong(pat->offset_next);
      if (next == newnext)
	 {
	 FrProgError("Infinite Loop in patch!") ;
	 break;
	 }
      next = newnext;
      for(int i = 0; i < PATCH_SIZE ; i++)
	 {
	 id = FrLoadLong(pat->ids[i]);
	 if (id != -1)
	    {
	    FrSymbol *sym = symbol_for_frameID(id) ;
	    result = pushlist(sym,result);
	    DBGOUT("  Pushed " << sym << "(id = " << id << ")" << flush) ;
	    }
	 }
      }
   DBGOUT(endl << "Exiting get_node_list with " << result << endl) ;
   return (listreverse(result)) ;
}

//----------------------------------------------------------------------
// searchtree: a helping function for fetch_names which searches through
// the binary tree to find the appropriate node.
// First long int is offset (zero for the root node);
// second long int is the hash value (obtained by hash_value(FrList *keywords))

static FrList* searchtree(int filedesc, char* key, long int offset,
			   long int hash)
{
   FrList* l = 0;

   if (offset == 0) return(l);   // base cases
   if (offset == -1) offset = 0;

   DBGOUT("Current offset: " << offset << flush) ;
   NodeTable *node = read_node(offset);
   long int curhash = node->hash ;
   DBGOUT("    Current hash: " << curhash << endl) ;
   if (curhash == hash)
      {
      char *temp;
      temp = node->name() ;
      DBGOUT("read key " << temp << endl) ;
      if (!strcmp(temp,key))
	 {
	 FrList *result = 0;
	 result = get_node_list(node->first);
	 DBGOUT("exiting searchtree with " << result << endl) ;
	 return result ;
	 }
      }
   DBGOUT("left child offset: " << node->left << " right: " << node->right
	  << endl) ;
   return searchtree(filedesc,key,(curhash>hash)?node->left:node->right,hash) ;
}

//----------------------------------------------------------------------

static FrList* cutwords(FrList *words)
{
   FrObject* s;
   FrList *final = 0;
   FrList *l = words;

   while(l)
      {
      s = poplist(l);
      if (!stoplist->member(s,equal))
	 pushlist(s,final);
      else
	 free_object(s) ;
      }
   return listreverse(final);
}

//----------------------------------------------------------------------

FrList *fetch_names(int filedesc, FrList *keys, int idxtype)
{
   if (filedesc < 0)
      {
      FrProgError("invalid filedesc given to fetch_names") ;
      return 0 ;
      }
   find_stream(filedesc) ;
   FrList *ll ;
   if (idxtype == STRING_WORDS)
      {
      ll = cutwords(keys ? (FrList*)keys->deepcopy() : 0) ;
      //      free_object(keys) ;
      }
   else
      ll = keys ? (FrList*)keys->deepcopy() : 0 ;
   // get list for first key
   FrObject* e = poplist(ll);
   char *temp = e->print();
   free_object(e) ;
   DBGOUT("searching for " << temp << " with hash value " <<
	  hash_value(temp) << endl) ;
   FrList *result = searchtree(filedesc,temp,-1,hash_value(temp)) ;
   // DBGOUT("result is " << result << endl) ;
   DBGOUT("Got list for first element of list" << endl) ;
   // now get the rest of the list
   while (ll)
      {
      DBGOUT("entering while loop in fetch_names" << endl) ;
      FrObject *e2 = poplist(ll) ;
      char *item = e2->print();
      free_object(e2) ;
      FrList *result2 = searchtree(filedesc,item,-1,hash_value(item));
      FrFree(item) ;
      FrList *tl = result->intersection(result2) ;
      free_object(result);
      free_object(result2);
      result = tl;
      if (!result)
	 break ;	// no sense continuing if nothing left in results
      }
   DBGOUT("At end of while loop" << endl) ;
   free_object(ll) ;
   FrFree(temp) ;
   DBGOUT("Returning from fetch_names with " << result << endl) ;
   return result ;
}

//----------------------------------------------------------------------

FrList* inclusive_fetch(int filedesc, FrList* key, int idxtype)
{
   find_stream(filedesc) ;
   FrList *l, *ll ;
   FrList *result = 0;
   if (idxtype == STRING_WORDS)
      {
      l = cutwords(key ? (FrList*)key->deepcopy() : 0);
      //      free_object(key) ;
      }
   else
      l = key ? (FrList*)key->deepcopy() : 0 ;
   ll = l;
   while(ll)
      {
      FrObject* e = poplist(ll);
      char *temp = e->print();
      FrList *tl = result->listunion(searchtree(filedesc,temp,-1,
				                 hash_value(temp)));
      FrFree(temp) ;
      free_object(result);
      result = tl;
      }
   free_object(l) ;
   return result ;
}

//----------------------------------------------------------------------

void init_ii()
{
   if (!Initialized)
      {
      NodeName_Pool = new FrMemoryPool("DiskNode names") ;
      NodeTable::init() ;
      PatchTable::init() ;
      stoplist = 0 ;
      for (int i = 0 ; i < (int)lengthof(stop_words) ; i++)
	 pushlist(new FrString(stop_words[i]), stoplist) ;
      stoplist = listreverse(stoplist) ;
      Initialized = true ;
      }
   return ;
}

//----------------------------------------------------------------------

// top-level function to update inverse indexes
int update_inv(int idxtype, int filedesc, FrFrame* old, FrFrame* newfr)
{
   find_stream(filedesc) ;
   delete_frame(old,idxtype,filedesc);
   add_frame(newfr,idxtype,filedesc);
   return(1);
}

//----------------------------------------------------------------------

static long int nodecheck(char* slot, long int hash, NodeTable *node)
{
   long int nhash = node->hash ;
   if (nhash != hash)
      return(hash-nhash);	       // return if hash values do not match

   // Now check if fields match
   if (slot == node->name() || strcmp(slot,node->name()) == 0)
      return(0);
   else
      return(1);
}

//----------------------------------------------------------------------

int delete_frame(FrFrame *f, int idxtype, int filedesc)
{
   find_stream(filedesc) ;
   FrList *frlist = framelist(f,idxtype) ;
   while(frlist)
      {
      FrObject *element = poplist(frlist);
      char *temp = element->print();
      df(frameID_for_symbol(f->frameName()),temp,filedesc,0,sizeof(temp));
      free_object(element) ;
      FrFree(temp) ;
      }
   return(1);
}

//----------------------------------------------------------------------

// function to recursively delete all frames within a tree
static int df(long int fid, char *slot, int filedesc, long int offset,
	      long int hash)
{
   NodeTable  *node ;
   long int decision,o1,o2,nf;

   node = read_node(offset);
   // determine whether the frame is in the current node
   if (offset == 0)
      decision = 1;
   else
      decision = nodecheck(slot,hash,node);
   if (decision == 0)
      {
      // read offset of first frame name
      nf = node->first ;
      if (nf)
	 {
	 del_name(fid,node->first,node->last);
	 write_node(node) ;
	 }
      return(1);
      }

   // read the addresses of left and right subtrees
   o1 = node->left;
   o2 = node->right;
   // make the recursive calls as necessary
   if ((decision < 0) && o1)
      df(fid,slot,filedesc,o1,hash);
   else if ((decision >= 0) && o2)
      df(fid,slot,filedesc,o2,hash);
   return(1);
}

//----------------------------------------------------------------------

static bool make_string_list(const FrObject* frameinfo, va_list args)
{
   FrSymbol *slot = (FrSymbol*)((FrList*)frameinfo)->second() ;
   FrSymbol *facet = (FrSymbol*)((FrList*)frameinfo)->third() ;
   FrList **l = va_arg(args,FrList**);
   pushlist(facet, *l);
   pushlist(slot, *l);
   return true ;
}

//----------------------------------------------------------------------

// This function fetches slots and facets that will later be processed for strings
static FrList* vallist(FrFrame* f, FrList* l)
{
   FrList *result = 0;

   while (l)
      {
      FrSymbol *slot = (FrSymbol*)poplist(l) ;
      FrList *facets = (f&&slot) ? f->slotFacets(slot) : 0 ;
      while (facets)
	 {
	 const FrList *fillers = f->getFillers(slot,(FrSymbol*)poplist(facets));
	 while (fillers)
	    {
	    FrObject *obj = fillers->first() ;
	    if ((!obj || !obj->stringp() || ((FrString*)obj)->stringLength() < 20) &&
		!result->member(obj,equal))
	       pushlist(obj ? obj->deepcopy() : 0,result) ;
	    fillers = fillers->rest() ;
	    }
	 }
      }
   return listreverse(result);
}

//----------------------------------------------------------------------

// This function makes a list of key words that are in strings
static FrList* stringlist(FrFrame* f, FrList* l)
{
   const FrList *temp2 ;
   FrList *words = 0;
   FrObject* s;
   char *temp3 ;
   char* ptr ;

   while(l)
      {
      // get fillers of given slot and facet
      FrSymbol *slot = (FrSymbol*)poplist(l) ;
      FrSymbol *facet = (FrSymbol*)poplist(l) ;
      temp2 = f->getFillers(slot,facet) ;
      while (temp2)  // process fillers
	 {
	 s = temp2->first();
	 if (s && s->stringp())		// if the filler is a string
	    {
	    temp3 = s->print();
	    temp3[strlen(temp3)-1] = 0; // delete quote at end of string
	    ptr = strtok(temp3+1," ,.?");
	    while(ptr)
	       {
	       FrString *str = new FrString(ptr) ;
	       if (!words->member(str,equal))
		  {
		  pushlist(str,words);
		  }
	       else
		  free_object(str) ;
	       ptr = strtok((char *) 0, " ,.?");
	       }
	    FrFree(temp3) ;
	    }
	 temp2 = temp2->rest() ;
	 }
      }
   if (words)
      return cutwords(words);
   else
      return 0 ;
}

//----------------------------------------------------------------------

static FrList* framelist(FrFrame* f, int idxtype)
{
   if (!f)
      return 0 ;
   FrList *l = 0;
   switch(idxtype)
      {
      case FILLER_VALUES:
	 l = vallist(f,f->allSlots()) ;
	 break;
      case SLOT_NAMES:
	 l = f->allSlots() ;
	 break;
      case STRING_WORDS:
	 f->iterate(make_string_list,&l) ;
	 l = stringlist(f,l);
	 break ;
      default:
	 break ;
      }
   return l ;
}


//----------------------------------------------------------------------

int add_frame(FrFrame* f, int idxtype, int filedesc)
{
   find_stream(filedesc) ;
   FrObject *element;
   FrList *frlist = framelist(f,idxtype) ;
   while (frlist)
      {
      element = poplist(frlist);
      char *temp = element->print();
      af(frameID_for_symbol(f -> frameName()),temp,filedesc,0,
	 hash_value(temp));
      free_object(element) ;
      FrFree(temp) ;
      }
   return(1);
}

//----------------------------------------------------------------------

static void af(long int fid, char *slot, int filedesc, long int offset,
	       long int hash)
{
   NodeTable *node ;
   long int decision,o1,o2 ;

   do {
      DBGOUT("af: offset " << offset << ", fid = " << fid << endl) ;
      node = read_node(offset) ;
      // determine whether the frame is in the current node
      if (offset == 0)
	 decision = 1;
      else
	 decision = nodecheck(slot,hash,node);
      if (decision == 0)
	 {
	 DBGOUT("af: adding frame name" << endl) ;
	 // read offset of first frame name
	 long int old = node->last;
	 add_name(fid,filedesc,node->last);
	 check_last(node->last);
	 if (node->last == old)
	    return;
	 break ;
	 }

      // read the addresses of left and right subtrees
      o1 = node->left;
      o2 = node->right;
      DBGOUT("left child: " << o1 << "   right child: " << o2 << endl) ;
      // make the recursive calls as necessary
      if (decision < 0)
	 {
	 if (o1)
	    offset = o1 ;
	 else
	    {
	    add_node(fid,slot,filedesc,node->left);
	    break ;
	    }
	 }
      else // decision > 0
	 {
	 if (o2)
	    offset = o2 ;
	 else
	    {
	    add_node(fid,slot,filedesc,node->right);
	    break ;
	    }
	 }
      } while (offset) ;
   write_node(node) ;
   return ;
}

//----------------------------------------------------------------------

int make_new_file(int filedesc)
{
   NodeTable *node = new NodeTable(filedesc) ;

   write_node(filedesc,0,node) ;
   return(1);
}

//----------------------------------------------------------------------

static int add_node(long int fid, char *slot, int filedesc,
		    long int &prevoffset)
{
   long int offset = seek_end(filedesc) ;
   NodeTable *node = new NodeTable(filedesc,slot) ;

   DBGOUT("Entering add_node, file size = " << offset << endl) ;
   node->hash = hash_value(slot);     // record hash value
   node->left = node->right = 0;    // record null pointers for children
   // record values for frame names
   node->first = node->last = add_patch(fid,filedesc) ;
   write_node(filedesc,seek_end(filedesc),node) ;
   DBGOUT("patch pointer (last) is " << node->last << endl) ;
   if (prevoffset != -1)  // If this is not a new file
      {
      DBGOUT("Recording new offset " << offset << " in node at "
	     << prevoffset << endl) ;
      prevoffset = offset + sizeof(patch);
      }
   DBGOUT("Leaving add_node" << endl) ;
   return 1 ;
}

//----------------------------------------------------------------------

static long int add_patch(long int fid, int filedesc)
{
   long int offset;
   PatchTable *pat = new PatchTable(filedesc) ;
   int i;

   DBGOUT("Entering add_patch." << endl) ;
   offset = seek_end(filedesc) ;
   DBGOUT("Writing value " << fid << " at offset " << offset << endl) ;
   FrStoreLong(fid,pat->ids[0]);
   for (i = 1 ; i < PATCH_SIZE ; i++)
      FrStoreLong(-1,pat->ids[i]); // record -1 for rest of 15 frame codes
   FrStoreLong(0,pat->offset_next);	// next offset
   write_patch(offset,pat) ;
   DBGOUT("Leaving add_patch, new patch at " << offset << endl) ;
   return offset ;
}


//----------------------------------------------------------------------

static int add_name(long int fid, int filedesc, long int patch_offset)
{
   long int fn,offset;
   PatchTable *pat ;
   int i,patchloc = 0;

   DBGOUT("offset of current patch: " << patch_offset << flush) ;
   pat = read_patch(patch_offset) ;
   offset = FrLoadLong(pat->offset_next);
   DBGOUT("   next patch: " << offset << endl) ;
   bool found = false ;
   for(i = 0; i < PATCH_SIZE ; i++)
      {
      fn = FrLoadLong(pat->ids[i]);
      DBGOUT(fn << ", ") ;
      if (fn == -1)
	 {
	 found = true ;
	 break ;		// quit searching, we found a free spot
	 }
      patchloc += sizeof(LONGbuf) ;
      }
   DBGOUT("  i = " << i << endl) ;
   if (found)
      {
      DBGOUT("Modifying with " << fid) ;
      FrStoreLong(fid,pat->ids[i]) ;
      }
   else if (offset != 0)
      {
      DBGOUT( "recursing to next patch in add_name at " << offset << endl) ;
      add_name(fid,filedesc,offset);
      return(1);
      }
   else
      {
      long int new_patch_offset = add_patch(fid,filedesc); // make a new patch
      DBGOUT("Made patch at " << new_patch_offset << endl) ;
      FrStoreLong(new_patch_offset,pat->offset_next) ;
      }
   write_patch(pat) ;
   return(1);
}

//----------------------------------------------------------------------

static int del_name(long int fid, long int &patch_offset,
		    long int &last_offset)
{
   long int next;

   do {
      PatchTable *pat = read_patch(patch_offset);
      next = FrLoadLong(pat->offset_next);
      // find frame name
      for(int i = 0; i < PATCH_SIZE ; i++)
	 {
	 long int data = FrLoadLong(pat->ids[i]);
	 if (fid == data)
	    {
	    FrStoreLong(-1,pat->ids[i]) ;
	    write_patch(pat) ;
	    last_offset = patch_offset ;	// update 'last' pointer
	    return 1 ;
	    }
	 }
      patch_offset = next ;
      } while (next) ;
   return(1);
}

//----------------------------------------------------------------------

static long int cpnodes(int filedesc, int tem, long int cur)
{
   DiskIndexNode node ;
   char temp[1000];
   long int toffset,offset,x;
   patch pt;
   int i;

   if(!cur)
      return(0);		 // base case: null pointer

   if(cur == -1) cur = 0;	 // root case: set current offset to zero
   DBGOUT("Entered cp nodes with offset " << cur << endl) ;
   toffset = lseek(tem,0,SEEK_END) ;
   DBGOUT("Sought end of temp, offset " << toffset << endl) ;
   lseek(filedesc,cur,SEEK_SET) ;
   int size = (int)sizeof(DiskIndexNode) ;
   if (read(filedesc,(unsigned char *)&node,size) < size ||
       read(filedesc,temp,FrLoadLong(node.n)) < 0)
      return -1 ;
   DBGOUT("read node, LC: " << FrLoadLong(node.left)) ;
   DBGOUT(", RC: " << FrLoadLong(node.right) << endl) ;
   DBGOUT("first patch is at " << FrLoadLong(node.first) << endl) ;

   offset = FrLoadLong(node.first);
   FrStoreLong(toffset + sizeof(node) + strlen(temp),node.first);
   for (i = 0; i < 4; i++) node.last[i] = node.first[i];
   (void)Fr_write(tem,(unsigned char *)&node,sizeof(DiskIndexNode));
   (void)Fr_write(tem,temp,FrLoadLong(node.n));
   DBGOUT("wrote node." << endl) ;
   while (offset > 0)
      {
      DBGOUT("patch offset " << offset << endl) ;
      lseek(filedesc,offset,SEEK_SET) ;
      if (read(filedesc,(unsigned char *)(pt.ids),PATCH_SIZE*4) < 0 ||
	  read(filedesc,(unsigned char *)(pt.offset_next),4) < 4)
	 return -1 ;
      offset = FrLoadLong(pt.offset_next);
      if (offset)
	 {
	 x = seek_end(tem) + sizeof(PatchTable);
	 FrStoreLong(x,pt.offset_next);
	 }
      else
	 FrStoreLong(0,pt.offset_next);
      if (!Fr_write(tem,(unsigned char *)&pt,sizeof(patch)))
	 break ;
      }
   // call left child and then fix offset of left child
   FrStoreLong(cpnodes(filedesc,tem,FrLoadLong(node.left)),node.left) ;
   // call right child and then fix offset of right child
   FrStoreLong(cpnodes(filedesc,tem,FrLoadLong(node.right)),node.right);
   // finally, update the copied node on disk
   lseek(tem,toffset,SEEK_SET) ;
   (void)Fr_write(tem,(unsigned char *)&node,sizeof(DiskIndexNode));
   return toffset ;
}

//----------------------------------------------------------------------

int defragmenter(char *idxfn)
{
   const char *tempname = "xxxtemp.db" ;
   int tempfh = open(tempname,O_RDWR | O_CREAT | O_TRUNC, 0666);
   int idxfh = open(idxfn, O_RDWR,0666);
   DBGOUT("opened files." << endl) ;

   if ((tempfh == -1) || (idxfh == -1))
      {
      FrWarning("Error opening temporary file.  Defragmentation cancelled.") ;
      return(0);
      }
   open_index(tempfh,0) ;
   open_index(idxfh,0) ;
   DBGOUT("opened file streams." << endl) ;
   cpnodes(idxfh,tempfh,-1);
   FrSafelyReplaceFile(tempname,idxfn) ;
   close_index(idxfh) ;
   close_index(tempfh) ;
   close(idxfh) ;
   close(tempfh) ;
   return(1);
}

//----------------------------------------------------------------------

static int check_last(long int &last_offset)
{
   int i;
   long int next;

   long int offset = last_offset ;
   while (offset)
      {
      DBGOUT("check_last, patch offset = " << offset << endl) ;
      PatchTable *pat = read_patch(offset) ;
      next = FrLoadLong(pat->offset_next);
      if (!next)
	 break ;
      for (i = 0; i < PATCH_SIZE ; i++)
	 if ((uint32_t)FrLoadLong(pat->ids[i]) == (uint32_t)-1)
	    {
	    last_offset = offset ;
	    return 1 ;
	    }
      if (offset == next)
	 {
	 FrError("Infinite Loop in check_last") ;
	 return 0 ;
	 }
      offset = next ;
      }
   if (offset)
      last_offset = offset ;
   return 1 ;
}

//----------------------------------------------------------------------

static long int hash_value(char* slot)
{
   register unsigned long sum = 0;
   register int count = 0;

   if (slot)
      while(*slot)
	 sum += (*slot++) << shift_counts[(count++)&15];
   return (unsigned int) (sum);
}

// end of file inv.C //
