/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC								*/
/*  Version 1.98							*/
/*	 by Jae Dong Kim						*/
/*									*/
/*  File: frclust3.cpp	      Tight Clustering main code		*/
/*  LastEdit: 04nov09 by Ralf Brown					*/
/*									*/
/*  (c) Copyright 2005,2006,2009 Jae Dong Kim and Ralf Brown		*/
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

#include "frassert.h"
#include "frclust.h"
#include "frclustp.h"
#include "frrandom.h"
#include "frqsort.h"
#include "frsignal.h"
#include "frtimer.h"
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

// accumulate all the old clusters
//#define ACCUMULATE_CLUSTER_HISTORY

/************************************************************************/
/*	Manifest Constants						*/
/************************************************************************/


#define MAX_MATRIX_SIZE  30000

#define SAMPLE_RATIO 0.66
#define ITERATION_B 10
//#define SMALLEST_K  51
#define SMALLEST_K  6

#define MAX_ITERATIONS_KMEANS 5

/************************************************************************/
/*	Types								*/
/************************************************************************/

// since we have B <= 255, we can store the count in a byte
typedef char ComemberCount ;

//----------------------------------------------------------------------

/* data structure for cluster sort by size */
class cluster_sort
{
   public:
      int size;
      int idx;
      int *clusts;
   public:
      static int compare(const cluster_sort &, const cluster_sort &) ;
      static void swap(cluster_sort &, cluster_sort &) ;
} ;

/************************************************************************/
/*	Global Variables for this module				*/
/************************************************************************/

static bool abort_requested = false ;
static FrSignalHandler *sigint ;

/************************************************************************/
/*	Helper Functions						*/
/************************************************************************/

#ifdef __DEBUG
#  define DEBUGOUT(x) cout << x ;
#else
#  define DEBUGOUT(x)
#endif /* __DEBUG */


//----------------------------------------------------------------------

static void sigint_handler(int)
{
   if (!abort_requested)
      {
      cout << ";; *** User interrupt: clustering will be aborted after current iteration ***"
	   << endl ;
      abort_requested = true ;
      }
   return ;
}

//----------------------------------------------------------------------

/* copy a term vector list as big as I want */
static FrList *copylist(const FrList *l1, int max_copy_size)
{
   FrList *result = 0 ;
   FrList **end = &result ;

   for ( ; l1 && --max_copy_size ; l1 = l1->rest())
      {
      FrObject *obj = l1->first();
      result->pushlistend(obj?obj->deepcopy():obj,end) ;
      }
   *end = 0 ;       // properly terminate the result list
   return result ;
}

//----------------------------------------------------------------------

static void clear_gensym_clusters(const FrList *vectors)
{
   for ( ; vectors ; vectors = vectors->rest())
      {
      FrTermVector *tv = (FrTermVector*)vectors->first() ;
      FrSymbol *clusname = tv->cluster() ;
      if (clusname && FrIsGeneratedClusterName(clusname))
	 tv->setCluster(0) ;
      }
   return ;
}

//----------------------------------------------------------------------

/* add comembership of cluster members to the comembership matrix */
static void save_comembership(const FrSymHashTable *clusters,
			      ComemberCount **D)
{
   // for each cluster
   FrList *keys = clusters->allKeys();
   for( FrList *key = keys; key; key = key->rest() )
      {
      FrSymHashEntry *entry = clusters->lookup((FrSymbol *)key->first());
      FrList *clus = (FrList *)entry->getUserData();
      DEBUGOUT(("Cluster SIZE: " << clus->simplelistlength() << endl)) ;
      // first two elements in a cluster are not for vector terms
      // these two loops add all the comemberships between any two terms
      for (FrList *tmp_clus1=FrCLUSTERMEMBERS(clus) ;
	   tmp_clus1 ;
	   tmp_clus1=tmp_clus1->rest())
	 {
	 for( FrList *tmp_clus2=tmp_clus1->rest();
	      tmp_clus2; tmp_clus2=tmp_clus2->rest())
	    {
	    int idx_i = ((FrTermVector *)tmp_clus1->first())->getId();
	    int idx_j = ((FrTermVector *)tmp_clus2->first())->getId();
	    DEBUGOUT(("IDX_I: " << idx_i << endl)) ;
	    DEBUGOUT(("IDX_J: " << idx_j << endl)) ;
	    // in D[i][j], j is always larger than i
	    if( idx_i<idx_j )
	       D[idx_i][idx_j] += 1;
	    else
	       D[idx_j][idx_i] += 1;
	    }
	 }
      }
   free_object(keys) ;
   return ;
}

