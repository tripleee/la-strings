/****************************** -*- C++ -*- *****************************/
/*									*/
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File: trie.h - Word-frequency trie					*/
/*  Version:  1.24				       			*/
/*  LastEdit: 08aug2014							*/
/*									*/
/*  (c) Copyright 2011,2012,2014 Ralf Brown/CMU				*/
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

#ifndef __TRIE_H_INCLUDED
#define __TRIE_H_INCLUDED

#include <cstdio>
#include <limits.h>
#include <stdint.h>

using namespace std ;

/************************************************************************/
/*	Manifest Constants						*/
/************************************************************************/

#define NULL_INDEX 0

// we can trade off speed for memory by adjusting how many bits each
//   node in the trie represents.  Current supported values are 2, 3,
//   and 4; two bits per node uses about 60% as much total memory as 4
//   bits, but needs twice as many memory accesses for lookups; three bits
//   is in-between
#define BITS_PER_LEVEL 2

// we want to store percentages for entries in the trie in 32 bits.  Since
//   it is very unlikely that any ngram in the trie will have a probability
//   greater than 4.2%, scale the percentage by a factor of one billion.
#define TRIE_SCALE_FACTOR 1000000000L

/************************************************************************/
/************************************************************************/

class NybbleTrie ;
class NybbleTrieNode ;
class NybbleTriePointer ;

class WildcardSet ;
class WildcardCollection ;

//----------------------------------------------------------------------

typedef bool NybbleTrieEnumFn(const NybbleTrieNode *node,
			      const uint8_t *key, unsigned keylen,
			      void *user_data) ;

//----------------------------------------------------------------------

class NybbleTrieNode
   {
   private:
      uint32_t	m_children[1<<BITS_PER_LEVEL] ;
      uint32_t  m_frequency ;
      bool	m_leaf ;
      bool	m_stopgram ;
   protected:
      bool nextFrequencies(const NybbleTrie *trie, uint32_t *frequencies,
			   uint8_t idx, unsigned bits) const ;

   public:
      void *operator new(size_t, void *where) { return where ; }
      NybbleTrieNode() ;
      ~NybbleTrieNode() {}

      // accessors
      bool leaf() const { return m_leaf ; }
      bool isStopgram() const { return m_stopgram ; }
      bool hasChildren() const ;
      bool hasChildren(const NybbleTrie *trie, uint32_t min_freq) const ;
      bool childPresent(unsigned int N) const ;
      bool allChildrenAreTerminals(const NybbleTrie *trie) const ;
      bool allChildrenAreTerminals(const NybbleTrie *trie,
				   uint32_t min_freq) const ;
      uint32_t childIndex(unsigned int N) const ;
      uint32_t frequency() const { return m_frequency ; }
      unsigned numExtensions(const NybbleTrie *trie,
			     uint32_t min_freq = 0) const ;
      bool singleChild(const NybbleTrie *trie) const ;
      bool singleChildSameFreq(const NybbleTrie *trie,
			       bool allow_nonleaf, double ratio) const ;
      bool enumerateChildren(const NybbleTrie *trie,
			     uint8_t *keybuf, unsigned max_keylength_bits,
			     unsigned curr_keylength_bits,
			     NybbleTrieEnumFn *fn,
			     void *user_data) const ;
      bool enumerateFullNodes(const NybbleTrie *trie, uint32_t &count,
			      uint32_t min_freq = 0) const ;
      bool enumerateTerminalNodes(const NybbleTrie *trie,
				  unsigned keylen_bits, uint32_t &count,
				  uint32_t min_freq = 0) const ;
      bool nextFrequencies(const NybbleTrie *trie,
			   uint32_t *frequencies) const ;

      // modifiers
      void markAsLeaf() { m_leaf = true ; }
      void markAsStopgram(bool stop = true) { m_stopgram = stop ; }
      void setFrequency(uint32_t f) { m_frequency = f ; }
      void incrFrequency(uint32_t incr = 1) { m_frequency += incr ; }
      void scaleFrequency(uint64_t total_count) ;
      void scaleFrequency(uint64_t total_count, double power, double log_power) ;
      bool insertChild(unsigned int N, NybbleTrie *trie) ;

      // I/O
      static NybbleTrieNode *read(FILE *fp) ;
      bool write(FILE *fp) const ;
   } ;

// backward compatibility
typedef NybbleTrieNode NybbleTrieNodeSingle ;

//----------------------------------------------------------------------

