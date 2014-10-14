/************************************************************************/
/*									*/
/*  FramepaC								*/
/*  Version 1.99							*/
/*	 by Rashmi Gangadharaiah and Ralf Brown				*/
/*									*/
/*  File: frclust5.cpp	      Spectral clustering main code		*/
/*  LastEdit: 23may12							*/
/*									*/
/*  (c) Copyright 2006,2009,2012 Rashmi Gangadharaiah and Ralf Brown	*/
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
#include "frtimer.h"

#ifdef FrSTRICT_CPLUSPLUS
#  include <cmath>
#  include <cstdlib>
#else
#  include <math.h>
#endif

/************************************************************************/
/*	Configuration Options						*/
/************************************************************************/

//#define DEBUG				// print out verbose debugging info
//#define SHOW_AFFINITY			// print out the affinity matrix
//#define RANDOMIZE_EIGEN_INIT		// random starting point for power meth
#define USE_SQRT			// use square root of sum for S*D

/************************************************************************/
/*	Manifest Constants						*/
/************************************************************************/

#define DEFAULT_NTH 7
#define EPSILON 1E-20

#define THRESHOLD 0.0000001
#define MAX_ITER 100

// allocate up to this many elements on the stack with FrLocalAlloc
#define MAX_STACK_BUF	30000

/************************************************************************/
/*	Types local to this module					*/
/************************************************************************/

class Matrix
   {
   private:
      double *m_values ;
      size_t  m_rows ;
      size_t  m_columns ;
   public:
      Matrix(size_t rows, size_t columns) ;
      ~Matrix() ;

      double &element(size_t rownum, size_t col)
	 { return m_values[rownum*m_columns+col] ; }
      double *row(size_t rownum)
	 { return &m_values[rownum*m_columns] ; }
      const double *row(size_t rownum) const
	 { return &m_values[rownum*m_columns] ; }
      size_t rows() const { return m_rows ; }
      size_t columns() const { return m_columns ; }
      bool good() const { return m_values != 0 ; }

      void setRow(size_t rownum, const double *new_row) _fnattr_nonnull
	 { double *r = &m_values[rownum*m_columns] ;
	   for (size_t i = 0 ; i < m_columns ; i++)
	      r[i] = new_row[i] ;
	 }
   } ;

/************************************************************************/
/*	Helper functions						*/
/************************************************************************/

static int compare_double(const void *val1, const void *val2)
{
   double diff = (*(double*)val1) - (*(double*)val2) ;
   return (diff < 0.0) ? -1 : ((diff > 0.0) ? +1 : 0) ;
}

/************************************************************************/
/*	Methods for class Matrix					*/
/************************************************************************/

Matrix::Matrix(size_t num_rows, size_t num_cols)
{
   m_rows = num_rows ;
   m_columns = num_cols ;
   m_values = FrNewN(double,num_rows*num_cols) ;
   return ;
}

//----------------------------------------------------------------------

Matrix::~Matrix()
{
   FrFree(m_values) ;
   m_values = 0 ;
   return ;
}

/************************************************************************/
/************************************************************************/

// use var=affinity(i,j) to access elements of the affinity matrix
#define affinity(i,j) (affinities[(i)*numvectors+(j)])