//----------------------------------------------------------------------

/* find a tight cluster from the comembership matrix */
static void find_cluster(
   int *tids,
   int *tid_size,
   int start_point,
   int data_size,
   ComemberCount **D,
   ComemberCount threshold
   )
{
   int j ;
   do {
      for(j=start_point+1; j<data_size; ++j )
	 {
	 if( D[start_point][j] >= threshold )
	    {
	    int k ;
	    for( k=0; k<*tid_size; ++k )
	       {
	       if( D[tids[k]][j] < threshold )
		  break ;
	       }
	    if( k == *tid_size )
	       {
	       for( k=0; k<*tid_size; ++k )
		  D[tids[k]][j] = 0;
	       DEBUGOUT(("SIZE: " << *tid_size << ", TID: " << j << endl)) ;
	       tids[(*tid_size)++] = j;
	       start_point = j;
	       break;
	       }
	    }
	 }
      } while (j < data_size) ;
   return ;
}

//----------------------------------------------------------------------

inline int cluster_sort::compare(const cluster_sort &c1,
				 const cluster_sort &c2)
{
   return c2.size - c1.size ;
}

//----------------------------------------------------------------------

inline void cluster_sort::swap(cluster_sort &c1, cluster_sort &c2)
{
   cluster_sort tmp = c1 ;
   c1 = c2 ;
   c2 = tmp ;
   return ;
}

//----------------------------------------------------------------------

/* find and get q top clusters by size from the comembership matrix */
static void get_top_clusters(
   FrList *vector,
   FrSymHashTable *clusters,
   size_t q,
   ComemberCount threshold,
   int data_size,
   ComemberCount **D,
   const FrClusteringParameters *params,
   bool /* run_verbosely */
   )
{
   // to store a sequence of term ids which are in a tight cluster
   int tids[MAX_MATRIX_SIZE];

   // to store cluster sizes
   size_t csize = data_size;
   cluster_sort *cs = FrNewN(cluster_sort,csize) ;
   size_t ccount = 0;

   // first, get cluster sizes, and then I will make only top q clusters
   for (int i=0; i<data_size; ++i )
      {
      for(int j=i+1; j<data_size; ++j )
	 {
	 // This pair is comembers of a tight cluster
	 if( D[i][j] >= threshold )
	    {
	    if( ccount == csize )
	       {
	       csize += MAX_MATRIX_SIZE;
	       cluster_sort *newcs = FrNewR(cluster_sort,cs,csize) ;
	       if (!newcs)
		  {
		  FrNoMemory("while extracting top clusters") ;
		  break ;
		  }
	       cs = newcs ;
	       }
	    int tid_size=0;
	    tids[tid_size++] = i;
	    tids[tid_size++] = j;
	    D[i][j] = 0;
	    // expand the tight cluster as much as possible
	    find_cluster(tids, &tid_size, j, data_size, D, threshold) ;
	    DEBUGOUT((ccount << ":" << i << "," << j << ":" << tid_size << endl));
	    cs[ccount].size = tid_size;
	    cs[ccount].idx = ccount;
	    cs[ccount].clusts = FrNewN(int,tid_size) ;
	    memcpy(cs[ccount].clusts, tids, sizeof(int)*tid_size);
	    ccount++;
	    }
	 }
      }
   FrQuickSort(cs, ccount) ;

   DEBUGOUT(("Sorted Cluster Sizes" << endl)) ;
   for(size_t i=0; i<q && i<ccount; ++i )
      {
      DEBUGOUT((cs[i].size << endl)) ;
      FrSymbol *clustername = gen_cluster_sym();
      for( int k=0; k<cs[i].size; ++k )
	 {
	 FrTermVector *tv = (FrTermVector *)vector->nth(cs[i].clusts[k]);
	 Fr__insert_in_cluster(clusters,clustername,tv,params) ;
	 }
      }

   for(size_t i=0; i<ccount; ++i )
      FrFree(cs[i].clusts) ;
   FrFree(cs) ;
   return ;
}

