/************************************************************************/
/*									*/
/*  MikroKARAT Inverse Indexer						*/
/*  Version 0.96  							*/
/*     by Henry Robertson                                               */
/*									*/
/*  File inv.h	  	inverse indexing routines			*/
/*  LastEdit: 04nov09 by Ralf Brown					*/
/*									*/
/*  (c) Copyright 1994,1995,1996 Center for Machine Translation		*/
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

// Inverse Indexer header file
// by Henry Robertson
// Pangloss Project
// When planning to use the Inverse Indexer funcs, first call init_ii().

#include <string.h>
#include "mikro_db.h"

//----------------------------------------------------------------------

#define FILLER_VALUES 0  // to sort according to VAL fields
#define SLOT_NAMES    1  // to sort according to slot names of the frame
#define STRING_WORDS  2  // to sort according to text strings in frame


void init_ii(void);
void open_index(int fd, int maxsize = 0) ;
void close_index(int fd) ;

// SEARCH FUNCTIONS

// fetch_names: does an exclusive search of frame names, so that
// if the list (A B C) is passed, only the frames which
// have the values A, B, and C exactly are returned.
// Frames with (A B) or (A B C D) will not be returned.
// int 'fd' for the data file with the frame names;
// List* is the list of key values;
// int is 0,1, or 2 (see define clauses above)

FrList* fetch_names(int fd, FrList*, int);

// inclusive_fetch: fetches all frames names that have one or more of
// the elements in the list.  Thus, if the list (B C) is passed,
// all frame names with values of B or C are returned.

FrList* inclusive_fetch(int fd, FrList*, int);


// UPDATER FUNCTIONS

int update_inv(int, int fd, FrFrame *, FrFrame *);
int delete_frame(FrFrame*, int, int fd);
int add_frame(FrFrame*, int, int fd);
int defragmenter(char*);
int make_new_file(int fd) ;

// end of file inv.h //
