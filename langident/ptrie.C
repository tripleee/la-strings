/****************************** -*- C++ -*- *****************************/
/*									*/
/*	LangIdent: n-gram based language-identification			*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File: ptrie.C - packed Word-frequency multi-trie			*/
/*  Version:  1.20				       			*/
/*  LastEdit: 02nov2012							*/
/*									*/
/*  (c) Copyright 2011,2012 Ralf Brown/CMU				*/
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
#include "ptrie.h"
#include "FramepaC.h"

using namespace std ;

/************************************************************************/
/*	Manifest Constants						*/
/************************************************************************/

// should we use a lookup table instead of the default FrPopulationCount() ?
#define USE_POPCOUNT_TABLE

// since no node will ever point at the root, we can re-use the root
//   index as the null pointer
#define NOCHILD_INDEX 0

#define MULTITRIE_SIGNATURE "MulTrie\0"
#define MULTITRIE_FORMAT_MIN_VERSION 2 // earliest format we can read
#define MULTITRIE_FORMAT_VERSION 3

// reserve some space for future additions to the file format
#define MULTITRIE_PADBYTES_1  58

/************************************************************************/
/*	Types								*/
/************************************************************************/

/************************************************************************/
/*	Global variables						*/
/************************************************************************/

FrAllocator PackedMultiTriePointer::allocator("PackedMTriePtr",
					      sizeof(PackedMultiTriePointer)) ;

double PackedTrieFreq::s_value_map[PACKED_TRIE_NUM_VALUES] ;
bool PackedTrieFreq::s_value_map_initialized = false ;

/************************************************************************/
/*	Helper functions						*/
/************************************************************************/

#ifndef lengthof
#  define lengthof(x) (sizeof(x)/sizeof((x)[0]))
#endif /* lengthof */

//----------------------------------------------------------------------

#ifdef USE_POPCOUNT_TABLE
static uint8_t popcount16[65536] ;
static bool popcount16_initialized = false ;
#endif

static void initialize_popcount32()
{
#ifdef USE_POPCOUNT_TABLE
   if (!popcount16_initialized)
      {
      for (size_t i = 0 ; i < lengthof(popcount16) ; i++)
	 {
	 popcount16[i] = (uint8_t)FrPopulationCount(i) ;
	 }
      popcount16_initialized = true ;
      }
#  define FrPopulationCount(x) (popcount16[((x)>>16)&0xFFFF]+popcount16[(x)&0xFFFF])
#endif
   return ;
}

/************************************************************************/
/*	Methods for class PackedTrieFreq				*/
/************************************************************************/

PackedTrieFreq::PackedTrieFreq(uint32_t freq, uint32_t langID, bool last,
			       bool is_stop)
{
   uint32_t data = (langID | (last * PACKED_TRIE_LASTENTRY) |
		    (is_stop * PACKED_TRIE_STOPGRAM)) ;
   uint32_t mantissa ;
   uint32_t exponent ;
   quantize(freq,mantissa,exponent) ;
   data |= mantissa ;
   data |= (exponent << PACKED_TRIE_FREQ_EXP_SHIFT) ;
   FrStoreLong(data,m_freqinfo) ;
   return ;
}

//----------------------------------------------------------------------

void PackedTrieFreq::quantize(uint32_t freq, uint32_t &mantissa, uint32_t &exp)
{
   uint32_t e = 0 ;
   if (freq)
      {
      const uint32_t highbits = PACKED_TRIE_FREQ_HIBITS ;
      const uint32_t max_exponent
	 = PACKED_TRIE_FREQ_EXPONENT >> PACKED_TRIE_FREQ_EXP_SHIFT ;
      while ((freq & highbits) == 0 && e < max_exponent)
	 {
	 freq <<= PTRIE_EXPONENT_SCALE ;
	 e++ ;
	 }
      freq &= PACKED_TRIE_FREQ_MANTISSA ;
      if (freq == 0)
	 freq = PACKED_TRIE_MANTISSA_LSB ;
      }
   mantissa = freq ;
   exp = e ;
   return ;
}

//----------------------------------------------------------------------

PackedTrieFreq::~PackedTrieFreq()
{
   FrStoreLong(0,m_freqinfo) ;
   return ;
}