//----------------------------------------------------------------------

// tight clustering algorithm
// tried to follow the notation of the paper
static void algorithm_A(
   FrList *X,       // data to cluster
   FrSymHashTable *clusters, // cluster output
   int B,           // subsampling times
   int k,           // k for K-means
   int q,           // top q clusters
   double alpha,    // alpha
   ComemberCount **D,     // comembership matrix
   FrClusteringParameters *params, // for alpha & other Fr function call
   bool run_verbosely       // verbose
   )
{
   int data_size = X->simplelistlength();
   int sample_size = (int)(SAMPLE_RATIO*data_size);
   int i, j;

   if (data_size <= 0)
      return ;

   // initialize the comembership matrix
   // j is always at least as big as i
   for (i=0; i<data_size; ++i)
      {
      for (j=i ; j < data_size ; ++j)
	 D[i][j] = 0;
      }

   for(i=0; i<B && !abort_requested; ++i )
      {
      DEBUGOUT(("B: " << i+1 << "/" << B << " , K: " << k << endl)) ;

      // make a shallow copy of X, since FrRandomSample frees the top-level
      //   list nodes of the input list
      FrList *tmpX = (FrList*)X->copy() ;
      FrList *sample = FrRandomSample( tmpX, sample_size, false, true );
#ifdef __DEBUG
      cout << "Sample Size: " << sample->simplelistlength()
	   << "/" << sample_size << endl;
      {
      int idx=0;
      for( FrList *j = sample; j; idx++, j=j->rest() )
	 {
	 FrTermVector *tv = (FrTermVector *)j->first();
	 cout << "Term" << idx << ": " << tv->numTerms() << endl; fflush(stdout);
	 }
      }
#endif

      FrSymHashTable *tmpC = new FrSymHashTable;
      if (tmpC) tmpC->onDelete(Fr__free_cluster) ;

      params->desiredClusters((unsigned int)k);
      if (params->maxIterations() > MAX_ITERATIONS_KMEANS )
	 params->maxIterations((size_t)MAX_ITERATIONS_KMEANS);

      // don't use cluster assignments from a prior iteration as seeds!
      clear_gensym_clusters(sample) ;

      // cluster the sampled vectors into tmpC
      Fr__cluster_kmeans( sample, tmpC, params, run_verbosely );

      // collect the centroids found by the k-Means clustering
      FrList *centroids = 0 ;
      tmpC->doHashEntries(Fr__extract_centroid,&centroids) ;

      // assign the outside sample terms to the clusters
      const FrList *s = sample;
      const FrList *v = X;
      for ( ; v ; v = v->rest())
	 {
	 // since sample is a subset of X, this comparison make sense
	 if( s && v->first()->equal(s->first()) )
	    {
	    s = s->rest();
	    continue;
	    }
	 FrTermVector *tv = (FrTermVector *)v->first();
	 // add the vector to the nearest cluster
	 FrSymbol *newcluster = Fr__nearest_centroid(centroids,tv,
						     params->measure(),
						     params) ;
	 Fr__insert_in_cluster(tmpC,newcluster,tv,params) ;
	 }
      // clean up the list of centroids
      centroids->eraseList(false) ;
      DEBUGOUT(("tmpC->currentSize: " << tmpC->currentSize() << endl)) ;

      // add relationship between 2 terms to comembership Matrix D using tmpC;
      save_comembership( tmpC, D );

      // erase top-level list nodes of sample, not the contained vectors
      sample->eraseList(false) ;
      sample = 0 ;
      // free temporary clusters
      delete tmpC;
      }

   // find top q cluster
   get_top_clusters(X,clusters, q,
		    (ComemberCount)(B * SAMPLE_RATIO * (1 - alpha) + 0.5),
		    data_size, D, params, run_verbosely);
   return ;
}