static double *compute_affinities(const FrList *vectors,
				  const FrClusteringParameters *params,
				  size_t &numvectors, FrList *&seeds,
				  bool run_verbosely, size_t *&seedIndex,
				  size_t &nseeds)
{
   FrTimer timer ;
   numvectors = vectors->simplelistlength() ;
   double *affinities = FrNewN(double,numvectors*numvectors) ;
   FrLocalAlloc(double,scale,MAX_STACK_BUF,numvectors) ;
   FrLocalAlloc(double,row,MAX_STACK_BUF,numvectors) ;
   seeds = 0 ;
   nseeds = 0 ;
   if (!affinities || !row || !scale)
      {
      FrNoMemory("while building affinity matrix for spectral clustering") ;
      FrFree(affinities) ;
      FrLocalFree(row) ;
      FrLocalFree(scale) ;
      return 0 ;
      }
   size_t i(0) ;
   for (const FrList *v1 = vectors ; v1 ; v1 = v1->rest(), i++)
      {
      FrTermVector *tv1 = (FrTermVector*)v1->first() ;
      // while we're scanning down the list anyway, collect the seeds
      if (tv1->cluster())
	 {
	 pushlist(tv1,seeds) ;
	 nseeds++ ;
	 }
      size_t j = i ;
      for (const FrList *v2 = v1 ; v2 ; v2 = v2->rest(), j++)
	 {
	 FrTermVector *tv2 = (FrTermVector*)v2->first() ;
	 double sim = FrTermVecSimilarity(tv1,tv2,params->measure(),true,
					  params->tvSimFn()) ;
	 // ensure that the similarity is in the range [0..1]
	 if (sim < 0.0) sim = 0.0 ;
	 // convert similarity scores to distance values
	 double dist = 1.0 / (sim + EPSILON) ;
	 affinity(i,j) = dist ;
	 affinity(j,i) = dist ;
	 }
      }
   // create a mapping from the seed list to the original list of vectors
   seedIndex = FrNewN(size_t,nseeds) ;
   if (!seedIndex)
      {
      FrNoMemory("while allocating mapping from seed vector to original position") ;
      }
   nseeds = 0 ;
   i = 0 ;
   for (const FrList *v1 = vectors ; v1 ; v1 = v1->rest(), i++)
      {
      FrTermVector *tv1 = (FrTermVector*)v1->first() ;
      // check whether this vector is a seed, and if so, add it to the mapping
      if (tv1->cluster())
	 seedIndex[nseeds++] = i ;
      }
   // compute the local scaling factors
   size_t nth = params->alpha()>=1.0 ? (size_t)params->alpha() : DEFAULT_NTH ;
   if (nth > numvectors / 2)
     nth = numvectors / 2 ;
#ifdef DEBUG
   bool show_scaling_factors = run_verbosely ;
#else
   bool show_scaling_factors = false ;
#endif
   if (show_scaling_factors)
      cout << ";  scaling factors:\n;" ;
   for (i = 0 ; i < numvectors ; i++)
      {
      // find the nth-nearest neighbor
      for (size_t j = 0 ; j < numvectors ; j++)
 	 row[j] = affinity(i,j) ;
      qsort(row,numvectors,sizeof(row[0]),compare_double) ;
      scale[i] = row[nth] ;
      if (show_scaling_factors)
	 {
	 cout << '\t' << scale[i] ;
	 if (i % 8 == 7)
	    cout << "\n; " ;
	 }
      }
   FrLocalFree(row) ;
   if (show_scaling_factors)
      cout << endl ;
   // apply local scaling, then exponentiate to generate affinity values
   for (i = 0 ; i < numvectors ; i++)
      {
      // scale the elements of the row
      affinity(i,i) = 1.0 ; 	// by definition
      for (size_t j = i+1 ; j < numvectors ; j++)
	 {
	 double dist = affinity(i,j) ;
	 double aff = exp(-dist * dist / scale[i] / scale[j]) ;
	 affinity(i,j) = aff ;
	 affinity(j,i) = aff ;
	 }
      }
#ifdef SHOW_AFFINITY
   cout << "Affinity matrix:" << endl ;
   i = 0 ;
   for (const FrList *v = vectors ; v ; v = v->rest(), i++)
      {
      const FrTermVector *tv = (FrTermVector*)v->first() ;
      cout << tv->key() << " =" ;
      for (size_t j = 0 ; j < numvectors ; j++)
	 cout << ' ' << affinity(i,j) ;
      cout << endl ;
      }
#endif /* SHOW_AFFINITY */
   if (run_verbosely)
      cout <<";  [ creating affinity matrix took " << timer.readsec()
	   << " seconds ]"<<endl;
   FrLocalFree(scale) ;
   return affinities ;
}

//----------------------------------------------------------------------

static void compute_S_times_D(size_t numvectors, double *affinities,
			      bool run_verbosely)
{
   FrTimer timer ;
//D^-1/2 S D^-1/2
   FrLocalAlloc(double,diag,MAX_STACK_BUF,numvectors) ;
   for(size_t k = 0 ; k < numvectors ; k++)
      {
      double sum = 0.0 ;
      for(size_t j = 0 ; j < numvectors ; j++)
	 {
	 sum += affinity(j,k) ;
	 }
      if (sum <= 0.0) sum = EPSILON ;
#ifdef USE_SQRT
      sum = sqrt(sum) ;
#endif
      diag[k] = 1.0 / sum ;
      }
   for (size_t j = 0 ; j < numvectors ; j++)
      {
      for (size_t k = 0 ; k < numvectors ; k++)
	 {
#ifdef USE_SQRT
	 affinity(j,k) *= diag[j] * diag[k] ;
#else
	 affinity(j,k) *= diag[k] ;
#endif
	 }
      }
   if (run_verbosely)
      cout << ";  [ computing S*D took " << timer.readsec() << " seconds ]"
	   << endl ;
   FrLocalFree(diag) ;
   return ;
}