//----------------------------------------------------------------------

double PackedTrieFreq::probability(uint32_t ID) const
{
   if (ID == languageID())
      return probability() ;
   else if (!isLast())
      return next()->probability(ID) ;
   return 0 ;
}

//----------------------------------------------------------------------

void PackedTrieFreq::isLast(bool last)
{
   uint32_t data = FrLoadLong(m_freqinfo) & ~PACKED_TRIE_LASTENTRY ;
   if (last)
      data |= PACKED_TRIE_LASTENTRY ;
   FrStoreLong(data,m_freqinfo) ;
   return ;
}

//----------------------------------------------------------------------

void PackedTrieFreq::initDataMapping(double (*mapfn)(uint32_t))
{
   for (size_t i = 0 ; i < PACKED_TRIE_NUM_VALUES ; i++)
      {
      uint32_t scaled = scaledScore(i << PACKED_TRIE_VALUE_SHIFT) ;
      double mapped_value ;
      if (mapfn)
	 {
	 scaled |= (i & 1) ; // add stop-gram bit as LSB
	 mapped_value = mapfn(scaled) ;
	 }
      else
	 {
	 mapped_value = (scaled / 100.0 * TRIE_SCALE_FACTOR) ;
	 if ((i & 1) != 0)
	    mapped_value = -mapped_value ;
	 }
      s_value_map[i] = mapped_value ;
      }
   s_value_map_initialized = true ;
   return ;
}

//----------------------------------------------------------------------

bool PackedTrieFreq::writeDataMapping(FILE *fp)
{
   if (fp)
      {
      LONGbuffer count ;
      FrStoreLong(PACKED_TRIE_NUM_VALUES,count) ;
      if (!fwrite(count,sizeof(count),1,fp) == 1)
	 return false ;
      return fwrite(s_value_map,1,sizeof(s_value_map),fp) == sizeof(s_value_map) ;
      }
   return false ;
}

/************************************************************************/
/*	Methods for class PackedTrieNode				*/
/************************************************************************/

PackedTrieNode::PackedTrieNode()
{
   FrStoreLong(INVALID_FREQ,m_frequency_info) ;
   FrStoreLong(0,m_firstchild) ;
   memset(m_children,'\0',sizeof(m_children)) ;
   return ;
}

//----------------------------------------------------------------------

bool PackedTrieNode::childPresent(unsigned int N) const 
{
   if (N >= PTRIE_CHILDREN_PER_NODE)
      return false ;
   uint32_t children = FrLoadLong(m_children[N/32]) ;
   uint32_t mask = (1U << (N % 32)) ;
   return (children & mask) != 0 ;
}

//----------------------------------------------------------------------

uint32_t PackedTrieNode::childIndex(unsigned int N) const
{
   if (N >= PTRIE_CHILDREN_PER_NODE)
      return NULL_INDEX ;
   uint32_t children = FrLoadLong(m_children[N/32]) ;
   uint32_t mask = (1U << (N % 32)) - 1 ;
   children &= mask ;
   return (firstChild() + m_popcounts[N/32] + FrPopulationCount(children)) ;
}

//----------------------------------------------------------------------

uint32_t PackedTrieNode::childIndexIfPresent(uint8_t N) const
{
#if PTRIE_CHILDREN_PER_NODE < 256
   if (N >= PTRIE_CHILDREN_PER_NODE)
      return NULL_INDEX ;
#endif
   uint32_t children = FrLoadLong(m_children[N/32]) ;
   uint32_t mask = (1U << (N % 32)) ;
   if ((children & mask) == 0)
      return NULL_INDEX ;
   mask-- ;
   children &= mask ;
   return (firstChild() + m_popcounts[N/32] + FrPopulationCount(children)) ;
}

//----------------------------------------------------------------------

uint32_t PackedTrieNode::childIndexIfPresent(unsigned int N) const
{
   if (N >= PTRIE_CHILDREN_PER_NODE)
      return NULL_INDEX ;
   uint32_t children = FrLoadLong(m_children[N/32]) ;
   uint32_t mask = (1U << (N % 32)) ;
   if ((children & mask) == 0)
      return NULL_INDEX ;
   mask-- ;
   children &= mask ;
   return (firstChild() + m_popcounts[N/32] + FrPopulationCount(children)) ;
}

