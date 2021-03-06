/****************************** -*- C++ -*- *****************************/
/*									*/
/*	LangIdent: n-gram based language-identification			*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File: pstrie.C - packed simple Word-frequency trie			*/
/*  Version:  1.22				       			*/
/*  LastEdit: 15jul2013							*/
/*									*/
/*  (c) Copyright 2012,2013 Ralf Brown/CMU				*/
/*      This program is free software; you can redistribute it and/or   */
/*      modify it under the terms of the GNU General Public License as  */
/*      published by the Free Software Foundation, version 3.           */
/*                                                                      */
/*      This program is distributed in the hope that it will be         */
/*      useful, but WITHOUT ANY WARRANTY; without even the implied      */
/*      warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR         */
/*      PURPOSE.  See the GNU General Public License for more details.  */
/*                                                                      */
/*      You should have received a copy of the GNU General Public       */
/*      License (file COPYING) along with this program.  If not, see    */
/*      http://www.gnu.org/licenses/                                    */
/*                                                                      */
/************************************************************************/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include "mtrie.h"
#include "pstrie.h"
#include "wildcard.h"
#include "FramepaC.h"

using namespace std ;

/************************************************************************/
/*	Manifest Constants						*/
/************************************************************************/

// since no node will ever point at the root, we can re-use the root
//   index as the null pointer
#define NOCHILD_INDEX 0

#define PACKEDTRIE_SIGNATURE "PackedTrie\0"
#define PACKEDTRIE_FORMAT_MIN_VERSION 1 // earliest format we can read
#define PACKEDTRIE_FORMAT_VERSION 1

// reserve some space for future additions to the file format
#define PACKEDTRIE_PADBYTES_1  58

/************************************************************************/
/*	Types								*/
/************************************************************************/

class EnumerationInfo
   {
   public:
      const PackedTrie  *trie ;
      PackedTrieMatch *matches ;
      uint8_t *key ;
      const WildcardSet **alternates ;
      unsigned max_keylen ;
      unsigned max_matches ;
      mutable unsigned num_matches ;
      bool extensible ;

   public:
      EnumerationInfo(const PackedTrie *t,
		      uint8_t *k,
		      unsigned keylen,
		      unsigned match,
		      PackedTrieMatch *matchinfo,
		      const WildcardSet **alts,
		      bool ext)
	 {
	    trie = t ;
	    alternates = alts ;
	    matches = matchinfo ;
	    key = k ;
	    max_keylen = keylen ;
	    max_matches = match ;
	    num_matches = 0 ;
	    extensible = ext ;
	 }

      void setMatch(const PackedSimpleTrieNode *node, unsigned keylen) const
	 {
	 if (num_matches < max_matches)
	    {
	    matches[num_matches].setNode(node) ;
	    matches[num_matches].setKey(key,keylen) ;
	    num_matches++ ;
	    }
	 return ;
	 }
   } ;

/************************************************************************/
/*	Global variables						*/
/************************************************************************/

FrAllocator PackedTriePointer::allocator("PackedTriePtr",
					 sizeof(PackedTriePointer)) ;

/************************************************************************/
/*	Helper functions						*/
/************************************************************************/

#ifndef lengthof
#  define lengthof(x) (sizeof(x)/sizeof((x)[0]))
#endif /* lengthof */

/************************************************************************/
/*	Methods for class PackedSimpleTrieNode				*/
/************************************************************************/

PackedSimpleTrieNode::PackedSimpleTrieNode()
{
   FrStoreLong(INVALID_FREQ,m_frequency) ;
   FrStoreLong(0,m_firstchild) ;
   memset(m_children,'\0',sizeof(m_children)) ;
   return ;
}

//----------------------------------------------------------------------

unsigned PackedSimpleTrieNode::numChildren() const
{
   return (m_popcounts[lengthof(m_popcounts)-1]
	   + FrPopulationCount(FrLoadLong(m_children[lengthof(m_popcounts)-1]))) ;
}

//----------------------------------------------------------------------

bool PackedSimpleTrieNode::childPresent(unsigned int N) const 
{
   if (N >= PTRIE_CHILDREN_PER_NODE)
      return false ;
   uint32_t children = FrLoadLong(m_children[N/32]) ;
   uint32_t mask = (1U << (N % 32)) ;
   return (children & mask) != 0 ;
}

//----------------------------------------------------------------------

uint32_t PackedSimpleTrieNode::childIndex(unsigned int N) const
{
   if (N >= PTRIE_CHILDREN_PER_NODE)
      return NULL_INDEX ;
   uint32_t mask = (1U << (N % 32)) - 1 ;
   uint32_t children = FrLoadLong(m_children[N/32]) ;
   return (firstChild() + m_popcounts[N/32]
	   + FrPopulationCount(children & mask)) ;
}

//----------------------------------------------------------------------