//----------------------------------------------------------------------

static int compare_ids(const FrObject *o1, const FrObject *o2)
{
   const FrTermVector *tv1 = (const FrTermVector*)o1 ;
   const FrTermVector *tv2 = (const FrTermVector*)o2 ;
   int id1 = tv1->getId() ;
   int id2 = tv2->getId() ;
   return id1 - id2 ;
}

//----------------------------------------------------------------------

static void sort_cluster_members(FrSymbol *key, FrSymHashTable *clus)
{
   FrSymHashEntry *entry = clus->lookup(key) ;
   if (entry)
      {
      FrList *cl = (FrList*)entry->getUserData() ;
      if (cl && cl->consp())
	 {
	 FrList *orig_cl = cl->rest() ;
	 cl = orig_cl->rest() ;
	 cl = cl->sort( compare_ids ) ;
	 // update the tail of orig_cl to point at the new head of the list
	 //   of cluster members
	 orig_cl->replacd(cl) ;
	 }
      else if (cl)
	 {
	 // somebody put something other than a list into the user data
	 entry->setUserData(0) ;
	 }
      }
   return ;
}

//----------------------------------------------------------------------

/* calculate the similarity between two clusters 	*/
/* sim(c1,c2) = |c1,c2_intersection|/|c1,c2_union| 	*/
/*   CLUSTER MEMBERS MUST BE SORTED			*/
static double cluster_similarity( const FrList *oldc, const FrList *newc )
{
   int uni = 0, its = 0 ;

   const FrList *oc = FrCLUSTERMEMBERS(oldc) ;
   const FrList *nc = FrCLUSTERMEMBERS(newc) ;
   int flag;
   // the cluster members have been sorted by ID, so run down the two lists
   //   looking for matches
   while( oc && nc )
      {
      flag = compare_ids(oc->first(),nc->first()) ;
      if( flag == 0 )
	 {
	 its++;
	 oc = oc->rest();
	 nc = nc->rest();
	 }
      else if( flag > 0 )
	 {
	 nc = nc->rest();
	 }
      else
	 {
	 oc = oc->rest();
	 }
      uni++;
      }
   if( oc ) uni += oc->simplelistlength();
   if( nc ) uni += nc->simplelistlength();

   DEBUGOUT(("Similarity: " << ((double)its)/uni << endl)) ;
   return ((double)its)/uni;
}

//----------------------------------------------------------------------

/* check if there is any converged cluster by checking every pair */
static bool converged_cluster(
   FrSymHashTable *old_cl,
   FrSymHashTable *new_cl,
   FrSymbol *&classname,
   const FrList *&clus,
   double beta
   )
{
   FrList *ok, *old_keys = old_cl->allKeys();
   FrList *new_keys = new_cl->allKeys();

   DEBUGOUT(("cluster counts: old->" << old_keys->simplelistlength()
	     << " : new->" << new_keys->simplelistlength() << endl)) ;

   for (ok = old_keys ; ok ; ok = ok->rest())
      sort_cluster_members((FrSymbol*)ok->first(),old_cl) ;

   while (new_keys)
      {
      FrSymbol *key = (FrSymbol*)poplist(new_keys) ;
      sort_cluster_members(key,new_cl) ;
      for( ok=old_keys; ok; ok=ok->rest() )
	 {
	 // found a converged cluster
	 double sim = cluster_similarity(
            (FrList *)old_cl->lookup((FrSymbol *)ok->first())->getUserData(),
            (FrList *)new_cl->lookup(key)->getUserData() );
	 DEBUGOUT(("Similarity: " << sim << endl)) ;
	 if (sim >= beta)
	    {
//	    classname = makeSymbol(key) ;
	    classname = key ;
	    clus = (FrList*)((FrList *)new_cl->lookup(classname)->getUserData());
	    free_object(old_keys) ;
	    free_object(new_keys) ;
	    return true;
	    }
	 }
      }
   free_object(old_keys) ;
   return false;
}