//----------------------------------------------------------------------

double PackedTrieNode::probability(const PackedTrieFreq *base,
				   uint32_t langID) const
{
   const PackedTrieFreq *freq = base + FrLoadLong(m_frequency_info) ;
   for ( ; ; freq++)
      {
      if (freq->languageID() == langID)
	 return freq->probability() ;
      if (freq->isLast())
	 break ;
      }
   return 0 ;
}

//----------------------------------------------------------------------

void PackedTrieNode::setChild(unsigned N)
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

void PackedTrieNode::setPopCounts()
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

bool PackedTrieNode::enumerateChildren(const PackedMultiTrie *trie,
				       uint8_t *keybuf,
				       unsigned max_keylength_bits,
				       unsigned curr_keylength_bits,
				       PackedTrieEnumFn *fn,
				       void *user_data) const
{
   if (leaf() && !fn(this,keybuf,curr_keylength_bits/8,user_data))
      return false ;
   else if (trie->terminalNode(this))
      return true ;
   if (curr_keylength_bits < max_keylength_bits)
      {
      unsigned curr_bits = curr_keylength_bits + PTRIE_BITS_PER_LEVEL ;
      for (unsigned i = 0 ; i < PTRIE_CHILDREN_PER_NODE ; i++)
	 {
	 uint32_t child = childIndexIfPresent(i) ;
	 if (child != NULL_INDEX)
	    {
	    PackedTrieNode *childnode = trie->node(child) ;
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
	 }
      }
   return true ;
}

/************************************************************************/
/*	Methods for class PackedMultiTrie				*/
/************************************************************************/

PackedMultiTrie::PackedMultiTrie(const MultiTrie *multrie)
{
   init() ;
   if (multrie)
      {
      m_numfreq = multrie->countFreqRecords() ;
      m_size = multrie->numFullByteNodes() ;
      m_numterminals = multrie->numTerminalNodes() ;
      m_size -= m_numterminals ;
      m_nodes = FrNewN(PackedTrieNode,m_size) ;
      m_terminals = FrNewN(PackedTrieTerminalNode,m_numterminals) ;
      m_freq = FrNewN(PackedTrieFreq,m_numfreq) ;
      if (m_nodes && m_freq)
	 {
	 const MultiTrieNode *mroot = multrie->rootNode() ;
	 PackedTrieNode *proot = &m_nodes[PTRIE_ROOT_INDEX] ;
	 new (proot) PackedTrieNode ;
	 m_used = 1 ;
	 if (!insertChildren(proot,multrie,mroot,PTRIE_ROOT_INDEX))
	    {
	    m_size = 0 ;
	    m_numfreq = 0 ;
	    m_numterminals = 0 ;
	    }
	 cout << "   converted " << m_used << " full nodes, "
	      << m_termused << " terminals, and "
	      << m_freqused << " frequencies" << endl ;
	 }
      else
	 {
	 FrFree(m_nodes) ;
	 FrFree(m_freq) ;
	 m_nodes = 0 ; 
	 m_freq = 0 ;
	 m_size = 0 ;
	 m_numfreq = 0 ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

PackedMultiTrie::PackedMultiTrie(FILE *fp, const char *filename)
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
	 m_nodes = (PackedTrieNode*)((char*)FrMappedAddress(fmap) + offset) ;
	 m_freq = (PackedTrieFreq*)(m_nodes + m_size) ;
	 m_terminals = (PackedTrieTerminalNode*)(m_freq + m_numfreq) ;
	 }
      else
	 {
	 // unable to memory-map the file, so read its contents into buffers
	 //   and point our variables at the buffers
	 char *nodebuffer = FrNewN(char,(m_size * sizeof(PackedTrieNode)) + (m_numterminals * sizeof(PackedTrieTerminalNode))) ;
	 m_nodes = (PackedTrieNode*)nodebuffer ;
	 m_terminals = (PackedTrieTerminalNode*)(m_nodes + m_size) ;
	 m_terminals_contiguous = true ;
	 m_freq = FrNewN(PackedTrieFreq,m_numfreq) ;
	 if (!m_nodes || !m_freq ||
	     fread(m_nodes,sizeof(PackedTrieNode),m_size,fp) != m_size ||
	     fread(m_freq,sizeof(PackedTrieFreq),m_numfreq,fp) != m_numfreq ||
	     fread(m_terminals,sizeof(PackedTrieTerminalNode),m_numterminals,fp) != m_numterminals)
	    {
	    FrFree(m_nodes) ;  m_nodes = 0 ;
	    FrFree(m_freq) ;   m_freq = 0 ;
	    m_terminals = 0 ;
	    m_size = 0 ; 
	    m_numfreq = 0 ;
	    m_numterminals = 0 ;
	    }
	 }
      }
   return ;
}

//----------------------------------------------------------------------

PackedMultiTrie::~PackedMultiTrie()
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
      FrFree(m_freq) ;
      }
   init() ;				// clear all of the fields
   return ;
}