uint32_t PackedSimpleTrieNode::childIndexIfPresent(unsigned int N) const
{
   if (N >= PTRIE_CHILDREN_PER_NODE)
      return NULL_INDEX ;
   uint32_t mask = (1U << (N % 32)) ;
   uint32_t children = FrLoadLong(m_children[N/32]) ;
   if ((children & mask) == 0)
      return NULL_INDEX ;
   mask-- ;
   return (firstChild() + m_popcounts[N/32]
	   + FrPopulationCount(children & mask)) ;
}

//----------------------------------------------------------------------

PackedSimpleTrieNode *PackedSimpleTrieNode::childNodeIfPresent(unsigned int N,
							       const PackedTrie *trie)
   const
{
   if (N >= PTRIE_CHILDREN_PER_NODE)
      return 0 ;
   uint32_t mask = (1U << (N % 32)) ;
   uint32_t children = FrLoadLong(m_children[N/32]) ;
   if ((children & mask) == 0)
      return 0 ;
   mask-- ;
   return trie->node(firstChild() + m_popcounts[N/32]
		     + FrPopulationCount(children & mask)) ;
}

//----------------------------------------------------------------------

void PackedSimpleTrieNode::setChild(unsigned N)
{
   if (N < PTRIE_CHILDREN_PER_NODE)
      {
      uint32_t mask = (1U << (N % 32)) ;
      uint32_t val = FrLoadLong(m_children[N/32]) ;
      val |= mask ;
      FrStoreLong(val,m_children[N/32]) ;
      }
   return ;
}

//----------------------------------------------------------------------

void PackedSimpleTrieNode::setPopCounts()
{
   // set up running population counts for faster lookup of children
   unsigned popcount = 0 ;
   for (size_t i = 0 ; i < lengthof(m_popcounts) ; i++)
      {
      m_popcounts[i] = (uint8_t)popcount ;
      uint32_t children = FrLoadLong(m_children[i]) ;
      popcount += FrPopulationCount(children) ;
      }
   return ;
}

//----------------------------------------------------------------------

bool PackedSimpleTrieNode::nextFrequencies(const PackedTrie *trie,
					   uint32_t *frequencies) const
{
   if (!frequencies || trie->terminalNode(this))
      return false ;
   uint32_t child = firstChild() ;
   for (size_t N = 0 ; N < PTRIE_CHILDREN_PER_NODE / 32 ; N++)
      {
      uint32_t children = FrLoadLong(m_children[N]) ;
      unsigned i = 32 ;
      while (children)
	 {
	 if ((children & 1) != 0)
	    {
	    *frequencies++ = trie->node(child++)->frequency() ;
	    }
	 else
	    *frequencies++ = 0 ;
	 i-- ;
	 children >>= 1 ;
	 }
      memset(frequencies,'\0',i*sizeof(frequencies[0])) ;
      frequencies += i ;
      }
   return (child != firstChild()) ;
}

//----------------------------------------------------------------------

// although this function breaks data abstraction by knowing about how ZipRec
//   computes its ngram scores, it saves a fair amount of compute time by
//   avoiding a second pass over the 256-element array of frequencies that
//   one gets with nextFrequencies()
// PRECOND: 'this' must not be a terminal node
bool PackedSimpleTrieNode::addToScores(const PackedTrie *trie,
				       float *scores, double weight) const
{
//   if (trie->terminalNode(this))
//      return false ;
   uint32_t child = firstChild() ;
   for (size_t N = 0 ; N < PTRIE_CHILDREN_PER_NODE / 32 ; N++)
      {
      uint32_t children = FrLoadLong(m_children[N]) ;
      unsigned i = 32 ;
      while (children)
	 {
	 if ((children & 1) != 0)
	    {
	    double f = trie->node(child)->frequency() ;
	    *scores += (float)(weight * f) ;
	    child++ ;
	    }
	 scores++ ;
	 i-- ;
	 children >>= 1 ;
	 }
      scores += i ;
      }
   return (child != firstChild()) ;
}

//----------------------------------------------------------------------

// although this function breaks data abstraction by knowing about how ZipRec
//   computes its ngram scores, it saves a fair amount of compute time by
//   avoiding a second pass over the 256-element array of frequencies that
//   one gets with nextFrequencies()
// PRECOND: 'this' must not be a terminal node
bool PackedSimpleTrieNode::addToScores(const PackedTrie *trie,
				       double *scores, double weight) const
{
//   if (trie->terminalNode(this))
//      return false ;
   uint32_t child = firstChild() ;
   for (size_t N = 0 ; N < PTRIE_CHILDREN_PER_NODE / 32 ; N++)
      {
      uint32_t children = FrLoadLong(m_children[N]) ;
      unsigned i = 32 ;
      while (children)
	 {
	 if ((children & 1) != 0)
	    {
	    double f = trie->node(child)->frequency() ;
	    *scores += weight * f ;
	    child++ ;
	    }
	 scores++ ;
	 i-- ;
	 children >>= 1 ;
	 }
      scores += i ;
      }
   return (child != firstChild()) ;
}