//----------------------------------------------------------------------

static void adjust_seed_affinities(size_t numvectors, const FrList *seeds,
				   const size_t *seedIndex,double *affinities,
				   bool run_verbosely)
{
   if (!seeds)
      return ;
   FrTimer timer ;
#ifdef DEBUG
//RASHMI
   int nseed = seeds->simplelistlength() ;
   cout<<" number of seeds = "<< nseed << endl; //RASHMI
   int newi=0;
   for (const FrList *vm = seeds ; vm ; vm = vm->rest(),newi++)
      {
      FrTermVector *tv1 = (FrTermVector*)vm->first() ;
      FrSymbol *clustername = tv1->cluster() ;
      cout << "Cluster name:  " << clustername << endl;	
      cout << "Seed contents: " << tv1->key() << endl;
      cout << "Seed index into matrix: "<<seedIndex[newi]<<endl;
      }
 //RASHMI
#endif /* DEBUG */

   size_t i = 0 ;
   for (const FrList *s1 = seeds ; s1 ; s1 = s1->rest(), i++)
      {
      const FrTermVector *tv1 = (FrTermVector*)s1->first() ;
      size_t j = 0 ;
      for (const FrList *s2 = seeds ; s2 ; s2 = s2->rest(), j++)
	 {
	 const FrTermVector *tv2 = (FrTermVector*)s2->first() ;
	 if (tv1 != tv2)
	    {
	    size_t v1 = seedIndex[i] ;
	    size_t v2 = seedIndex[j] ;
	    if (tv1->cluster() == tv2->cluster())
	       {
	       // define perfect affinity between members of each seed cluster
	       affinity(v1,v2) = 1.0 ;
	       }
	    else
	       {
	       // define NO affinity between members of different seed clusters
	       affinity(v1,v2) = EPSILON ;
	       }
	    }
	 }
      }
   if (run_verbosely)
      cout << ";  [ adjusting affinities between seed clusters took "
	   << timer.readsec() << " seconds ]" << endl ;
   return ;
}

//----------------------------------------------------------------------