//----------------------------------------------------------------------

void PackedMultiTrie::init()
{
   m_fmap = 0 ;
   m_nodes = 0 ;
   m_terminals = 0 ;
   m_freq = 0 ;
   m_size = 0 ;
   m_used = 0 ;
   m_numterminals = 0 ;
   m_termused = 0 ;
   m_numfreq = 0 ;
   m_freqused = 0 ;
   m_maxkeylen = 0 ;
   m_casesensitivity = CS_Full ;
   m_ignorewhitespace = false ;
   m_terminals_contiguous = false ;
   initialize_popcount32() ;
   return ;
}

//----------------------------------------------------------------------

uint32_t PackedMultiTrie::allocateChildNodes(unsigned numchildren)
{
   uint32_t index = m_used ;
   m_used += numchildren ;
   if (m_used > m_size)
      {
      m_used = m_size ;
      return NOCHILD_INDEX ;		// error!  should never happen!
      }
   // initialize each of the new children
   for (size_t i = 0 ; i < numchildren ; i++)
      {
      new (m_nodes + index + i) PackedTrieNode ;
      }
   return index ;
}

//----------------------------------------------------------------------

uint32_t PackedMultiTrie::allocateTerminalNodes(unsigned numchildren)
{
   uint32_t index = m_termused ;
   m_termused += numchildren ;
   if (m_termused > m_numterminals)
      {
      m_termused = m_numterminals ;
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

bool PackedMultiTrie::insertTerminals(PackedTrieNode *parent,
				      const MultiTrie *mtrie,
				      const MultiTrieNode *mnode,
				      uint32_t mnode_index,
				      unsigned keylen)
{
   if (!parent || !mnode)
      return false ;
   unsigned numchildren = mnode->numExtensions(mtrie) ;
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
      if (mtrie->extendKey(nodeindex,(uint8_t)i))
	 {
	 // set the appropriate bit in the child array
	 parent->setChild(i) ;
	 // add frequency info to the child node
	 PackedTrieNode *pchild = node(firstchild + index) ;
	 index++ ;
	 const MultiTrieNode *mchild = mtrie->node(nodeindex) ;
	 unsigned numfreq = mchild->numFrequencies() ;
	 if (numfreq > 0)
	    {
	    uint32_t freq_index = m_freqused ;
	    m_freqused += numfreq ;
	    pchild->setFrequencies(freq_index) ;
	    const MultiTrieFrequency *mfreq = mchild->frequencies() ;
	    while (mfreq && numfreq > 0)
	       {
	       (void)new (m_freq + freq_index) PackedTrieFreq
		  (mfreq->frequency(),mfreq->languageID(),numfreq <= 1) ;
	       freq_index++ ;
	       numfreq-- ;
	       mfreq = mfreq->next() ;
	       }
	    }
	 else
	    pchild->setFrequencies(INVALID_FREQ) ;
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

bool PackedMultiTrie::insertChildren(PackedTrieNode *parent,
				     const MultiTrie *mtrie,
				     const MultiTrieNode *mnode,
				     uint32_t mnode_index,
				     unsigned keylen)
{
   if (!parent || !mnode)
      return false ;
   // first pass: fill in all the children
   unsigned numchildren = mnode->numExtensions(mtrie) ;
   if (numchildren == 0)
      return true ;
   keylen++ ;
   if (keylen > longestKey())
      m_maxkeylen = keylen ;
   bool terminal = mnode->allChildrenAreTerminals(mtrie) ;
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
      uint32_t nodeindex = mnode_index ;
      if (mtrie->extendKey(nodeindex,(uint8_t)i))
	 {
	 // set the appropriate bit in the child array
	 parent->setChild(i) ;
	 // add frequency info to the child node
	 PackedTrieNode *pchild = node(firstchild + index) ;
	 index++ ;
	 const MultiTrieNode *mchild = mtrie->node(nodeindex) ;
	 unsigned numfreq = mchild->numFrequencies() ;
	 if (numfreq > 0)
	    {
	    uint32_t freq_index = m_freqused ;
	    m_freqused += numfreq ;
	    pchild->setFrequencies(freq_index) ;
	    const MultiTrieFrequency *mfreq = mchild->frequencies() ;
	    while (mfreq && numfreq > 0)
	       {
	       bool is_stop = (mfreq->isStopgram() || mfreq->frequency() == 0);
	       (void)new (m_freq + freq_index) PackedTrieFreq
		  (mfreq->frequency(),mfreq->languageID(),numfreq <= 1,is_stop) ;
	       freq_index++ ;
	       numfreq-- ;
	       mfreq = mfreq->next() ;
	       }
	    }
	 if (terminal)
	    {
	    if (!insertTerminals(pchild,mtrie,mchild,nodeindex,keylen))
	       return false ;
	    }
	 else if (!insertChildren(pchild,mtrie,mchild,nodeindex,keylen))
	    return false ;
	 }
      }
   parent->setPopCounts() ;
   return true ;
}

//----------------------------------------------------------------------

bool PackedMultiTrie::parseHeader(FILE *fp)
{
   const size_t siglen = sizeof(MULTITRIE_SIGNATURE) ;
   char signature[siglen] ;
   if (fread(signature,sizeof(char),siglen,fp) != siglen ||
       memcmp(signature,MULTITRIE_SIGNATURE,siglen) != 0)
      {
      // error: wrong file type
      return false ;
      }
   unsigned char version ;
   if (fread(&version,sizeof(char),sizeof(version),fp) != sizeof(version)
       || version < MULTITRIE_FORMAT_MIN_VERSION
       || version > MULTITRIE_FORMAT_VERSION)
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
   LONGbuffer val_size, val_keylen, val_numfreq, val_numterm ;
   char ignore_white ;
   char case_sens ;
   char padbuf[MULTITRIE_PADBYTES_1] ;
   if (fread(val_size,sizeof(val_size),1,fp) != 1 ||
       fread(val_keylen,sizeof(val_keylen),1,fp) != 1 ||
       fread(val_numfreq,sizeof(val_numfreq),1,fp) != 1 ||
       fread(val_numterm,sizeof(val_numterm),1,fp) != 1 ||
       fread(&ignore_white,sizeof(ignore_white),1,fp) != 1 ||
       fread(&case_sens,sizeof(case_sens),1,fp) != 1 ||
       fread(padbuf,1,sizeof(padbuf),fp) != sizeof(padbuf))
      {
      // error reading header
      return false ;
      }
   m_maxkeylen = FrLoadLong(val_keylen) ;
   m_size = FrLoadLong(val_size) ;
   m_numterminals = FrLoadLong(val_numterm) ;
   m_numfreq = FrLoadLong(val_numfreq) ;
   return true ;
}

//----------------------------------------------------------------------

PackedTrieNode *PackedMultiTrie::findNode(const uint8_t *key,
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

bool PackedMultiTrie::extendKey(uint32_t &nodeindex, uint8_t keybyte) const
{
   if ((nodeindex & PTRIE_TERMINAL_MASK) != 0)
      {
      nodeindex = NULL_INDEX ;
      return false ;
      }
#if 0 // currently not used, so don't waste time
   if (ignoringWhiteSpace() && keybyte == ' ')
      return true ;
   switch (caseSensitivity())
      {
      case CS_Full:
      default:
	 // do nothing
	 break ;
      case CS_ASCII:
	 if (isascii(keybyte))
	    keybyte = tolower(keybyte) ;
	 break ;
      case CS_Latin1:
	 keybyte = Fr_tolower(keybyte) ;
	 break ;
      }
#endif
   PackedTrieNode *n = node(nodeindex) ;
   uint32_t index = n->childIndexIfPresent(keybyte) ;
   nodeindex = index ;
   return (index != NULL_INDEX) ;
}

//----------------------------------------------------------------------

uint32_t PackedMultiTrie::extendKey(uint8_t keybyte, uint32_t nodeindex) const
{
   if ((nodeindex & PTRIE_TERMINAL_MASK) != 0)
      {
      return NULL_INDEX ;
      }
#if 0 // currently not used, so don't waste time
   if (ignoringWhiteSpace() && keybyte == ' ')
      return true ;
   switch (caseSensitivity())
      {
      case CS_Full:
      default:
	 // do nothing
	 break ;
      case CS_ASCII:
	 if (isascii(keybyte))
	    keybyte = tolower(keybyte) ;
	 break ;
      case CS_Latin1:
	 keybyte = Fr_tolower(keybyte) ;
	 break ;
      }
#endif
   PackedTrieNode *n = node(nodeindex) ;
   return n->childIndexIfPresent(keybyte) ;
}

//----------------------------------------------------------------------

bool PackedMultiTrie::enumerate(uint8_t *keybuf, unsigned maxkeylength,
				PackedTrieEnumFn *fn, void *user_data) const
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

PackedMultiTrie *PackedMultiTrie::load(FILE *fp, const char *filename)
{
   if (fp)
      {
      PackedMultiTrie *trie = new PackedMultiTrie(fp,filename) ;
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

PackedMultiTrie *PackedMultiTrie::load(const char *filename)
{
   if (filename && *filename)
      {
      FILE *fp = fopen(filename,"rb") ;
      if (fp)
	 {
	 PackedMultiTrie *trie = load(fp,filename) ;
	 fclose(fp) ;
	 return trie ;
	 }
      }
   return 0 ;
}

//----------------------------------------------------------------------

bool PackedMultiTrie::writeHeader(FILE *fp) const
{
   // write the signature string
   const size_t siglen = sizeof(MULTITRIE_SIGNATURE) ;
   if (fwrite(MULTITRIE_SIGNATURE,sizeof(char),siglen,fp) != siglen)
      return false; 
   // follow with the format version number
   unsigned char version = MULTITRIE_FORMAT_VERSION ;
   if (fwrite(&version,sizeof(char),sizeof(version),fp) != sizeof(version))
      return false ;
   unsigned char bits = PTRIE_BITS_PER_LEVEL ;
   if (fwrite(&bits,sizeof(char),sizeof(bits),fp) != sizeof(bits))
      return false ;
   // write out the size of the trie
   LONGbuffer val_used, val_keylen, val_numfreq, val_numterm ;
   FrStoreLong(size(),val_used) ;
   FrStoreLong(longestKey(),val_keylen) ;
   FrStoreLong(m_numfreq,val_numfreq) ;
   FrStoreLong(m_numterminals,val_numterm) ;
   char case_sens = caseSensitivity() ;
   if (fwrite(val_used,sizeof(val_used),1,fp) != 1 || 
       fwrite(val_keylen,sizeof(val_keylen),1,fp) != 1 ||
       fwrite(val_numfreq,sizeof(val_numfreq),1,fp) != 1 ||
       fwrite(val_numterm,sizeof(val_numterm),1,fp) != 1 ||
       fwrite(&m_ignorewhitespace,sizeof(m_ignorewhitespace),1,fp) != 1 ||
       fwrite(&case_sens,sizeof(case_sens),1,fp) != 1)
      return false ;
   // pad the header with NULs for the unused reserved portion of the header
   for (size_t i = 0 ; i < MULTITRIE_PADBYTES_1 ; i++)
      {
      if (fputc('\0',fp) == EOF)
	 return false ;
      }
   return true ;
}

//----------------------------------------------------------------------

bool PackedMultiTrie::write(FILE *fp) const
{
   if (fp)
      {
      if (writeHeader(fp))
	 {
	 // write the actual trie nodes
	 if (fwrite(m_nodes,sizeof(PackedTrieNode),m_size,fp) != m_size)
	    return false ;
	 // write the frequency information
	 if (fwrite(m_freq,sizeof(PackedTrieFreq),m_numfreq,fp) != m_numfreq)
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
   PackedMultiTrie *trie = (PackedMultiTrie*)user_data ;
   return trie->write(fp) ;
}

//----------------------------------------------------------------------

bool PackedMultiTrie::write(const char *filename) const
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

static const PackedTrieFreq *base_frequency ;

static bool dump_ngram(const PackedTrieNode *node, const uint8_t *key,
		       unsigned keylen, void *user_data)
{
   FILE *fp = (FILE*)user_data ;
   if (fp && node)
      {
      fprintf(fp,"   ") ;
      for (size_t i = 0 ; i < keylen ; i++)
	 {
	 write_escaped_char(key[i],fp) ;
	 }
      fprintf(fp,"  ::") ;
      const PackedTrieFreq *freq = node->frequencies(base_frequency) ;
      for ( ; freq ; freq++)
	 {
	 fprintf(fp," %lu=%g",(unsigned long)freq->languageID(),
		 freq->probability()) ;
	 if (freq->isLast())
	    break ;
	 }
      fprintf(fp,"\n") ;
      }
   return true ;
}

//----------------------------------------------------------------------

bool PackedMultiTrie::dump(FILE *fp) const
{
   FrLocalAlloc(uint8_t,keybuf,10000,longestKey()) ;
   base_frequency = m_freq ;
   bool success = keybuf ? enumerate(keybuf,longestKey(),dump_ngram,fp) : false ;
   FrLocalFree(keybuf) ;
   return success ;
}

/************************************************************************/
/*	Methods for class PackedMultiTriePointer			*/
/************************************************************************/

bool PackedMultiTriePointer::hasChildren(uint32_t node_index,
					 uint8_t keybyte) const
{
   PackedTrieNode *n = m_trie->node(node_index) ;
   return n->childPresent(keybyte) ;
}

//----------------------------------------------------------------------

bool PackedMultiTriePointer::extendKey(uint8_t keybyte)
{
   if (m_failed)
      return false ;
   m_nodeindex = m_trie->extendKey(keybyte,m_nodeindex) ;
   if (m_nodeindex != NULL_INDEX)
      {
      m_keylength++ ;
      return true ;
      }
   else
      {
      m_failed = true ;
      return false ;
      }
}

//----------------------------------------------------------------------

bool PackedMultiTriePointer::lookupSuccessful() const
{
   PackedTrieNode *n = node() ;
   return n && n->leaf() ;
}

/************************************************************************/
/*	Additional methods for class MultiTrie				*/
/************************************************************************/

// note: these global variables make add_ngram non-reentrant
static const PackedTrieFreq *frequency_base = 0 ;
static const PackedTrieFreq *frequency_end = 0 ;

static bool add_ngram(const PackedTrieNode *node, const uint8_t *key,
		      unsigned keylen, void *user_data)
{
   // not the most efficient method, since it does a separate insertion
   //   for each language ID, but we only use this during training so
   //   speed is not critical
   MultiTrie *trie = (MultiTrie*)user_data ;
   const PackedTrieFreq *frequencies = node->frequencies(frequency_base) ;
   if (frequencies)
      {
      for ( ; frequencies < frequency_end ; frequencies++)
	 {
	 trie->insert(key,keylen,frequencies->languageID(),
		      (uint32_t)(frequencies->probability() * TRIE_SCALE_FACTOR + 0.5),
		      frequencies->isStopgram()) ;
	 if (frequencies->isLast())
	    break ;
	 }
      }
   return true ;
}

//----------------------------------------------------------------------

MultiTrie::MultiTrie(const class PackedMultiTrie *ptrie)
{
   if (ptrie)
      {
      init(ptrie->size() * 3 / 2) ;
      FrLocalAlloc(uint8_t,keybuf,512,ptrie->longestKey()) ;
      if (keybuf)
	 {
	 frequency_base = ptrie->frequencyBaseAddress() ;
	 frequency_end = frequency_base + ptrie->numFrequencies() ;
	 ptrie->enumerate(keybuf,ptrie->longestKey(),add_ngram,this) ;
	 frequency_base = 0 ;
	 frequency_end = 0 ;
	 FrLocalFree(keybuf) ;
	 }
      }
   else
      init(1) ;
   return ;
}

// end of file ptrie.C //