//----------------------------------------------------------------------

unsigned PackedSimpleTrieNode::countMatches(const PackedTrie *trie,
					    const uint8_t *key,
					    unsigned keylen,
					    const WildcardSet **alternatives,
					    unsigned max_matches,
					    bool nonterminals_only) const
{
   if (keylen == 0)			// if we get to the end of the key,
      {					//   we successfully matched
      return (!nonterminals_only || !trie->terminalNode(this)) ? 1 : 0 ;
      }
   if (trie->terminalNode(this))
      return 0 ;
   if (alternatives[0] && alternatives[0]->setSize() > 0)
      {
      // we have a wildcard, so scan all the possibilities
      unsigned matches = 0 ;
      uint32_t child = firstChild() ;
      for (size_t N = 0 ; N < PTRIE_CHILDREN_PER_NODE ; N += 32)
	 {
	 uint32_t children = FrLoadLong(m_children[N/32]) ;
	 for (size_t i = 0 ; children ; i++)
	    {
	    if ((children & 1) != 0)
	       {
	       if (alternatives[0]->contains(N+i))
		  {
		  PackedSimpleTrieNode *childnode = trie->node(child) ;
		  if (!childnode)
		     continue ;
		  matches += childnode->countMatches(trie,key+1,keylen-1,
						     alternatives+1,
						     max_matches - matches,
						     nonterminals_only) ;
		  if (matches > max_matches)
		     return matches ;
		  }
	       child++ ;
	       }
	    children >>= 1 ;
	    }
	 }
      return matches ;
      }
   else
      {
      // the current key byte must match
      PackedSimpleTrieNode *childnode = childNodeIfPresent(*key,trie) ;
      if (childnode)
	 return childnode->countMatches(trie,key+1,keylen-1,
					alternatives+1,
					max_matches,
					nonterminals_only) ;
      return 0 ;
      }
}

//----------------------------------------------------------------------