static void compute_eigenvalues(size_t numclusters, size_t numvectors,
				double *affinities, Matrix &uu,
				bool run_verbosely)
{
   FrTimer timer ;

//power method for eigen values//
   size_t it,kh;
   double eigenvalue ;
   double sump;
   FrLocalAlloc(double,dummx,MAX_STACK_BUF,numvectors) ;
   FrLocalAlloc(double,xnew,MAX_STACK_BUF,numvectors) ;
   if (!dummx || !xnew)
      {
      FrLocalFree(dummx) ;
      FrLocalFree(xnew) ;
      FrNoMemory("while computing eigenvectors") ;
      return ;
      }
   for(size_t ty = 0 ; ty < numclusters ; ty++)
      {
#ifdef RANDOMIZE_EIGEN_INIT
      for(kh = 0 ; kh < numvectors ; kh++)
	 {
         dummx[kh]=FrRandomNumber(10.0) ;
//          cout << dummx[kh] << endl;
	 }
      sump=0;
      for(kh = 0 ; kh < numvectors ; kh++)
	 {
	 sump=sump+(dummx[kh]*dummx[kh]);
	 }
      sump = sqrt(sump) ;
      for(kh = 0 ; kh < numvectors ; kh++)
	 {
	 dummx[kh]=dummx[kh]/sump;
	 }
#else
      for (kh = 0 ; kh < numvectors ; kh++)
	 dummx[kh] = 1.0 ;
#endif /* RANDOMIZE_EIGEN_INIT */
      for(it=1;it<=100;it++)
	 {
	 // compute xnew = affinities * dummx ;
	 for (size_t j = 0 ; j < numvectors ; j++)
	    {
	    double sum = 0.0 ;
	    for (size_t k = 0 ; k < numvectors ; k++)
	       {
	       sum += affinity(j,k) * dummx[k] ;
	       }
	    xnew[j] = sum ;
	    }
	 sump=0;
	 for(kh = 0 ; kh < numvectors ; kh++)
	    {
	    sump=sump+(dummx[kh]*xnew[kh]);
	    }
	 eigenvalue=sump;
	 sump=0;
	 for(kh = 0 ; kh < numvectors ; kh++)
	    {
	    sump=sump+(xnew[kh]*xnew[kh]);
	    }
	 sump = sqrt(sump) ;
	 for(kh = 0 ; kh < numvectors ; kh++)
	    {
	    xnew[kh] /= sump ;
	    }
	 int sigg;
	 if(eigenvalue>0)
	    {
	    sigg=1;
	    }
	 else if(eigenvalue<0)
	    {
	    sigg=-1;
	    }
	 else
	    {
	    sigg=0;
	    }
	 sump=0;
	 double delta;
	 for(kh = 0 ; kh < numvectors ; kh++)
	    {
	    delta=dummx[kh]-sigg*xnew[kh];
	    sump=sump+(delta*delta);
	    }
	 //delta = dummx - sigg*xnew;
	 //  err = sqrt ( sum( delta.t() .* delta ) );
	 double err=sqrt(sump);
	 if(err<0.001)
	    {
	    break;
	    }
	 else
	    {
	    // dummx = xnew
	    for (kh=0 ; kh < numvectors ; kh++)
	       dummx[kh] = xnew[kh] ;
	    }
	 }

#ifdef DEBUG
      //cout<<"eigen value "<<ty<<" = "<<eigenvalue<<endl;
      //cin>>kh;
      FILE *f1=fopen("eig_values_rashmi","a");
      fprintf(f1,"%f\n",eigenvalue);
      fclose(f1);
#endif /* DEBUG */

      for (kh = 0 ; kh < numvectors ; kh++)
	 {
	 double xnew_kh = xnew[kh] ;
	 uu.element(kh,ty) = xnew_kh ;
	 double d_kh = eigenvalue * xnew_kh ;
	 for (it = 0 ; it < numvectors ; it++)
	    {
	    affinity(kh,it) -= (d_kh * xnew[it]) ;
	    }
	 }
      }
   FrLocalFree(dummx) ;
   FrLocalFree(xnew) ;
//end of power method for eigen values//

   // normalize the uu matrix
   for(size_t k = 0 ; k < numvectors ; k++)
      {
      double sum = 0.0 ;
      for(size_t j = 0 ; j < numclusters ; j++)
	 {
	 double value = uu.element(k,j) ;
	 sum += (value*value) ;
	 }
      // if the sum of squares is zero, all the elements of the column are
      //   already zero, so only bother scaling the values if the sum is
      //   nonzero
      if (sum > 0.0)
	 {
	 sum = sqrt(sum);
	 for(size_t j = 0 ; j < numclusters ; j++)
	    {
	    uu.element(k,j) /= sum ;
	    }
	 }
      }

   if (run_verbosely)
      cout << ";  [ computing normalized eigenvalues took "
	   << timer.readsec() << " seconds ]" << endl ;
   return ;
}

//----------------------------------------------------------------------

static void initialize_centers(size_t numclusters, size_t numvectors,
			       const Matrix &uu, Matrix &kcenter,
			       FrClusteringMeasure sim_measure,
			       bool orthogonal)
{
   FrLocalAllocC(bool,used,MAX_STACK_BUF,numvectors) ;
   if (orthogonal)
      {
      size_t k = FrRandomNumber(numvectors) ;
      kcenter.setRow(0,uu.row(k)) ;
      used[k] = true ;
      for (size_t pt = 1 ; pt < numclusters ; pt++)
	 {
	 int min_index = 0 ;
	 double min_value = DBL_MAX ;
	 for(size_t vec = 0 ; vec < numvectors ; vec++)
	    {
	    if (used[vec])
	       continue ;		// no need to check this vector
	    const double *row = uu.row(vec) ;
	    // find the nearest center to the vector under consideration
	    double value = -1.0 ;
	    for (size_t center = 0 ; center < pt ; center++)
	       {
	       double sim = FrVectorSimilarity(sim_measure,row,
					       kcenter.row(center),
					       kcenter.columns(),false) ;
	       if (sim > value)
		  value = sim ;
	       }
	    // if the nearest center is more distant than for any previously-
	    //   processed vector, remember this one
	    if (value < min_value)
	       {
	       min_value = value ;
	       min_index = vec ;
	       }
	    }
	 used[min_index] = true ;
	 kcenter.setRow(pt,uu.row(min_index)) ;
	 }
      }
   else
      {
      for (size_t k = 0 ; k < numclusters ; k++)
	 {
	 size_t ran_index ;
	 // find a random vector which hasn't been used yet
	 do {
	    ran_index = FrRandomNumber(numvectors) ;
	    } while (used[ran_index]) ;
	 used[ran_index] = true ;
	 kcenter.setRow(k,uu.row(ran_index)) ;
	 }
      }
   FrLocalFree(used) ;
   return ;
}

