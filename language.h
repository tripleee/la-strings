/****************************** -*- C++ -*- *****************************/
/*                                                                      */
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     language.h						*/
/*  Version:  1.16							*/
/*  LastEdit: 26jun2012							*/
/*                                                                      */
/*  (c) Copyright 2010,2011,2012 Ralf Brown/Carnegie Mellon University	*/
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

#ifndef __LANGUAGE_H_INCLUDED
#define __LANGUAGE_H_INCLUDED

enum OutputFormat
   {
      OF_Native,
      OF_UTF8,
      OF_UTF16LE,
      OF_UTF16BE
   } ;

class CharacterSet ;

class CodePoints
   {
   private:
      size_t	m_numpoints ;
      uint32_t *m_points ;
   public:
      CodePoints(size_t numpoints) ;
      CodePoints(const CharacterSet *set, const char *language) ;
      CodePoints(const CharacterSet *set, const char *language_spec,
		 bool is_filename) ;
      ~CodePoints() ;

      // configuration
      void setPoint(size_t cp) ;
      void clearPoint(size_t cp) ;
      bool setValidPoints(const char *enc, const char *language,
			  const char *range_spec) ;

      // accessors
      bool validPoint(size_t cp) const ;
   } ;

#endif /* !__LANGUAGE_H_INCLUDED */

/* end of file language.h */