//----------------------------------------------------------------------

inline bool equal_inline(const FrObject *obj1,const FrObject *obj2)
{
   if (obj1 == obj2)    // two objects are equal if their pointers
      return true ;   //  are equal
   else if (!obj1 || !obj2) // if only one pointer is NULL, then the
      return false ;    //  objects can't be equal
   else
      return obj1->equal(obj2) ;
}

//----------------------------------------------------------------------

static bool term_equal( const FrObject *o1, const FrObject *o2 )
{
   if (o1 == o2) return true;
   else if (!o1 || !o2) return false;
   FrObject *key1 = ((FrTermVector*)o1)->key() ;
   FrObject *key2 = ((FrTermVector*)o2)->key() ;
   if (key1 == key2) return true ;
#if 1 // probably don't need the following checks
   const char *term1 = key1->printableName() ;
   const char *term2 = key2->printableName() ;
//  printf("[%s] = [%s]\n", term1, term2) ; fflush(stdout) ;
   if (term1 && term2 && strcmp(term1,term2) == 0)
      return true;
#endif
   return false;
}

//----------------------------------------------------------------------

/* remove clustered data from the original data set */
static FrList * remove_cluster_data(FrList *vectors, const FrList *clus)
{
   clus = clus ? FrCLUSTERMEMBERS(clus) : 0 ;
   if (!clus)
      return vectors ;
   FrList *result = 0 ;
   FrList **end = &result ;
   while (vectors)
      {
      FrObject *obj = poplist(vectors) ;
      if (!clus->member(obj,term_equal))
	 result->pushlistend(obj,end) ;
      }
   *end = 0 ;
   return result ;
}

//----------------------------------------------------------------------