//----------------------------------------------------------------------

static void assign_to_nearest(size_t numclusters, size_t numvectors,
			      const Matrix &kcenter, const Matrix &uu,
			      FrClusteringMeasure sim_measure,
			      unsigned int *cluster_assignment)
{
   for(size_t kj = 0 ; kj < numvectors ; kj++)
      {
      const double *vec = uu.row(kj);
      double max_sim = -DBL_MAX ;
      int index = 0 ;
      for(size_t ik = 0 ; ik < numclusters ; ik++)
	 {
	 const double *center = kcenter.row(ik) ;
	 double sim = FrVectorSimilarity(sim_measure,vec,center,numclusters,
					 false) ;
	 if (sim > max_sim)
	    {
	    max_sim = sim ;
	    index = ik ;
	    }
	 }
      cluster_assignment[kj]=index ;
      }
   return ;
}

//----------------------------------------------------------------------

static bool update_centroids(size_t numclusters, size_t numvectors, Matrix &kcenter,
			       const Matrix &uu, double &diff,
			       const unsigned int *cluster_assignment)
{
   diff=0.0;
   FrLocalAllocC(double,newcentroid,MAX_STACK_BUF,numclusters) ;
   for(size_t ik = 0 ; ik < numclusters ; ik++)
      {
      size_t murcount = 0 ;
      for(size_t kj = 0 ; kj < numvectors ; kj++)
	 {
	 if(cluster_assignment[kj]==ik)
	    {
	    murcount++;
	    const double *vec = uu.row(kj) ;
	    for (size_t i = 0 ; i < numclusters ; i++)
	       newcentroid[i] += vec[i] ;
	    }
	 }
      if (murcount>0)
	 {
	 for (size_t i = 0 ; i < numclusters ; i++)
	    newcentroid[i] /= murcount;
	 }
      const double *center = kcenter.row(ik) ;
      for (size_t i = 0 ; i < numclusters ; i++)
	 {
	 diff += fabs(center[i] - newcentroid[i]) ;
	 }
      kcenter.setRow(ik,newcentroid) ;
      }
   FrLocalFree(newcentroid) ;
   return (!isnan(diff) && diff < THRESHOLD) ;
}

//----------------------------------------------------------------------

