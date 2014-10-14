/****************************** -*- C++ -*- *****************************/
/*                                                                      */
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     score.h							*/
/*  Version:  1.09							*/
/*  LastEdit: 02dec2011							*/
/*                                                                      */
/*  (c) Copyright 2010,2011 Ralf Brown/Carnegie Mellon University	*/
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

#ifndef __SCORE_H_INCLUDED
#define __SCORE_H_INCLUDED

class StringScore
   {
   private:
      unsigned m_total_chars ;
      unsigned m_total_alpha ;
      unsigned m_total_desired ;
      unsigned m_wordcover ;
      unsigned m_alpha_run ;
      unsigned m_desired_run ;
      unsigned m_other_run ;
      double   m_logprobability ;
      double   m_weighted_runs ;
      double   m_language_score ;
      bool     m_have_dictionary ;
   public:
      StringScore() ;
      ~StringScore() {}

      void haveDictionary() { m_have_dictionary = true ; }
      void update(const class CharacterSet *, wchar_t codepoint,
		  int charsize = 1) ;
      void addWord(int wordlength) ;
      void setLanguageScore(const class LanguageScores *scores) ;
      void finalize() ;

      // statistics
      unsigned totalChars() const { return m_total_chars ; }
      double alphaPercent() const ;
      double desiredPercent() const ;
      double wordCoverage() const ;
      double logProbability() const ;
      double weightedRuns() const ;
      double computeScore() const ;
      double languageScore() const { return m_language_score ; }
      unsigned desiredRun() const { return m_desired_run ; }
      unsigned undesiredRun() const { return m_other_run ; }
   } ;

#endif /* !__SCORE_H_INCLUDED */

/* end of file score.h */