/* Tight cluster main function */
void Fr__cluster_tight(const FrList *vectors, FrSymHashTable *clusters,
		       const FrClusteringParameters *params,
		       bool run_verbosely)
{
   int k0 = params->desiredClusters()+SMALLEST_K;      // k0: suitable starting point of k for K-means
   int q = 7;				// q: top q clusters
   if (vectors->simplelistlength() > MAX_MATRIX_SIZE )
      {
      cout << "Vocabulary size exceeds " << MAX_MATRIX_SIZE
	   << ". Truncated" << endl;
      }

   // copy first at most MAX_MATRIX_SIZE terms into tmpX to do clustering
   FrList *tmpX = copylist(vectors, MAX_MATRIX_SIZE);

   // build vocabulary for 2-dim array index
   FrList *vec = (FrList *)tmpX;
   for( int i=0; vec; ++i, vec=vec->rest() )
      {
      FrTermVector *tv = (FrTermVector *)vec->first();
      tv->setId(i);
      }

   // matrix allocation
   unsigned int matrix_alloc_size = tmpX->simplelistlength();
   // allocate comembership calculation matrix
   FrLocalAlloc(ComemberCount*,D,MAX_MATRIX_SIZE,matrix_alloc_size) ;
   if (!D)
      {
      FrNoMemory("allocating comembership matrix") ;
      return ;
      }
   for( unsigned int i=0; i<matrix_alloc_size; ++i )
      {
      D[i] = FrNewN(ComemberCount, matrix_alloc_size);
      if (!D[i])
	 {
	 FrNoMemory("allocating comembership matrix") ;
	 while (i > 0)
	    FrFree(D[--i]) ;
	 FrLocalFree(D) ;
	 return ;
	 }
      }

   cout << ";  total vectors = " << vectors->simplelistlength() << endl ;
   DEBUGOUT(("tmpX size[" << tmpX->simplelistlength() << "]" << endl)) ;

   // trap signals to allow graceful early termination
   sigint = new FrSignalHandler(SIGINT,sigint_handler) ;

   FrTimer timer ;
   // I save desiredClusters since it changes inside the loop
   unsigned int desired_clusters = params->desiredClusters();
   for( ; k0 >= SMALLEST_K && clusters->currentSize() < desired_clusters; --k0 )
      {
      int k = k0;
      FrSymHashTable *new_cl = 0;
      FrSymHashTable *old_cl = 0;
      FrSymbol *classname = 0;
      const FrList *clus = 0;
      double alpha = params->alpha() ;

      DEBUGOUT(("K0: " << k0 << endl)) ;
      timer.start() ;
      while (!abort_requested)
	 {
	 new_cl = new FrSymHashTable;
	 if (new_cl)
	    new_cl->onDelete(Fr__free_cluster) ;
	 else
	    {
	    FrNoMemory("allocating cluster table") ;
	    break ;
	    }
	 // get clusters
	 algorithm_A( tmpX, new_cl, ITERATION_B, k, q, alpha, D,
		      (FrClusteringParameters *)params, run_verbosely );
	 ++k;
	 // if this is the first iteration
	 if( old_cl == 0 )
	    {
	    old_cl = new_cl;
	    continue;
	    }
	 else if( old_cl->currentSize() == 0 || new_cl->currentSize() == 0 )
	    {
	    cout << "Break by 0 cluster counts: old->"
		 << old_cl->currentSize() << " : new->"
		 << new_cl->currentSize() << endl;
	    break;
	    }
	 // check the convergence
	 if( converged_cluster(old_cl,new_cl,classname,clus,params->beta()) )
	    {
	    DEBUGOUT(("Break by convergence" << endl)) ;
	    break;
	    }
	 if (k - k0 > 10)		// if we're having trouble finding
	    {				//   a suff. tight cluster, relax
	    alpha += 0.05 ;		//   the threshold gradually
	    if (alpha > 1.0)
	       alpha = 1.0 ;
	    }
#ifdef ACCUMULATE_CLUSTER_HISTORY
	 DEBUGOUT(("ACCUMULATE_CLUSTER_HISTORY is set " << endl)) ;
	 // move new clusters' entries to old clusters
	 FrList *tkeys = new_cl->allKeys();
	 while (tkeys)
	    {
	    FrSymbol *tkey = (FrSymbol*)poplist(tkeys) ;
	    FrSymHashEntry *entry = new_cl->lookup(tkey) ;
	    if (entry)
	       old_cl->add( tkey, entry->getUserData() ) ;
	    }
	 new_cl->onDelete(0) ;	// don't clean up, since we've copied ptrs
	 delete new_cl;
#else
	 // free old clusters and hold the new clusters as old clusters
	 delete old_cl;
	 old_cl = new_cl;
#endif /* ACCUMULATE_CLUSTER_HISTORY */
	 }

      // found a converged cluster
      if (classname)
	 {
	 cout << ";\tAdded cluster " << classname->printableName()
	      << " with " << FrCLUSTERMEMBERS(clus)->simplelistlength()
	      << " elements in " << timer.readsec() << " seconds" << endl;
	 // add this cluster as a tight cluster
	 (void)Fr__copy_cluster(clusters,classname,clus) ;

	 // remove terms from the original data set
	 tmpX = remove_cluster_data( tmpX, clus );
	 }

      // free new and old clusters
      delete new_cl;
      delete old_cl;
      if (abort_requested)
	 break ;
      }

   // free the comembership array
   for( unsigned int i=0; i<matrix_alloc_size; ++i )
      FrFree(D[i]) ;
   FrLocalFree(D);

   // free the temporary data
   free_object(tmpX) ;

   // untrap signals
   delete sigint ;
   sigint = 0 ;
   return ;
}

// end of file frclust3.cpp //