bool PackedSimpleTrieNode::enumerateMatches(const PackedTrie *trie,
					    uint8_t *keybuf,
					    unsigned max_keylength,
					    unsigned curr_keylength,
					    const WildcardSet **alternatives,
					    PackedSimpleTrieMatchFn *fn,
					    void *user_data) const
{
   if (curr_keylength >= max_keylength)
      return fn(keybuf,curr_keylength,trie,this,user_data) ;
   else if (trie->terminalNode(this))
      return true ;
   if (alternatives[curr_keylength] &&
       alternatives[curr_keylength]->setSize() > 0)
      {
      // we have a wildcard, so enumerate the possibilities
      uint32_t child = firstChild() ;
      for (size_t N = 0 ; N < PTRIE_CHILDREN_PER_NODE ; N += 32)
	 {
	 uint32_t children = FrLoadLong(m_children[N/32]) ;
	 for (size_t i = 0 ; children ; i++)
	    {
	    if ((children & 1) != 0)
	       {
	       if (alternatives[curr_keylength]->contains(N+i))
		  {
		  PackedSimpleTrieNode *childnode = trie->node(child) ;
		  keybuf[curr_keylength] = (uint8_t)(N + i) ;
		  if (!childnode->enumerateMatches(trie,keybuf,max_keylength,
						   curr_keylength+1,alternatives,
						   fn,user_data))
		     return false ;
		  }
	       child++ ;
	       }
	    children >>= 1 ;
	    }
	 }
      }
   else
      {
      // the current key byte must match
      PackedSimpleTrieNode *childnode
	 = childNodeIfPresent(keybuf[curr_keylength],trie) ;
      if (childnode &&
	  !childnode->enumerateMatches(trie,keybuf,max_keylength,
				       curr_keylength+1,alternatives,
				       fn,user_data))
	 {
	 return false ;
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

bool PackedSimpleTrieNode::enumerateChildren(const PackedTrie *trie,
					     uint8_t *keybuf,
					     unsigned max_keylength_bits,
					     unsigned curr_keylength_bits,
					     PackedSimpleTrieEnumFn *fn,
					     void *user_data) const
{
   if (leaf() && !fn(keybuf,curr_keylength_bits/8,frequency(),user_data))
      return false ;
   else if (trie->terminalNode(this))
      return true ;
   if (curr_keylength_bits < max_keylength_bits)
      {
      unsigned curr_bits = curr_keylength_bits + PTRIE_BITS_PER_LEVEL ;
      uint32_t child = firstChild() ;
      for (size_t N = 0 ; N < PTRIE_CHILDREN_PER_NODE ; N += 32)
	 {
	 uint32_t children = FrLoadLong(m_children[N/32]) ;
	 for (size_t i = 0 ; children ; i++)
	    {
	    if ((children & 1) != 0)
	       {
	       PackedSimpleTrieNode *childnode = trie->node(child++) ;
	       if (childnode)
		  {
		  unsigned byte = curr_keylength_bits / 8 ;
		  keybuf[byte] = i ;
		  if (!childnode->enumerateChildren(trie,keybuf,
						    max_keylength_bits,
						    curr_bits,fn,user_data))
		     return false ;
		  }
	       }
	    children >>= 1 ;
	    }
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

unsigned PackedSimpleTrieNode::enumerateMatches(const EnumerationInfo *info,
						unsigned keylen) const
{
   const PackedTrie *trie = info->trie ;
   if (keylen >= info->max_keylen)	// if we get to the end of the key,
      {					//   we successfully matched
      if (!leaf())
	 return 0 ;
      if (info->extensible && (trie->terminalNode(this) || frequency() == 0))
	 return 0 ;
      info->setMatch(this,keylen) ;
      return 1 ;
      }
   if (trie->terminalNode(this))
      return 0 ; // no extension possible, so not a match
   const WildcardSet *alternative = info->alternates[keylen] ;
   if (alternative && alternative->setSize() > 0)
      {
      // we have a wildcard, so scan all the possibilities
      unsigned count = 0 ;
      uint32_t child = firstChild() ;
      if (trie->isTerminalNode(child))
	 {
	 for (size_t N = 0 ; N < PTRIE_CHILDREN_PER_NODE ; N += 32)
	    {
	    uint32_t children = FrLoadLong(m_children[N/32]) ;
	    for (size_t i = 0 ; children ; i++)
	       {
	       if ((children & 1) != 0)
		  {
		  if (alternative->contains(N+i))
		     {
		     PackedSimpleTrieNode *childnode = trie->getTerminalNode(child) ;
		     info->key[keylen] = (uint8_t)(N+i) ;
		     count += childnode->enumerateMatches(info,keylen+1) ;
		     if (count > info->max_matches)
			return count ;
		     }
		  child++ ;
		  }
	       children >>= 1 ;
	       }
	    }
	 }
      else
	 {
	 for (size_t N = 0 ; N < PTRIE_CHILDREN_PER_NODE ; N += 32)
	    {
	    uint32_t children = FrLoadLong(m_children[N/32]) ;
	    for (size_t i = 0 ; children ; i++)
	       {
	       if ((children & 1) != 0)
		  {
		  if (alternative->contains(N+i))
		     {
		     PackedSimpleTrieNode *childnode = trie->getFullNode(child) ;
		     info->key[keylen] = (uint8_t)(N+i) ;
		     count += childnode->enumerateMatches(info,keylen+1) ;
		     if (count > info->max_matches)
			return count ;
		     }
		  child++ ;
		  }
	       children >>= 1 ;
	       }
	    }
	 }
      return count ;
      }
   else
      {
      // the current key byte must match
      uint8_t N = info->key[keylen] ;
      uint32_t mask = (1U << (N % 32)) ;
      uint32_t children = FrLoadLong(m_children[N/32]) ;
      if ((children & mask) != 0)
	 {
	 children &= (mask-1) ;
	 PackedSimpleTrieNode *childnode
	    = trie->node(firstChild() + m_popcounts[N/32]
			 + FrPopulationCount(children)) ;
	 return childnode->enumerateMatches(info,keylen+1) ;
	 }
      return 0 ;
      }
}

/************************************************************************/
/*	Methods for class PackedTrie					*/
/************************************************************************/

PackedTrie::PackedTrie(const NybbleTrie *trie, uint32_t min_freq,
		       bool show_conversion)
{
   init() ;
   if (trie)
      {
      m_size = trie->numFullNodes(min_freq) ;
      m_numterminals = trie->numTerminalNodes(min_freq) ;
      m_nodes = FrNewN(PackedSimpleTrieNode,m_size) ;
      m_terminals = FrNewN(PackedTrieTerminalNode,m_numterminals) ;
      if (m_nodes && m_terminals)
	 {
	 const NybbleTrieNode *mroot = trie->rootNode() ;
	 PackedSimpleTrieNode *proot = &m_nodes[PTRIE_ROOT_INDEX] ;
	 new (proot) PackedSimpleTrieNode ;
	 m_used = 1 ;
	 if (!insertChildren(proot,trie,mroot,PTRIE_ROOT_INDEX,0,min_freq))
	    {
	    m_size = 0 ;
	    m_numterminals = 0 ;
	    }
	 if (show_conversion)
	    {
	    cout << "   converted " << m_used << " full nodes and "
		 << m_termused << " terminals" << endl ;
	    }
	 m_size = m_used ;
	 m_numterminals = m_termused ;
	 }
      else
	 {
	 FrFree(m_nodes) ;
	 FrFree(m_terminals) ;
	 m_nodes = 0 ; 
	 m_terminals = 0 ;
	 m_size = 0 ;
	 m_numterminals = 0 ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

PackedTrie::PackedTrie(FILE *fp, const char *filename)
{
   init() ;
   if (fp && parseHeader(fp))
      {
      size_t offset = ftell(fp) ;
      FrFileMapping *fmap = FrMapFile(filename,FrM_READONLY) ;
      if (fmap)
	 {
	 // we can memory-map the file, so just point our member variables
	 //   at the mapped data
	 m_fmap = fmap ;
	 m_nodes = (PackedSimpleTrieNode*)((char*)FrMappedAddress(fmap) + offset) ;
	 m_terminals = (PackedTrieTerminalNode*)(m_nodes + m_size) ;
	 }
      else
	 {
	 // unable to memory-map the file, so read its contents into buffers
	 //   and point our variables at the buffers
	 char *nodebuffer = FrNewN(char,(m_size * sizeof(PackedSimpleTrieNode)) + (m_numterminals * sizeof(PackedTrieTerminalNode))) ;
	 m_nodes = (PackedSimpleTrieNode*)nodebuffer ;
	 m_terminals = (PackedTrieTerminalNode*)(m_nodes + m_size) ;
	 m_terminals_contiguous = true ;
	 if (!m_nodes ||
	     fread(m_nodes,sizeof(PackedSimpleTrieNode),m_size,fp) != m_size ||
	     fread(m_terminals,sizeof(PackedTrieTerminalNode),m_numterminals,fp) != m_numterminals)
	    {
	    FrFree(m_nodes) ;  m_nodes = 0 ;
	    m_terminals = 0 ;
	    m_size = 0 ; 
	    m_numterminals = 0 ;
	    }
	 }
      }
   return ;
}

//----------------------------------------------------------------------

PackedTrie::~PackedTrie()
{
   if (m_fmap)
      {
      FrUnmapFile(m_fmap) ;
      }
   else
      {
      FrFree(m_nodes) ;
      if (!m_terminals_contiguous)
	 FrFree(m_terminals) ;
      }
   init() ;				// clear all of the fields
   return ;
}

//----------------------------------------------------------------------

void PackedTrie::init()
{
   m_fmap = 0 ;
   m_nodes = 0 ;
   m_terminals = 0 ;
   m_size = 0 ;
   m_used = 0 ;
   m_numterminals = 0 ;
   m_termused = 0 ;
   m_maxkeylen = 0 ;
   m_terminals_contiguous = false ;
   return ;
}

//----------------------------------------------------------------------

uint32_t PackedTrie::allocateChildNodes(unsigned numchildren)
{
   uint32_t index = m_used ;
   m_used += numchildren ;
   if (m_used > m_size)
      {
      m_used = m_size ;
cerr<<"out of full nodes"<<endl;
      return NOCHILD_INDEX ;		// error!  should never happen!
      }
   // initialize each of the new children
   for (size_t i = 0 ; i < numchildren ; i++)
      {
      new (m_nodes + index + i) PackedSimpleTrieNode ;
      }
   return index ;
}

//----------------------------------------------------------------------

uint32_t PackedTrie::allocateTerminalNodes(unsigned numchildren)
{
   uint32_t index = m_termused ;
   m_termused += numchildren ;
   if (m_termused > m_numterminals)
      {
      m_termused = m_numterminals ;
cerr<<"out of terminal nodes"<<endl;
      return NOCHILD_INDEX ;		// error!  should never happen!
      }
   // initialize each of the new children
   for (size_t i = 0 ; i < numchildren ; i++)
      {
      new (m_terminals + index + i) PackedTrieTerminalNode ;
      }
   return (index | PTRIE_TERMINAL_MASK) ;
}

//----------------------------------------------------------------------

bool PackedTrie::insertTerminals(PackedSimpleTrieNode *parent,
				 const NybbleTrie *trie,
				 const NybbleTrieNode *mnode,
				 uint32_t mnode_index,
				 unsigned keylen, uint32_t min_freq)
{
   if (!parent || !mnode)
      return false ;
   unsigned numchildren = mnode->numExtensions(trie,min_freq) ;
   if (numchildren == 0)
      return true ;
   keylen++ ;
   if (keylen > longestKey())
      m_maxkeylen = keylen ;
   uint32_t firstchild = allocateTerminalNodes(numchildren) ;
   parent->setFirstChild(firstchild) ;
   if (firstchild == NOCHILD_INDEX)
      {
      cerr << "insertTerminals: firstchild==NOCHILD_INDEX"<<endl;
      return false ;
      }
   unsigned index = 0 ;
   for (unsigned i = 0 ; i < PTRIE_CHILDREN_PER_NODE ; i++)
      {
      uint32_t nodeindex = mnode_index ;
      if (trie->extendKey(nodeindex,(uint8_t)i))
	 {
	 const NybbleTrieNode *mchild = trie->node(nodeindex) ;
	 uint32_t mfreq = mchild->frequency() ;
	 if (mfreq >= min_freq)
	    {
	    // set the appropriate bit in the child array
	    parent->setChild(i) ;
	    // add frequency info to the child node
	    PackedSimpleTrieNode *pchild = node(firstchild + index) ;
	    index++ ;
	    pchild->setFrequency(mfreq) ;
	    }
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

bool PackedTrie::insertChildren(PackedSimpleTrieNode *parent,
				const NybbleTrie *trie,
				const NybbleTrieNode *node,
				uint32_t node_index,
				unsigned keylen,
				uint32_t min_freq)
{
   if (!parent || !node)
      return false ;
   // first pass: fill in all the children
   unsigned numchildren = node->numExtensions(trie,min_freq) ;
   if (numchildren == 0)
      return true ;
   keylen++ ;
   if (keylen > longestKey())
      m_maxkeylen = keylen ;
   bool terminal = node->allChildrenAreTerminals(trie,min_freq) ;
//!!!   bool terminal = node->allChildrenAreTerminals(trie) ;
   uint32_t firstchild = (terminal
			  ? allocateTerminalNodes(numchildren)
			  : allocateChildNodes(numchildren)) ;
   parent->setFirstChild(firstchild) ;
   if (firstchild == NOCHILD_INDEX)
      {
      cerr << "insertChildren: firstchild==NOCHILD_INDEX" << endl ;
      return false ;
      }
   unsigned index = 0 ;
   for (unsigned i = 0 ; i < PTRIE_CHILDREN_PER_NODE ; i++)
      {
      uint32_t nodeindex = node_index ;
      if (trie->extendKey(nodeindex,(uint8_t)i))
	 {
	 const NybbleTrieNode *mchild = trie->node(nodeindex) ;
	 uint32_t mfreq = mchild->frequency() ;
	 if (mfreq >= min_freq)
	    {
	    // set the appropriate bit in the child array
	    parent->setChild(i) ;
	    // add frequency info to the child node
	    PackedSimpleTrieNode *pchild = this->node(firstchild + index) ;
	    index++ ;
	    pchild->setFrequency(mfreq) ;
	    if (terminal)
	       {
	       if (!insertTerminals(pchild,trie,mchild,nodeindex,keylen,
				    min_freq))
		  return false ;
	       }
	    else if (!insertChildren(pchild,trie,mchild,nodeindex,keylen,
				     min_freq))
	       return false ;
	    }
	 }
      }
   parent->setPopCounts() ;
   return true ;
}

//----------------------------------------------------------------------

bool PackedTrie::parseHeader(FILE *fp)
{
   const size_t siglen = sizeof(PACKEDTRIE_SIGNATURE) ;
   char signature[siglen] ;
   if (fread(signature,sizeof(char),siglen,fp) != siglen ||
       memcmp(signature,PACKEDTRIE_SIGNATURE,siglen) != 0)
      {
      // error: wrong file type
      return false ;
      }
   unsigned char version ;
   if (fread(&version,sizeof(char),sizeof(version),fp) != sizeof(version)
       || version < PACKEDTRIE_FORMAT_MIN_VERSION
       || version > PACKEDTRIE_FORMAT_VERSION)
      {
      // error: wrong version of data file
      return false ;
      }
   unsigned char bits ;
   if (fread(&bits,sizeof(char),sizeof(bits),fp) != sizeof(bits) ||
       bits != PTRIE_BITS_PER_LEVEL)
      {
      // error: wrong type of trie
      return false ;
      }
   LONGbuffer val_size, val_keylen, val_numterm ;
   char padbuf[PACKEDTRIE_PADBYTES_1] ;
   if (fread(val_size,sizeof(val_size),1,fp) != 1 ||
       fread(val_keylen,sizeof(val_keylen),1,fp) != 1 ||
       fread(val_numterm,sizeof(val_numterm),1,fp) != 1 ||
       fread(padbuf,1,sizeof(padbuf),fp) != sizeof(padbuf))
      {
      // error reading header
      return false ;
      }
   m_maxkeylen = FrLoadLong(val_keylen) ;
   m_size = FrLoadLong(val_size) ;
   m_numterminals = FrLoadLong(val_numterm) ;
   return true ;
}

//----------------------------------------------------------------------

PackedSimpleTrieNode *PackedTrie::findNode(const uint8_t *key,
					   unsigned keylength) const
{
   uint32_t cur_index = PTRIE_ROOT_INDEX ;
   while (keylength > 0)
      {
      if (!extendKey(cur_index,*key))
	 return 0 ;
      key++ ;
      keylength-- ;
      }
   return node(cur_index) ;
}

//----------------------------------------------------------------------

uint32_t PackedTrie::find(const uint8_t *key, unsigned keylength) const
{
   uint32_t cur_index = PTRIE_ROOT_INDEX ;
   while (keylength > 0)
      {
      if (!extendKey(cur_index,*key))
	 return 0 ;
      key++ ;
      keylength-- ;
      }
   PackedSimpleTrieNode *n = node(cur_index) ;
   return n ? n->frequency() : 0 ;
}

//----------------------------------------------------------------------

bool PackedTrie::extendKey(uint32_t &nodeindex, uint8_t keybyte) const
{
   if ((nodeindex & PTRIE_TERMINAL_MASK) != 0)
      {
      nodeindex = NULL_INDEX ;
      return false ;
      }
   PackedSimpleTrieNode *n = node(nodeindex) ;
   uint32_t index = n->childIndexIfPresent(keybyte) ;
   nodeindex = index ;
   return (index != NULL_INDEX) ;
}

//----------------------------------------------------------------------

unsigned PackedTrie::countMatches(const uint8_t *key, unsigned keylen,
				  const WildcardSet **alternatives,
				  unsigned max_matches,
				  bool nonterminals_only) const
{
   if (key && keylen > 0 && alternatives && m_nodes && m_nodes[0].firstChild())
      return m_nodes[0].countMatches(this,key,keylen,alternatives,
				     max_matches,nonterminals_only) ;
   return 0 ;
}

//----------------------------------------------------------------------

bool PackedTrie::enumerate(uint8_t *keybuf, unsigned maxkeylength,
			   PackedSimpleTrieEnumFn *fn, void *user_data) const
{
   if (keybuf && fn && m_nodes && m_nodes[0].firstChild())
      {
      memset(keybuf,'\0',maxkeylength) ;
      return m_nodes[0].enumerateChildren(this,keybuf,maxkeylength*8,0,fn,
					  user_data) ;
      }
   return false ;
}

//----------------------------------------------------------------------

bool PackedTrie::enumerate(uint8_t *keybuf, unsigned keylength,
			   const WildcardSet **alternatives,
			   PackedSimpleTrieMatchFn *fn, void *user_data) const
{
   if (keybuf && keylength > 0 && fn && alternatives &&
       m_nodes && m_nodes[0].firstChild())
      {
      return m_nodes[0].enumerateMatches(this,keybuf,keylength,0,
					 alternatives,fn,user_data) ;
      }
   return false ;
}

//----------------------------------------------------------------------

unsigned PackedTrie::enumerate(uint8_t *keybuf, unsigned keylength,
			       const WildcardSet **alternatives,
			       PackedTrieMatch *matches,
			       unsigned max_matches,
			       bool require_extensible_match) const
{
   if (keybuf && keylength > 0 && matches && alternatives &&
       m_nodes && m_nodes[0].firstChild())
      {
      EnumerationInfo info(this,keybuf,keylength,max_matches,matches,alternatives,
			   require_extensible_match) ;
      return m_nodes[0].enumerateMatches(&info,0) ;
      }
   return 0 ;
}

//----------------------------------------------------------------------

PackedTrie *PackedTrie::load(FILE *fp, const char *filename)
{
   if (fp)
      {
      PackedTrie *trie = new PackedTrie(fp,filename) ;
      if (!trie || !trie->good())
	 {
	 delete trie ;
	 return 0 ;
	 }
      return trie ;
      }
   return 0 ;
}

//----------------------------------------------------------------------

PackedTrie *PackedTrie::load(const char *filename)
{
   if (filename && *filename)
      {
      FILE *fp = fopen(filename,"rb") ;
      if (fp)
	 {
	 PackedTrie *trie = load(fp,filename) ;
	 fclose(fp) ;
	 return trie ;
	 }
      }
   return 0 ;
}

//----------------------------------------------------------------------

bool PackedTrie::writeHeader(FILE *fp) const
{
   // write the signature string
   const size_t siglen = sizeof(PACKEDTRIE_SIGNATURE) ;
   if (fwrite(PACKEDTRIE_SIGNATURE,sizeof(char),siglen,fp) != siglen)
      return false; 
   // follow with the format version number
   unsigned char version = PACKEDTRIE_FORMAT_VERSION ;
   if (fwrite(&version,sizeof(char),sizeof(version),fp) != sizeof(version))
      return false ;
   unsigned char bits = PTRIE_BITS_PER_LEVEL ;
   if (fwrite(&bits,sizeof(char),sizeof(bits),fp) != sizeof(bits))
      return false ;
   // write out the size of the trie
   LONGbuffer val_used, val_keylen, val_numterm ;
   FrStoreLong(size(),val_used) ;
   FrStoreLong(longestKey(),val_keylen) ;
   FrStoreLong(m_numterminals,val_numterm) ;
   if (fwrite(val_used,sizeof(val_used),1,fp) != 1 || 
       fwrite(val_keylen,sizeof(val_keylen),1,fp) != 1 ||
       fwrite(val_numterm,sizeof(val_numterm),1,fp) != 1)
      return false ;
   // pad the header with NULs for the unused reserved portion of the header
   for (size_t i = 0 ; i < PACKEDTRIE_PADBYTES_1 ; i++)
      {
      if (fputc('\0',fp) == EOF)
	 return false ;
      }
   return true ;
}

//----------------------------------------------------------------------

bool PackedTrie::write(FILE *fp) const
{
   if (fp)
      {
      if (writeHeader(fp))
	 {
	 // write the actual trie nodes
	 if (fwrite(m_nodes,sizeof(PackedSimpleTrieNode),m_size,fp) != m_size)
	    return false ;
	 // write the terminals
	 if (fwrite(m_terminals,sizeof(PackedTrieTerminalNode),
		    m_numterminals,fp) != m_numterminals)
	    return false ;
	 return true ;
	 }
      }
   return false ;
}

//----------------------------------------------------------------------

static bool write_multitrie(FILE *fp, void *user_data)
{
   PackedTrie *trie = (PackedTrie*)user_data ;
   return trie->write(fp) ;
}

//----------------------------------------------------------------------

bool PackedTrie::write(const char *filename) const
{
   if (filename && *filename)
      {
      return FrSafelyRewriteFile(filename,write_multitrie,(void*)this) ;
      }
   return false ;
}

//----------------------------------------------------------------------

static const char hexdigit[] = "0123456789ABCDEF" ;

static void write_escaped_char(uint8_t c, FILE *fp)
{
   switch (c)
      {
      case '\\':
	 fputs("\\\\",fp) ;
	 break ;
      case ' ':
	 fputs("\\ ",fp) ;
	 break ;
      default:
	 if (c < ' ')
	    {
	    fputc('\\',fp) ;
	    fputc(hexdigit[(c>>4)&0xF],fp) ;
	    fputc(hexdigit[c&0xF],fp) ;
	    }
	 else
	    fputc(c,fp) ;
	 break ;
      }
   return ;
}

//----------------------------------------------------------------------

static bool dump_ngram(const uint8_t *key, unsigned keylen,
		       uint32_t frequency, void *user_data)
{
   FILE *fp = (FILE*)user_data ;
   if (fp && frequency != INVALID_FREQ)
      {
      fprintf(fp,"   ") ;
      for (size_t i = 0 ; i < keylen ; i++)
	 {
	 write_escaped_char(key[i],fp) ;
	 }
      fprintf(fp," :: %lu\n",(unsigned long)frequency) ;
      }
   return true ;
}

//----------------------------------------------------------------------

bool PackedTrie::dump(FILE *fp) const
{
   FrLocalAlloc(uint8_t,keybuf,10000,longestKey()) ;
   bool success = keybuf ? enumerate(keybuf,longestKey(),dump_ngram,fp) : false ;
   FrLocalFree(keybuf) ;
   return success ;
}

/************************************************************************/
/*	Methods for class PackedTriePointer				*/
/************************************************************************/

bool PackedTriePointer::hasChildren(uint32_t node_index,
				    uint8_t keybyte) const
{
   PackedSimpleTrieNode *n = m_trie->node(node_index) ;
   return m_trie->terminalNode(n) ? false : n->childPresent(keybyte) ;
}

//----------------------------------------------------------------------

bool PackedTriePointer::hasExtension(uint8_t keybyte) const
{
   PackedSimpleTrieNode *n = m_trie->node(m_nodeindex) ;
   return m_trie->terminalNode(n) ? false : n->childPresent(keybyte) ;
}

//----------------------------------------------------------------------

bool PackedTriePointer::extendKey(uint8_t keybyte)
{
   if (m_failed)
      return false ;
   bool success = m_trie->extendKey(m_nodeindex,keybyte) ;
   if (success)
      m_keylength++ ;
   else
      m_failed = true ;
   return success ;
}

//----------------------------------------------------------------------

bool PackedTriePointer::lookupSuccessful() const
{
   PackedSimpleTrieNode *n = node() ;
   return n && n->leaf() ;
}

/************************************************************************/
/*	Additional methods for class NybbleTrie				*/
/************************************************************************/

static bool add_ngram(const uint8_t *key, unsigned keylen,
		      uint32_t frequency, void *user_data)
{
   NybbleTrie *trie = (NybbleTrie*)user_data ;
   trie->insert(key,keylen,frequency,false) ;
   return true ;
}

//----------------------------------------------------------------------

NybbleTrie::NybbleTrie(const class PackedTrie *ptrie)
{
   if (ptrie)
      {
      init(ptrie->size() * 3 / 2) ;
      FrLocalAlloc(uint8_t,keybuf,512,ptrie->longestKey()) ;
      if (keybuf)
	 {
	 ptrie->enumerate(keybuf,ptrie->longestKey(),add_ngram,this) ;
	 FrLocalFree(keybuf) ;
	 }
      }
   else
      init(1) ;
   return ;
}

// end of file ptrie.C //
