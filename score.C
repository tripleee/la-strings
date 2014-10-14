/****************************** -*- C++ -*- *****************************/
/*                                                                      */
/*	LA-Strings: language-aware text-strings extraction		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     score.C							*/
/*  Version:  1.14							*/
/*  LastEdit: 12mar2012							*/
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

#include <cmath>
#include "charset.h"
#include "score.h"
#include "langident/langid.h"

/************************************************************************/
/*	Manifest Constants						*/
/************************************************************************/

#define DICT_WEIGHT 2.0

/************************************************************************/
/*	Helper functions						*/
/************************************************************************/

static double weight_alpha(double alpha)
{   
   if (alpha > 1.0)
      return alpha * alpha ;
   else
      return 0.0 ;
}

//----------------------------------------------------------------------

static double weight_desired(double desired)
{
   if (desired > 1.0)
      return desired * desired ;
   else
      return 0.0 ;
}

/************************************************************************/
/*	Methods for class StringScore					*/
/************************************************************************/

StringScore::StringScore()
{
   m_total_chars = 0 ;
   m_total_alpha = 0 ;
   m_total_desired = 0 ;
   m_wordcover = 0 ;
   m_alpha_run = 0 ;
   m_desired_run = 0 ;
   m_other_run = 0 ;
   m_weighted_runs = 0.0 ;
   m_logprobability = 0.0 ;
   m_language_score = -999.9 ;
   m_have_dictionary = false ;
   return ;
}

//----------------------------------------------------------------------

void StringScore::update(const CharacterSet *charset, wchar_t codepoint,
			 int charsize)
{
   m_total_chars += charsize ;
   bool is_alpha = charset->isAlphaNum(codepoint) ;
   if (is_alpha)
      m_total_alpha += charsize ;
   if (is_alpha || codepoint == ' ' || codepoint == '\t')
      m_alpha_run += charsize ;
   else
      {
      m_weighted_runs += weight_alpha(m_alpha_run) ;
      m_alpha_run = 0 ;
      }
   bool is_desired = charset->desiredCodePoint(codepoint) ;
   if (is_desired)
      m_total_desired += charsize ;
   if (is_desired || codepoint == ' ' || codepoint == '\t')
      {
      m_desired_run += charsize ;
      m_other_run = 0 ;
      }
   else
      {
      m_weighted_runs += weight_desired(m_desired_run) ;
      m_desired_run = 0 ;
      m_other_run += charsize ;
      }
   return ;
}

//----------------------------------------------------------------------

void StringScore::addWord(int wordlength)
{
   m_wordcover += wordlength ;
   return ;
}

//----------------------------------------------------------------------

void StringScore::setLanguageScore(const LanguageScores *scores)
{
   if (scores)
      m_language_score = scores->highestScore() ;
   return ;
}

//----------------------------------------------------------------------

void StringScore::finalize()
{
   m_weighted_runs += weight_alpha(m_alpha_run) ;
   m_weighted_runs += weight_desired(m_desired_run) ;
   m_alpha_run = 0 ;
   m_desired_run = 0 ;
   m_other_run = 0 ;
   return ;
}

//----------------------------------------------------------------------

double StringScore::alphaPercent() const
{
   return m_total_alpha / (double)m_total_chars ;
}

//----------------------------------------------------------------------

double StringScore::desiredPercent() const
{
   return m_total_desired / (double)m_total_chars ;
}

//----------------------------------------------------------------------

double StringScore::wordCoverage() const
{
   return (m_total_chars) ? m_wordcover / (double)m_total_chars : 0.0 ;
}

//----------------------------------------------------------------------

double StringScore::logProbability() const
{
   return (m_total_chars) ? m_logprobability / (double)m_total_chars : 0.0 ;
}

//----------------------------------------------------------------------

double StringScore::weightedRuns() const
{
   return m_weighted_runs / totalChars() / totalChars() ;
}

//----------------------------------------------------------------------

double StringScore::computeScore() const
{
   double score = 0.0 ;
   // use an appropriate scale factor such that scores < 10 are probably
   //   spurious extractions rather than real text
   double scale = 2.0 ;
   if (m_have_dictionary)
      {
      // we want strings to have at least 50% known words by character count,
      //   but we also want to stretch the scale to more easily distinguish
      //   between those cases (especially since they are correlated with
      //   the run lengths), so we square the scaled value
      double sc = 2.0 * wordCoverage() ;
      score += DICT_WEIGHT * (sc * sc) ;
      }
   else
      scale += DICT_WEIGHT ;
   score += (scale * weightedRuns()) ;
   // add in a factor for length, since longer strings are less likely to
   //   be spurious
   score *= (0.5 * sqrt(totalChars())) ;
   // prevent the score from going negative
   if (score < 0.0)
      score = 0.0 ;
   // if we have a language-model score, make its scaled value half of
   //   the string's overall score
   if (m_language_score >= 0.0)
      {
      score = (score + (8.0 * m_language_score)) / 2.0 ;
      }
   // cap the score just in case it goes really high, to keep the
   //   output nicely formatted
   if (score > 99.999)
      score = 99.999 ;
   return score ;
}

// end of file score.C //