/* Spectral clustering main function */
void Fr__cluster_spectral(const FrList *vectors, FrSymHashTable *clusters,
			  const FrClusteringParameters *params,
			  bool run_verbosely)
{
   if (!vectors)
      return ;			// nothing to cluster

   FrTimer timer ;
   size_t numvectors ;
   FrList *seeds = 0 ;
   // create the affinity matrix
   size_t nseeds ;
   size_t *seedIndex = 0 ; // will be allocated in compute_affinities()
   double *affinities = compute_affinities(vectors,params,numvectors,
					   seeds,run_verbosely,seedIndex,
					   nseeds) ;
   if (!affinities)
      {
      FrFree(seedIndex) ;
      return ;
      }

   // compute eigenvalues and use them to decide on clusters

/////////////RASHMI
   size_t numclusters = (int)params->desiredClusters() ;
   // the clustering is likely to hang in initialize_centers if we request
   //   nearly as many clusters as we have vectors, so limit the number of
   //   clusters if necessary
   if (numclusters > numvectors / 2)
      numclusters = numvectors / 2 ;

   adjust_seed_affinities(numvectors,seeds,seedIndex,affinities,run_verbosely);
   seeds->eraseList(false) ; 	// shallow erase of list of seeds
   FrFree(seedIndex) ;
   compute_S_times_D(numvectors,affinities,run_verbosely) ;

   Matrix uu(numvectors,numclusters);
   if (!uu.good())
      {
      FrNoMemory("while allocating matrix for eigenvectors") ;
      return ;
      }
   compute_eigenvalues(numclusters,numvectors,affinities,uu,run_verbosely) ;
   FrFree(affinities) ;

   double min_distortion=DBL_MAX;
   FrLocalAllocC(unsigned int,best_cluster_assignment,MAX_STACK_BUF,numvectors) ;
   FrLocalAllocC(unsigned int,cluster_assignment,MAX_STACK_BUF,numvectors) ;
   Matrix kcenter(numclusters,numclusters);
   size_t num_ortho = params->maxIterations() ;
   size_t num_random = params->maxIterations() ;
   for(size_t j=1 ; j <= (num_random+num_ortho) ; j++)
      {
      if (run_verbosely)
	 printf(";  Iteration %u (%s)\n",
		(unsigned)j,(j<=num_ortho)?"ortho":"random");
      initialize_centers(numclusters,numvectors,uu,kcenter,
			 params->measure(),j<=num_ortho) ;
      //kmeans_nostat
      bool converged=false;
      double diff ;
      size_t iter;
      for(iter = 1 ; iter <= MAX_ITER && !converged ; iter++)
	 {
	 // assign each point to the nearest centroid
	 assign_to_nearest(numclusters,numvectors,kcenter,uu,
			   params->measure(),cluster_assignment) ;
	 // compute the new centroid of each cluster and track cumulative
	 //   differences from the previous iteration
	 converged = update_centroids(numclusters,numvectors,kcenter,uu,
				      diff,cluster_assignment) ;
	 }

      // compute the global distortion (squared error) for the current
      //   cluster assignment
      double distortion=0.0;
      for(size_t vec = 0 ; vec < numvectors ; vec++)
	 {
	 int clus = cluster_assignment[vec] ;
	 for(size_t eigenvec = 0 ; eigenvec < numclusters ; eigenvec++)
	    {
	    double d = (uu.element(vec,eigenvec) -
			kcenter.element(clus,eigenvec)) ;
	    distortion += d * d ;
	    }
	 }
		
      if (run_verbosely)
	 {
	 printf(";    K-means %s. diff=%f iter=%u distort=%g min=%g\n",
		(converged?"done":"ended"),diff,
		(unsigned)iter,distortion,min_distortion) ;
	 }
      if (distortion<min_distortion)
	 {
	 memcpy(best_cluster_assignment,cluster_assignment,
		sizeof(cluster_assignment[0])*numvectors) ;
	 min_distortion=distortion;
	 }
      }
   FrLocalFree(cluster_assignment) ;
#ifdef DEBUG
   cout << "Printing assignments\n";
   for(size_t ik = 0 ; ik < numvectors ; ik++)
      {
      cout<<ik << " " << best_cluster_assignment[ik]<<endl;
      }
   cout<<endl;
#endif /* DEBUG */

   // output the final results of the clustering
   for (size_t clus = 0 ; clus < numclusters ; clus++)
      {
      FrSymbol *clustername = 0 ;
      // scan for a seed vector in the current cluster; if found, use its
      //   cluster name as the current cluster's name
      const FrList *v1 = vectors ;
      for (size_t i = 0 ; i < numvectors ; i++, v1 = v1->rest())
	 {
	 if (best_cluster_assignment[i] == clus)
	    {
	    FrTermVector *tv =  (FrTermVector*)v1->first();
	    if (tv->cluster())
	       {
	       clustername = tv->cluster() ;
	       break ;
	       }
	    }
	 }
      // if no seed found, generate a new cluster name
      if (!clustername)
	 clustername = gen_cluster_sym();
      // now output the members of the current cluster
      v1 = vectors ;
      for (size_t pt = 0 ; pt < numvectors ; pt++, v1 = v1->rest())
	 {
	 if (best_cluster_assignment[pt] == clus)
	    {
	    FrTermVector *tv =  (FrTermVector*)v1->first();
	    Fr__insert_in_cluster(clusters,clustername,tv,params);
	    }
	 }
      }
   FrLocalFree(best_cluster_assignment) ;

   if (run_verbosely)
      cout << ";  [ spectral clustering took " << timer.readsec()
	   << " seconds ]" << endl ;
   return ;
}

// end of file frclust5.cpp //
