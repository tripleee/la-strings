/****************************** -*- C++ -*- *****************************/
/*									*/
/*  FramepaC								*/
/*  Version 1.98							*/
/*	by Ralf Brown <ralf@cs.cmu.edu>					*/
/*									*/
/*  File: frclust4.cpp	      Seed-Growing main code			*/
/*  LastEdit: 04nov09							*/
/*									*/
/*  (c) Copyright 2005,2009 Ralf Brown					*/
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

/************************************************************************/
/************************************************************************/

static FrTermVector *nearest_seed(FrTermVector *vector, const FrList *seeds,
				  const FrClusteringParameters *params)
{
   FrTermVector *cluster = 0 ;
   double best_sim = -999.9 ;
   double threshold = params->threshold(0) ;
   FrClusteringMeasure measure = params->measure() ;
   for ( ; seeds ; seeds = seeds->rest())
      {
      FrTermVector *seed = (FrTermVector*)seeds->first() ;
      double sim = FrTermVecSimilarity(vector,seed,measure,params->tvSimFn()) ;
      if (sim >= threshold && sim > best_sim)
	 {
	 best_sim = sim ;
	 cluster = seed ;
	 }
      }
   return cluster ;
}

//----------------------------------------------------------------------

/* Grow-Seed clustering main function */
void Fr__cluster_growseed(const FrList *vectors, FrSymHashTable *clusters,
			  const FrClusteringParameters *params,
			  bool run_verbosely)
{
   FrList *seeds = 0 ;
   for (const FrList *v = vectors ; v ; v = v->rest())
      {
      FrTermVector *vector = (FrTermVector*)v->first() ;
      FrSymbol *clustername = vector->cluster() ;
      if (clustername)
	 {
	 pushlist(vector,seeds) ;
	 // request centroid for a new cluster, nearest for existing cluster
	 //   so that the "centroid" is in fact the first (seed) vector
	 FrClusteringRep rep = (clusters->lookup(clustername)
				? FrCR_NEAREST
				: FrCR_CENTROID) ;
	 Fr__add_to_cluster(clusters,clustername,vector,rep,
			    params->cacheSize()) ;
	 }
      }
   if (run_verbosely)
      cout << ";    " << seeds->listlength() << " seed vectors" << endl ;
   for ( ; vectors ; vectors = vectors->rest())
      {
      FrTermVector *vector = (FrTermVector*)vectors->first() ;
      if (!vector->cluster())
	 {
	 FrTermVector *cluster = nearest_seed(vector,seeds,params) ;
	 FrSymbol *clustername = cluster ? cluster->cluster() : 0 ;
	 vector->setCluster(clustername) ;
	 if (clustername)
	    {
	    // request centroid for a new cluster, nearest for existing cluster
	    //   so that the "centroid" is in fact the first (seed) vector
	    FrClusteringRep rep = (clusters->lookup(clustername)
				   ? FrCR_NEAREST
				   : FrCR_CENTROID) ;
	    Fr__add_to_cluster(clusters,clustername,vector,rep,
			       params->cacheSize()) ;
	    }
	 }
      }
   seeds->eraseList(false) ;		// shallow erase, don't kill vectors
   return ;
}

// end of file frclust4.cpp //