class NybbleTrie
   {
   private:
      NybbleTrieNode  **m_nodes ;	// list of buckets of nodes
      void	       *m_userdata ;
      uint32_t	        m_capacity ;
      uint32_t	 	m_used ;
      uint32_t	 	m_totaltokens ;
      unsigned	 	m_maxkeylen ;
      bool	 	m_ignorewhitespace ;
   private:
      void init(uint32_t capacity) ;
      bool extendNybble(uint32_t &nodeindex, uint8_t nybble) const ;
      uint32_t insertNybble(uint32_t nodeindex, uint8_t nybble) ;
   public:
      NybbleTrie(uint32_t capacity = 0) ;
      NybbleTrie(const char *filename, bool verbose) ;
      NybbleTrie(const class PackedTrie *ptrie) ;
      ~NybbleTrie() ;

      bool loadWords(const char *filename, bool verbose) ;
      uint32_t allocateNode() ;

      // modifiers
      void setUserData(void *ud) { m_userdata = ud ; }
      void ignoreWhiteSpace(bool ignore = true) { m_ignorewhitespace = ignore ; }
      void addTokenCount(uint32_t incr = 1) { m_totaltokens += incr ; }
      bool insert(const uint8_t *key, unsigned keylength,
		  uint32_t frequency, bool stopgram) ;
      bool insertMax(const uint8_t *key, unsigned keylength,
		     uint32_t frequency, bool stopgram) ;
      void insertChild(uint32_t &nodeindex, uint8_t keybyte) ;
      uint32_t increment(const uint8_t *key, unsigned keylength,
			 uint32_t incr = 1, bool stopgram = false) ;
      bool incrementExtensions(const uint8_t *key, unsigned prevlength,
			       unsigned keylength, uint32_t incr = 1) ;

      // accessors
      void *userData() const { return m_userdata ; }
      uint32_t size() const { return m_used ; }
      uint32_t capacity() const { return m_capacity ; }
      uint32_t totalTokens() const { return m_totaltokens ; }
      unsigned longestKey() const { return m_maxkeylen ; }
      bool ignoringWhiteSpace() const { return m_ignorewhitespace ; }
      NybbleTrieNode *node(uint32_t N) const ;
      NybbleTrieNode *rootNode() const ;
      NybbleTrieNode *findNode(const uint8_t *key, unsigned keylength) const ;
      uint32_t find(const uint8_t *key, unsigned keylength) const ;
      bool extendKey(uint32_t &nodeindex, uint8_t keybyte) const ;
      bool enumerate(uint8_t *keybuf, unsigned maxkeylength,
		     NybbleTrieEnumFn *fn, void *user_data) const ;
      bool scaleFrequencies(uint64_t total_count) ;
      bool scaleFrequencies(uint64_t total_count, double power, double log_power) ;
      uint32_t numTerminalNodes(uint32_t min_freq = 0) const ;
      uint32_t numFullNodes(uint32_t min_freq = 0) const ;

      // I/O
      static NybbleTrie *load(FILE *fp) ;
      static NybbleTrie *load(const char *filename) ;
      bool write(FILE *fp) const ;
   } ;

//----------------------------------------------------------------------

class NybbleTriePointer
   {
   private:
      const NybbleTrie	*m_trie ;
      uint32_t		 m_nodeindex ;
      int		 m_keylength ;
      bool		 m_failed ;
   public:
      void initPointer(const NybbleTrie *t)
	 { m_trie = t ; m_nodeindex = 0 ; m_keylength = 0 ; m_failed = false ; }
      NybbleTriePointer() { initPointer(0) ; } // for arrays
      NybbleTriePointer(const NybbleTrie *t) { initPointer(t) ; }
      ~NybbleTriePointer() { m_trie = 0 ; m_nodeindex = 0 ; m_failed = true ; }

      void resetKey() ;
      bool extendKey(uint8_t keybyte) ;

      // accessors
      bool lookupSuccessful() const ;
      bool hasExtension(uint8_t keybyte) const ;
      bool hasChildren(uint32_t node_index, uint8_t nybble) const ;
      int keyLength() const { return m_keylength ; }
      NybbleTrieNode *node() const
         { return m_failed ? 0 : m_trie->node(m_nodeindex) ; }
   } ;

/************************************************************************/
/************************************************************************/

double scaling_log_power(double power) ;
double scale_frequency(double freq, double power, double log_power) ;
double unscale_frequency(uint32_t scaled, double power) ;
uint32_t scaled_frequency(uint32_t raw_freq, uint64_t total_count) ;
uint32_t scaled_frequency(uint32_t raw_freq, uint64_t total_count,
			  double power, double log_power) ;

#endif /* !__TRIE_H_INCLUDED */

/* end of file trie.h */
