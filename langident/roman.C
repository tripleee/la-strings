/************************************************************************/
/*                                                                      */
/*	LangIdent: long n-gram-based language identification		*/
/*	by Ralf Brown / Carnegie Mellon University			*/
/*									*/
/*  File:     roman.C							*/
/*  Version:  1.16							*/
/*  LastEdit: 26jun2012							*/
/*                                                                      */
/*  (c) Copyright 2011,2012 Ralf Brown/Carnegie Mellon University	*/
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

#include <stdint.h>
#include "FramepaC.h"
#include "roman.h"

/************************************************************************/
/*	Types local to this module					*/
/************************************************************************/

struct ISO9Element
   {
      uint16_t codepoint1, codepoint2 ;
   } ;

/************************************************************************/
/************************************************************************/

#ifndef lengthof
#  define lengthof(x) (sizeof(x)/sizeof((x)[0]))
#endif /* lengthof */

/************************************************************************/
/*	Global data for this module					*/
/************************************************************************/

// romanization information for Greek character codepoints
#define ISO843_TABLE_START 0x037C
static const ISO9Element ISO843_table[] =
   {
      { 0, 0 }, { 0, 0 }, { '?', 0 }, { 0, 0 },				// 0x037C-0x037F
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x0380-0x0383
      { 0x00B4, 0 }, { 0, 0 }, { 0x00C1, 0 }, { 0x00B7, 0 },		// 0x0384-0x0387
      { 0x00C9, 0 }, { 0x012A, 0x0301 }, { 0x00CD, 0 }, { 0, 0 },	// 0x0388-0x038B
      { 0x00D3, 0 }, { 0, 0 }, { 0x00DD, 0 }, { 0x1E52, 0 },		// 0x038C-0x038F
      { 0x1E2F, 0 }, { 'A', 0 }, { 'V', 0 }, { 'G', 0 },		// 0x0390-0x0393
      { 'D', 0 }, { 'E', 0 }, { 'Z', 0 }, { 0x012A, 0 },		// 0x0394-0x0397
      { 'T', 'h' }, { 'I', 0 }, { 'K', 0 }, { 'L', 0 },			// 0x0398-0x039B
      { 'M', 0 }, { 'N', 0 }, { 'X', 0 }, { 'O', 0 },			// 0x039C-0x039F
      { 'P', 0 }, { 'R', 0 }, { 0, 0 }, { 'S', 0 },			// 0x03A0-0x03A3
      { 'T', 0 }, { 'Y', 0 }, { 'F', 0 }, { 'C', 'h' },			// 0x03A4-0x03A7
      { 'P', 's' }, { 0x014C, 0 }, { 0x00CF, 0 }, { 0x0178, 0 },	// 0x03A8-0x03AB
      { 0x00E1, 0 }, { 0x00E9, 0 }, { 0x012B, 0x0301 }, { 0x00ED, 0 },	// 0x03AC-0x03AF
      { 0x00FF, 0x0301 }, { 'a', 0 }, { 'v', 0 }, { 'g', 0 },		// 0x03B0-0x03B3
      { 'd', 0 }, { 'e', 0 }, { 'z', 0 }, { 0x012B, 0 },		// 0x03B4-0x03B7
      { 't', 'h' }, { 'i', 0 }, { 'k', 0 }, { 'l', 0 },			// 0x03B8-0x03BB
      { 'm', 0 }, { 'n', 0 }, { 'x', 0 }, { 'o', 0 },			// 0x03BC-0x03BF
      { 'p', 0 }, { 'r', 0 }, { 's', 0 }, { 's', 0 },			// 0x03C0-0x03C3
      { 't', 0 }, { 'y', 0 }, { 'f', 0 }, { 'c', 'h' },			// 0x03C4-0x03C7
      { 'p', 's' }, { 0x014D, 0 }, { 0x00EF, 0 }, { 0x00FF, 0 },	// 0x03C8-0x03CB
      { 0x00F3, 0 }, { 0x00FD, 0 }, { 0x1E53, 0 }, { 0, 0 },		// 0x03CC-0x03CF
      { 'v', 0 }, { 't', 'h' }, { 'Y', 0 }, { 'Y', 0x0301 },		// 0x03D0-0x03D3
      { 'Y', 0x0308 }, { 'f', 0 }, { 'p', 0 }, { '&', 0 },		// 0x03D4-0x03D7
   } ;

// romanization information for Unicode codepoints 0x0400 to 0x051F (Cyrillic)
#define ISO9_TABLE_START 0x0400
static const ISO9Element ISO9_table[] =
   {
      { 0, 0 }, { 0x00CB, 0 }, { 0x0110, 0 }, { 0x01F4, 0 },		// 0x0400-0403
      { 0x00CA, 0 }, { 0x1E90, 0 }, { 0x00CC, 0 }, { 0x00CF, 0 },	// 0x0404-0407
      { 'J', 0x030C }, { 'L', 0x0302 }, { 'N', 0x0302 }, { 0x0106, 0 },	// 0x0408-040B
      { 0x1E30, 0 }, { 0, 0 }, { 0x016C, 0 }, { 'D', 0x0302 },		// 0x040C-040F
      { 'A', 0 }, { 'B', 0 }, { 'V', 0 }, { 'G', 0 },			// 0x0410-0413
      { 'D', 0 }, { 'E', 0 }, { 0x017D, 0 }, { 'Z', 0 },		// 0x0414-0417
      { 'I', 0 }, { 'J', 0 }, { 'K', 0 }, { 'L', 0 },			// 0x0418-041B
      { 'M', 0 }, { 'N', 0 }, { 'O', 0 }, { 'P', 0 },			// 0x041C-041F
      { 'R', 0 }, { 'S', 0 }, { 'T', 0 }, { 'Y', 0 },			// 0x0420-0423
      { 'F', 0 }, { 'H', 0 }, { 'C', 0 }, { 0x010C, 0 },		// 0x0424-0427
      { 0x0160, 0 }, { 0x015C, 0 }, { 0x02BA, 0 }, { 'Y', 0 },		// 0x0428-042B
      { 0x02B9, 0 }, { 0x00C8, 0 }, { 0x00DB, 0 }, { 0x00C2, 0 },	// 0x042C-042F
      { 'a', 0 }, { 'b', 0 }, { 'v', 0 }, { 'g', 0 },			// 0x0430-0433
      { 'd', 0 }, { 'e', 0 }, { 0x017E, 0 }, { 'z', 0 },		// 0x0434-0437
      { 'i', 0 }, { 'j', 0 }, { 'k', 0 }, { 'l', 0 },			// 0x0438-043B
      { 'm', 0 }, { 'n', 0 }, { 'o', 0}, { 'p', 0 },			// 0x043C-043F
      { 'r', 0 }, { 's', 0 }, { 't', 0 }, { 'u', 0 },			// 0x0440-0443
      { 'f', 0 }, { 'h', 0 }, { 'c', 0 }, { 0x010D, 0 },		// 0x0444-0447
      { 0x0161, 0 }, { 0x015D, 0 }, { 0x02BA, 0 }, { 'y', 0 },		// 0x0448-044B
      { 0x02B9, 0 }, { 0x00E8, 0 }, { 0x00FB, 0 }, { 0x00E2, 0 },	// 0x044C-044F
      { 0, 0 }, { 0x00EB, 0 }, { 0x0111, 0 }, { 0x01F5, 0 },		// 0x0450-0453
      { 0x00EA, 0 }, { 0x1E91, 0 }, { 0x00EC, 0 }, { 0x00EF, 0 },	// 0x0454-0457
      { 0x01F0, 0 }, { 'l', 0x0302 }, { 'n', 0x0302 }, { 0x0107, 0 },	// 0x0458-045B
      { 0x1E31, 0 }, { 0, 0 }, { 0x016D, 0 }, { 'd', 0x0302 },		// 0x045C-045F
      { 0, 0 }, { 0, 0 }, { 0x011A, 0 }, { 0x011B, 0 },			// 0x0460-0463
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x0464-0467
      { 0, 0 }, { 0, 0 }, { 0x01CD, 0 }, { 0x01CE, 0 },			// 0x0468-046B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x046C-046F
      { 0, 0 }, { 0, 0 }, { 'F', 0x0300 }, { 'f', 0x0300 },		// 0x0470-0473
      { 0x1EF2, 0 }, { 0x1EF3, 0 }, { 0, 0 }, { 0, 0 },			// 0x0474-0477
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x0478-047B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x047C-047F
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x0480-0483
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x0484-0487
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x0488-048B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x048C-048F
      { 'G', 0x0300 }, { 'g', 0x0300 }, { 0x0120, 0 }, { 0x0121, 0 },	// 0x0490-0493
      { 0x011E, 0 }, { 0x011F, 0 }, { 0, 0 }, { 0, 0 },			// 0x0494-0497
      { 0, 0 }, { 0, 0 }, { 0x0136, 0 }, { 0x0137, 0 },			// 0x0498-049B
      { 'K', 0x0302 }, { 'k', 0x0302 }, { 'K', 0x0304 }, { 'k', 0x0304 },	// 0x049C-049F
      { 0x01E8, 0 }, { 0x01E9, 0 }, { 0x0145, 0 }, { 0x0146, 0 },	// 0x04A0-04A3
      { 0x1E44, 0 }, { 0x1E45, 0 }, { 0x1E54, 0 }, { 0x1E55, 0 },	// 0x04A4-04A7
      { 0x00D2, 0 }, { 0x00F2, 0 }, { 0x015E, 0 }, { 0x015F, 0 },	// 0x04A8-04AB
      { 0x0162, 0 }, { 0x0163, 0 }, { 0x00D9, 0 }, { 0x00F9, 0 },	// 0x04AC-04AF
      { 'U', 0x0307 }, { 'u', 0x0307 }, { 0x1E28, 0 }, { 0x1E29, 0 },	// 0x04B0-04B3
      { 'C', 0x0304 }, { 'c', 0x0304 }, { 0x00C7, 0 }, { 0x00E7, 0 },	// 0x04B4-04B7
      { 0x0108, 0 }, { 0x0109, 0 }, { 0x1E24, 0 }, { 0x1E25, 0 },	// 0x04B8-04BB
      { 'C', 0x0306 }, { 'c', 0x0306 }, { 0x00C7, 0x0306 }, { 0x00E7, 0x0306 },	// 0x04BC-04BF
      { 0x2021, 0 }, { 'Z', 0x0306 }, { 'z', 0x0306 }, { 0x1E32, 0 },	// 0x04C0-04C3
      { 0x01E33, 0 }, { 0x013B, 0 }, { 0x013C, 0 }, { 0x0143, 0 },	// 0x04C4-04C7
      { 0x0144, 0 }, { 0x1E46, 0 }, { 0x1E47, 0 }, { 'C', 0x0323 },	// 0x04C8-04CB
      { 'c', 0x0323 }, { 0, 0 }, { 0, 0 }, { 0, 0 },			// 0x04CC-04CF
      { 0x0102, 0 }, { 0x0103, 0 }, { 0x00C4, 0 }, { 0x00E4, 0 },	// 0x04D0-04D3
      { 0x00C6, 0 }, { 0x00E6, 0 }, { 0x0114, 0 }, { 0x0115, 0 },	// 0x04D4-04D7
      { 'A', 0x030B }, { 'a', 0x030B }, { 0x00C0, 0 }, { 0x00E0, 0 },	// 0x04D8-04DB
      { 'Z', 0x0304 }, { 'z', 0x0304 }, { 'Z', 0x0308 }, { 'z', 0x0308 },	// 0x04DC-04DF
      { 0x0179, 0 }, { 0x017A, 0 }, { 0x012A, 0 }, { 0x012B, 0 },	// 0x04E0-04E3
      { 0x00CE, 0 }, { 0x00EE, 0 }, { 0x00D6, 0 }, { 0x00F6, 0 },	// 0x04E4-04E7
      { 0x00D4, 0 }, { 0x00F4, 0 }, { 0x0150, 0 }, { 0x0151, 0 },	// 0x04E8-04EB
      { 0, 0 }, { 0, 0 }, { 0x016A, 0 }, { 0x016B, 0 },			// 0x04EC-04EF
      { 0x00DC, 0 }, { 0x00FC, 0 }, { 0x0170, 0 }, { 0x0171, 0 },	// 0x04F0-04F3
      { 'C', 0x0308 }, { 'c', 0x0308 }, { 0, 0 }, { 0, 0 },		// 0x04F4-04F7
      { 0x0178, 0 }, { 0x00FF, 0 }, { 0, 0 }, { 0, 0 },			// 0x04F8-04FB
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x04FC-04FF
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x0500-0503
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x0504-0507
      { 0, 0 }, { 0, 0 }, { 0x01F8, 0 }, { 0x01F9, 0 },			// 0x0508-050B
      { 0, 0 }, { 0, 0 }, { 'T', 0x0300 }, { 't', 0x0300 },		// 0x050C-050F
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x0510-0513
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },				// 0x0514-0517
      { 0, 0 }, { 0, 0 }, { 'Q', 0 }, { 'q', 0 },			// 0x0518-051B
      { 'W', 0 }, { 'w', 0 }, { 0, 0 }, { 0, 0 }			// 0x051C-051F
   } ;

// romanization table for Armenian
#define ISO9985_TABLE_START 0x0530
static const ISO9Element ISO9985_table[] = 
   {
      { 0, 0 }, { 'A', 0 }, { 'B', 0 }, { 'G', 0 },		// 0x0530-0x0533
      { 'D', 0 }, { 'E', 0 }, { 'Z', 0 }, { 0x0112, 0 },	// 0x0534-0x0537
      { 0x00CB, 0 }, { 'T', '\'' }, { 'Z', 'H' }, { 'I', 0 },	// 0x0538-0x053B
      { 'L', 0 }, { 'X', 0 }, { 0x00C7, 0 }, { 'K', 0 },	// 0x053C-0x053F
      { 'H', 0 }, { 'J', 0 }, { 'G', 'H' }, { 0x010C, 0 },	// 0x0540-0x0543
      { 'M', 0 }, { 'Y', 0 }, { 'N', 0 }, { 0x0160, 0 },	// 0x0544-0x0547
      { 'O', 0 }, { 0x010C, 0 }, { 'P', 0 }, { 0x01F0, 0 },	// 0x0548-0x054B
      { 'R', 'R' }, { 'S', 0 }, { 'V', 0 }, { 'T', 0 },		// 0x054C-0x054F
      { 'R', 0 }, { 'C', '\'' }, { 'W', 0 }, { 'P', '\'' },	// 0x0550-0x0553
      { 'K', '\'' }, { 0x00D2, 0 }, { 'F', 0 }, { 'E', 'W' },	// 0x0554-0x0557
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },			// 0x0558-0x055B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },			// 0x055C-0x055F
      { 0, 0 }, { 'a', 0 }, { 'b', 0 }, { 'g', 0 },		// 0x0560-0x0563
      { 'd', 0 }, { 'e', 0 }, { 'z', 0 }, { 0x0113, 0 },	// 0x0564-0x0567
      { 0x00EB, 0 }, { 't', '\'' }, { 'z', 'h' }, { 'i', 0 },	// 0x0568-0x056B
      { 'l', 0 }, { 'x', 0 }, { 0x00E7, 0 }, { 'k', 0 },	// 0x056C-0x056F
      { 'h', 0 }, { 'j', 0 }, { 'g', 'h' }, { 0x010D, 0 },	// 0x0570-0x0573
      { 'm', 0 }, { 'y', 0 }, { 'n', 0 }, { 's', 'h' },		// 0x0574-0x0577
      { 'o', 0 }, { 'c', 'h' }, { 'p', 0 }, { 'j', 0 },		// 0x0578-0x057B
      { 'r', 'r' }, { 's', 0 }, { 'v', 0 }, { 't', 0 },		// 0x057C-0x057F
      { 'r', 0 }, { 'c', '\'' }, { 'w', 0 }, { 'p', '\'' },	// 0x0580-0x0583
      { 'k', '\'' }, { 0x00F2, 0 }, { 'f', 0 }, { 'e', 'w' },	// 0x0584-0x0587
   } ;

// romanization information for Hebrew
#define ISO259_TABLE_START 0x05B0
static const ISO9Element ISO259_table[] =
   {
      { 0, 0 }, { 'e', 0 }, { 'a', 0 }, { 'o', 0 },		// 0x05B0-0x05B3
      { 'i', 0 }, { 'e', 0 }, { 'e', 0 }, { 'a', 0 },		// 0x05B4-0x05B7
      { 'a', 0 }, { 0, 0 }, { 0, 0 }, { 'u', 0 },		// 0x05B8-0x05BB
      { 'u', 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x05BC-0x05BF
      { 0, 0 }, { 0, 0 }, { 'o', 0 }, { 0, 0 },		// 0x05C0-0x05C3
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x05C4-0x05C7
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x05C8-0x05CB
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x05CC-0x05CF
      { 0x02C0, 0 }, { 'b', 0 }, { 'g', 0 }, { 'd', 0 },		// 0x05D0-0x05D3
      { 'h', 0 }, { 'w', 0 }, { 'z', 0 }, { 0x1E25, 0 },		// 0x05D4-0x05D7
      { 0x1E6F, 0 }, { 'y', 0 }, { 'k', 0 }, { 'k', 0 },		// 0x05D8-0x05DB
      { 'l', 0 }, { 'm', 0 }, { 'm', 0 }, { 'n', 0 },		// 0x05DC-0x05DF
      { 'n', 0 }, { 's', 0 }, { 0x02C1, 0 }, { 'p', 0 },		// 0x05E0-0x05E3
      { 'p', 0 }, { 'c', 0 }, { 'c', 0 }, { 'q', 0 },		// 0x05E4-0x05E7
      { 'r', 0 }, { 0x0161, 0 }, { 't', 0 }, { 0, 0 },		// 0x05E8-0x05EB
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x05EC-0x05EF
   } ;

// romanization information for Arabic
#define ISO233_TABLE_START 0x0600
static const ISO9Element ISO233_table[] = 
   {
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0600-0x0603
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0604-0x0607
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0608-0x060B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x060C-0x060F
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0610-0x0613
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0614-0x0617
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0618-0x061B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x061C-0x061F
      { 0, 0 }, { 0x02CC, 0 }, { '\'', 'A' }, { '>', 0 },		// 0x0620-0x0623
      { '&', 0 }, { '<', 0 }, { '}', 0 }, { 'A', 0 },		// 0x0624-0x0627
      { 'b', 0 }, { 0x1E97, 0 }, { 't', 0 }, { 0x1E6F, 0 },		// 0x0628-0x062B
      { 0x01E7, 0 }, { 0x1E25, 0 }, { 0x1E96, 0 }, { 'd', 0 },	// 0x062C-0x062F
      { 0x1E0F, 0 }, { 'r', 0 }, { 'z', 0 }, { 's', 0 },	// 0x0630-0x0633
      { 0x0161, 0 }, { 0x1E63, 0 }, { 0x1E0D, 0 }, { 0x1E6D, 0 },		// 0x0634-0x0637
      { 0x1E93, 0 }, { 0x02BF, 0 }, { 0x0121, 0 }, { 0, 0 },		// 0x0638-0x063B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x063C-0x063F
      { '_', 0 }, { 'f', 0 }, { 'q', 0 }, { 'k', 0 },		// 0x0640-0x0643
      { 'l', 0 }, { 'm', 0 }, { 'n', 0 }, { 'h', 0 },		// 0x0644-0x0647
      { 'w', 0 }, { 0x1EF3, 0 }, { 'y', 0 }, { 'F', 0 },		// 0x0648-0x064B
      { 'N', 0 }, { 'K', 0 }, { 'a', 0 }, { 'u', 0 },		// 0x064C-0x064F
      { 'i', 0 }, { '~', 0 }, { 'o', 0 }, { '^', 0 },		// 0x0650-0x0653
      { 0x02C8, 0 }, { '\'', 0 }, { 0, 0 }, { 0, 0 },		// 0x0654-0x0657
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0658-0x065B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x065C-0x065F
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0660-0x0663
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0664-0x0667
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0668-0x066B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x066C-0x066F
      { '`', 0 }, { '{', 0 }, { 0, 0 }, { 0, 0 },		// 0x0670-0x0673
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0674-0x0677
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0678-0x067B
      { 0, 0 }, { 0, 0 }, { 'P', 0 }, { 0, 0 },		// 0x067C-0x067F
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0680-0x0683
      { 0, 0 }, { 0, 0 }, { 'J', 0 }, { 0, 0 },		// 0x0684-0x0687
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0688-0x068B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x068C-0x068F
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0690-0x0693
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0694-0x0697
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0698-0x069B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x069C-0x069F
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x06A0-0x06A3
      { 'V', 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x06A4-0x06A7
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x06A8-0x06AB
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 'G', 0 }		// 0x06AC-0x06AF
   } ;

// romanization table for Devanagari
#define ISO15919_TABLE_START 0x0900
static const ISO9Element ISO15919_table[] = 
   {
      { 0, 0 }, { 'm', 0x0310 }, { 0x1E41, 0 }, { 0x1E25, 0 },		// 0x0900-0x0903
      { 0, 0 }, { 'a', 0 }, { 0x0101, 0 }, { 'i', 0 },			// 0x0904-0x0907
      { 0x012B, 0 }, { 'u', 0 }, { 0x016B, 0 }, { 0x1E5B, 0 },		// 0x0908-0x090B
      { 0x1E37, 0 }, { 0x00EA, 0 }, { 'e', 0 }, { 0x0113, 0 },		// 0x090C-0x090F
      { 'a', 'i' }, { 0x00F4, 0 }, { 'o', 0 }, { 0x014D, 0 },		// 0x0910-0x0913
      { 'a', 'u' }, { 'k', 'a' }, { 'k', 'h' }, { 'g', 'a' },		// 0x0914-0x0917
      { 'g', 'h' }, { 0x1E45, 'a' }, { 'c', 'a' }, { 'c', 'h' },	// 0x0918-0x091B
      { 'j', 'a' }, { 'j', 'h' }, { 0x00F1, 'a' }, { 0x1E6D, 'a' },	// 0x091C-0x091F
      { 0x1E6D, 'h' }, { 0x1E0D, 'a' }, { 0x1E0D, 'h' }, { 0x1E47, 'a' },// 0x0920-0x0923
      { 't', 'a' }, { 't', 'h' }, { 'd', 'a' }, { 'd', 'h' },		// 0x0924-0x0927
      { 'n', 'a' }, { 'n', 0x0331 }, { 'p', 'a' }, { 'p', 'h' },	// 0x0928-0x092B
      { 'b', 'a' }, { 'b', 'h' }, { 'm', 'a' }, { 'y', 'a' },		// 0x092C-0x092F
      { 'r', 'a' }, { 'r', 0x0331 }, { 'l', 'a' }, { 0x1E37, 'a' },	// 0x0930-0x0933
      { 'l', 0x0331 }, { 'v', 'a' }, { 0x015B, 'a' }, { 0x1E63, 'a' },	// 0x0934-0x0937
      { 's', 'a' }, { 'h', 'a' }, { 0, 0 }, { 0, 0 },			// 0x0938-0x093B
      { 0, 0 }, { '\'', 0 }, { 0x0101, 0 }, { 'i', 0 },			// 0x093C-0x093F
      { 0x012B, 0 }, { 'u', 0 }, { 0x016B, 0 }, { 0x1E5B, 0 },		// 0x0940-0x0943
      { 0x1E5D, 0 }, { 0x00EA, 0 }, { 'e', 0 }, { 0x0113, 0 },		// 0x0944-0x0947
      { 'a', 'i' }, { 0x00F4, 0 }, { 'o', 0 }, { 0x014D, 0 },		// 0x0948-0x094B
      { 'a', 'u' }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x094C-0x094F
      { 'o', 0x1E43 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0950-0x0953
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },			// 0x0954-0x0957
      { 'q', 'a' }, { 'k' , 'h' }, { 0x0121, 0 }, { 'z', 'a' },		// 0x0958-0x095B
      { 0x1E5B, 'a' }, { 0x1E5B, 'h' }, { 'f', 'a' }, { 0x1E8F, 'a' },	// 0x095C-0x095F
      { 0x1E5D, 0 }, { 0x1E39, 0 }, { 0x1E37, 0 }, { 0x1E39, 0 },	// 0x0960-0x0963
      { '.', 0 }, { '.', '.' }, { '0', 0 }, { '1', 0 },			// 0x0964-0x0967
      { '2', 0 }, { '3', 0 }, { '4', 0 }, { '5', 0 },			// 0x0968-0x096B
      { '6', 0 }, { '7', 0 }, { '8', 0 }, { '9', 0 },			// 0x096C-0x096F
      { 0x2026, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },			// 0x0970-0x0973
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0974-0x0977
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x0978-0x097B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },		// 0x097C-0x097F
   } ;

// romanization table for Thai
#define ISO11940_TABLE_START 0x0E00
static const ISO9Element ISO11940_table[] = 
   {
      { 0, 0 }, { 'k', 0 }, { 'k', 'h' }, { 0x1E33, 'h' },	// 0x0E00-0x0E03
      { 'k', 'h' }, { 'k', 'h' }, { 0x1E33, 'h' }, { 'n', 'g' }, // 0x0E04-0x0E07
      { 'c', 0 }, { 'c', 'h' }, { 'c', 'h' }, { 's', 0 },	// 0x0E08-0x0E0B
      { 'c', 'h' }, { 'y', 0 }, { 0x1E0D, 0 }, { 0x1E6D, 0 },	// 0x0E0C-0x0E0F
      { 0x1E6D, 'h' }, { 0x1E6F, 'h' }, { 't', 'h' }, { 0x1E47, 0 }, // 0x0E10-0x0E13
      { 'd', 0 }, { 't', 0 }, { 't', 'h' }, { 't', 'h' },	// 0x0E14-0x0E17
      { 0x1E6D, 'h' }, { 'n', 0 }, { 'b', 0 }, { 'p', 0 },	// 0x0E18-0x0E1B
      { 'p', 'h' }, { 'f', 0 }, { 'p', 'h' }, { 'f', 0 },	// 0x0E1C-0x0E1F
      { 'p', 'h' }, { 'm', 0 }, { 'y', 0 }, { 'r', 0 },		// 0x0E20-0x0E23
      { 'v', 0 }, { 'l', 0 }, { 0x0142, 0 }, { 'w', 0 },	// 0x0E24-0x0E27
      { 0x1E63, 0 }, { 's', 0 }, { 's', 0 }, { 'h', 0 },	// 0x0E28-0x0E2B
      { 0x1E37, 0 }, { 'x', 0 }, { 0x1E25, 0 }, { 0x01C2, 0 },	// 0x0E2C-0x0E2F
      { 'a', 0 }, { 0x1EA1, 0 }, { 0x0101, 0 }, { 0x00E5, 0 },	// 0x0E30-0x0E33
      { 'i', 0 }, { 0x012B, 0 }, { 0x1EE5, 0 }, { 0x1EE5, 0 },	// 0x0E34-0x0E37
      { 'u', 0 }, { 0x016B, 0 }, { 0, 0 }, { 0, 0 },		// 0x0E38-0x0E3B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },			// 0x0E3C-0x0E3F
      { 'e', 0 }, { 0x00E6, 0 }, { 'o', 0 }, { 0x0131, 0 },	// 0x0E40-0x0E43
      { 0x1ECB, 0 }, { 0x0268, 0 }, { 0x00AB, 0 }, { 0, 0 },	// 0x0E44-0x0E47
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },			// 0x0E48-0x0E4B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0x00A7, 0 },		// 0x0E4C-0x0E4F
      { '0', 0 }, { '1', 0 }, { '2', 0 }, { '3', 0 },		// 0x0E50-0x0E53
      { '4', 0 }, { '5', 0 }, { '6', 0 }, { '7', 0 },		// 0x0E54-0x0E57
      { '8', 0 }, { '9', 0 }, { 0x01C1, 0 }, { 0x00BB, 0 },	// 0x0E58-0x0E5B
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }			// 0x0E5C-0x0E5F
   } ;

// romanization information for Georgian
#define ISO9984_TABLE_START 0x10D0
static const ISO9Element ISO9984_table[] = 
   {
      { 'a', 0 }, { 'b', 0 }, { 'g', 0 }, { 'd', 0 },		// 0x10D0-0x10D3
      { 'e', 0 }, { 'v', 0 }, { 'z', 0 }, { 't', '\'' },	// 0x10D4-0x10D7
      { 'i', 0 }, { 'k', 0 }, { 'l', 0 }, { 'm', 0 },		// 0x10D8-0x10DB
      { 'n', 0 }, { 'o', 0 }, { 'p', 0 }, { 0x017E, 0 },	// 0x10DC-0x10DF
      { 'r', 0 }, { 's', 0 }, { 't', 0 }, { 'u', 0 },		// 0x10E0-0x10E3
      { 'p', '\'' }, { 'k', '\'' }, { 0x1E21, 0 }, { 'q', 0 },	// 0x10E4-0x10E7
      { 0x0161, 0 }, { 0x010D, '\'' }, { 'c', '\'' }, { 'j', 0 }, // 0x10E8-0x10EB
      { 'c', 0 }, { 0x010D, 0 }, { 'x', 0 }, { 0x01F0, 0 },	// 0x10EC-0x10EF
      { 'h', 0 }, { 0x0113, 0 }, { 'y', 0 }, { 'w', 0 },	// 0x10F0-0x10F3
      { 0x1E96, 0 }, { 0x014D, 0 }, { 'f', 0 }, { 0, 0 }	// 0x10F4-0x10F7
   } ;

// romanization information for extended Greek characters
//    (treated like ISO 843 after ignoring diacritics)
#define ISO843EXT_TABLE_START 0x1F00
static const ISO9Element ISO843ext_table[] = 
   {
      { 'a', 0 }, { 'a', 0 }, { 'a', 0 }, { 'a', 0 },		// 0x1F00-0x1F03
      { 'a', 0 }, { 'a', 0 }, { 'a', 0 }, { 'a', 0 },		// 0x1F04-0x1F07
      { 'A', 0 }, { 'A', 0 }, { 'A', 0 }, { 'A', 0 },		// 0x1F08-0x1F0B
      { 'A', 0 }, { 'A', 0 }, { 'A', 0 }, { 'A', 0 },		// 0x1F0C-0x1F0F
      { 'e', 0 }, { 'e', 0 }, { 'e', 0 }, { 'e', 0 },		// 0x1F10-0x1F13
      { 'e', 0 }, { 'e', 0 }, { 0, 0 }, { 0, 0 },		// 0x1F14-0x1F17
      { 'E', 0 }, { 'E', 0 }, { 'E', 0 }, { 'E', 0 },		// 0x1F18-0x1F1B
      { 'E', 0 }, { 'E', 0 }, { 0, 0 }, { 0, 0 },		// 0x1F1C-0x1F1F
      { 0x012B, 0 }, { 0x012B, 0 }, { 0x012B, 0 }, { 0x012B, 0 }, // 0x1F20-0x1F23
      { 0x012B, 0 }, { 0x012B, 0 }, { 0x012B, 0 }, { 0x012B, 0 }, // 0x1F24-0x1F27
      { 0x012A, 0 }, { 0x012A, 0 }, { 0x012A, 0 }, { 0x012A, 0 }, // 0x1F28-0x1F2B
      { 0x012A, 0 }, { 0x012A, 0 }, { 0x012A, 0 }, { 0x012A, 0 }, // 0x1F2C-0x1F2F
      { 'i', 0 }, { 'i', 0 }, { 'i', 0 }, { 'i', 0 },		// 0x1F30-0x1F33
      { 'i', 0 }, { 'i', 0 }, { 'i', 0 }, { 'i', 0 },		// 0x1F34-0x1F37
      { 'I', 0 }, { 'I', 0 }, { 'I', 0 }, { 'I', 0 },		// 0x1F38-0x1F3B
      { 'I', 0 }, { 'I', 0 }, { 'I', 0 }, { 'I', 0 },		// 0x1F3C-0x1F3F
      { 'o', 0 }, { 'o', 0 }, { 'o', 0 }, { 'o', 0 },		// 0x1F40-0x1F43
      { 'o', 0 }, { 'o', 0 }, { 0, 0 }, { 0, 0 },		// 0x1F44-0x1F47
      { 'O', 0 }, { 'O', 0 }, { 'O', 0 }, { 'O', 0 },		// 0x1F48-0x1F4B
      { 'O', 0 }, { 'O', 0 }, { 0, 0 }, { 0, 0 },		// 0x1F4C-0x1F4F
      { 'u', 0 }, { 'u', 0 }, { 'u', 0 }, { 'u', 0 },		// 0x1F50-0x1F53
      { 'u', 0 }, { 'u', 0 }, { 'u', 0 }, { 'u', 0 },		// 0x1F54-0x1F57
      { 0, 0 }, { 'U', 0 }, { 0, 0 }, { 'U', 0 },		// 0x1F58-0x1F5B
      { 0, 0 }, { 'U', 0 }, { 0, 0 }, { 'U', 0 },		// 0x1F5C-0x1F5F
      { 0x014D, 0 }, { 0x014D, 0 }, { 0x014D, 0 }, { 0x014D, 0 },		// 0x1F60-0x1F63
      { 0x014D, 0 }, { 0x014D, 0 }, { 0x014D, 0 }, { 0x014D, 0 },		// 0x1F64-0x1F67
      { 0x014C, 0 }, { 0x014C, 0 }, { 0x014C, 0 }, { 0x014C, 0 },		// 0x1F68-0x1F6B
      { 0x014C, 0 }, { 0x014C, 0 }, { 0x014C, 0 }, { 0x014C, 0 },		// 0x1F6C-0x1F6F
      { 'a', 0 }, { 'a', 0 }, { 'e', 0 }, { 'e', 0 },		// 0x1F70-0x1F73
      { 0x012B, 0 }, { 0x012B, 0 }, { 'i', 0 }, { 'i', 0 },		// 0x1F74-0x1F77
      { 'o', 0 }, { 'o', 0 }, { 'u', 0 }, { 'u', 0 },		// 0x1F78-0x1F7B
      { 0x014D, 0 }, { 0x014D, 0 }, { 0, 0 }, { 0, 0 },		// 0x1F7C-0x1F7F
      { 'a', 0 }, { 'a', 0 }, { 'a', 0 }, { 'a', 0 },		// 0x1F80-0x1F83
      { 'a', 0 }, { 'a', 0 }, { 'a', 0 }, { 'a', 0 },		// 0x1F84-0x1F87
      { 'A', 0 }, { 'A', 0 }, { 'A', 0 }, { 'A', 0 },		// 0x1F88-0x1F8B
      { 'A', 0 }, { 'A', 0 }, { 'A', 0 }, { 'A', 0 },		// 0x1F8C-0x1F8F
      { 0x012B, 0 }, { 0x012B, 0 }, { 0x012B, 0 }, { 0x012B, 0 }, // 0x1F90-0x1F93
      { 0x012B, 0 }, { 0x012B, 0 }, { 0x012B, 0 }, { 0x012B, 0 }, // 0x1F94-0x1F97
      { 0x012A, 0 }, { 0x012A, 0 }, { 0x012A, 0 }, { 0x012A, 0 }, // 0x1F98-0x1F9B
      { 0x012A, 0 }, { 0x012A, 0 }, { 0x012A, 0 }, { 0x012A, 0 }, // 0x1F9C-0x1F9F
      { 0x014D, 0 }, { 0x014D, 0 }, { 0x014D, 0 }, { 0x014D, 0 }, // 0x1FA0-0x1FA3
      { 0x014D, 0 }, { 0x014D, 0 }, { 0x014D, 0 }, { 0x014D, 0 }, // 0x1FA4-0x1FA7
      { 0x014C, 0 }, { 0x014C, 0 }, { 0x014C, 0 }, { 0x014C, 0 }, // 0x1FA8-0x1FAB
      { 0x014C, 0 }, { 0x014C, 0 }, { 0x014C, 0 }, { 0x014C, 0 }, // 0x1FAC-0x1FAF
      { 'a', 0 }, { 'a', 0 }, { 'a', 0 }, { 'a', 0 },		// 0x1FB0-0x1FB3
      { 'a', 0 }, { 0, 0 }, { 'a', 0 }, { 'a', 0 },		// 0x1FB4-0x1FB7
      { 'A', 0 }, { 'A', 0 }, { 'A', 0 }, { 'A', 0 },		// 0x1FB8-0x1FBB
      { 'A', 0 }, { '\'', 0 }, { 0, 0 }, { '\'', 0 },		// 0x1FBC-0x1FBF
      { '~', 0 }, { 0, 0 }, { 0x012B, 0 }, { 0x012B, 0 },	// 0x1FC0-0x1FC3
      { 0x012B, 0 }, { 0, 0 }, { 0x012B, 0 }, { 0x012B, 0 },	// 0x1FC4-0x1FC7
      { 'E', 0 }, { 'E', 0 }, { 0x012A, 0 }, { 0x012A, 0 },	// 0x1FC8-0x1FCB
      { 0x012A, 0 }, { '\'', '`' }, { 0, 0 }, { 0, 0 },		// 0x1FCC-0x1FCF
      { 'i', 0 }, { 'i', 0 }, { 'i', 0 }, { 'i', 0 },		// 0x1FD0-0x1FD3
      { 0, 0 }, { 0, 0 }, { 'i', 0 }, { 'i', 0 },		// 0x1FD4-0x1FD7
      { 'I', 0 }, { 'I', 0 }, { 'I', 0 }, { 'I', 0 },		// 0x1FD8-0x1FDB
      { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 },			// 0x1FDC-0x1FDF
      { 'u', 0 }, { 'u', 0 }, { 'u', 0 }, { 'u', 0 },		// 0x1FE0-0x1FE3
      { 'r', 0 }, { 'r', 0 }, { 'u', 0 }, { 'u', 0 },		// 0x1FE4-0x1FE7
      { 'U', 0 }, { 'U', 0 }, { 'U', 0 }, { 'U', 0 },		// 0x1FE8-0x1FEB
      { 'R', 0 }, { 0, 0 }, { 0, 0 }, { '`', 0 },		// 0x1FEC-0x1FEF
      { 0, 0 }, { 0, 0 }, { 0x014D, 0 }, { 0x014D, 0 },		// 0x1FF0-0x1FF3
      { 0x014D, 0 }, { 0, 0 }, { 0x014D, 0 }, { 0x014D, 0 },	// 0x1FF4-0x1FF7
      { 'O', 0 }, { 'O', 0 }, { 0x014C, 0 }, { 0x014C, 0 },	// 0x1FF8-0x1FFB
      { 0x014C, 0 }, { '\'', 0 }, { '`', 0 }, { 0, 0 },		// 0x1FFC-0x1FFF
   } ;

/************************************************************************/
/*	Helper Functions						*/
/************************************************************************/

inline bool in_ISO9_table(wchar_t codepoint)
{
   return ((unsigned)codepoint >= ISO9_TABLE_START && 
	   (unsigned)codepoint < ISO9_TABLE_START + lengthof(ISO9_table)) ;
}

//----------------------------------------------------------------------

inline bool in_ISO233_table(wchar_t codepoint)
{
   return ((unsigned)codepoint >= ISO233_TABLE_START && 
	   (unsigned)codepoint < ISO233_TABLE_START + lengthof(ISO233_table)) ;
}

//----------------------------------------------------------------------

inline bool in_ISO259_table(wchar_t codepoint)
{
   return ((unsigned)codepoint >= ISO259_TABLE_START && 
	   (unsigned)codepoint < ISO259_TABLE_START + lengthof(ISO259_table)) ;
}

//----------------------------------------------------------------------

inline bool in_ISO843_table(wchar_t codepoint)
{
   return ((unsigned)codepoint >= ISO843_TABLE_START && 
	   (unsigned)codepoint < ISO843_TABLE_START + lengthof(ISO843_table)) ;
}

//----------------------------------------------------------------------

inline bool in_ISO843ext_table(wchar_t codepoint)
{
   return ((unsigned)codepoint >= ISO843EXT_TABLE_START && 
	   (unsigned)codepoint < ISO843EXT_TABLE_START + lengthof(ISO843ext_table)) ;
}

//----------------------------------------------------------------------

inline bool in_ISO9984_table(wchar_t codepoint)
{
   return ((unsigned)codepoint >= ISO9984_TABLE_START && 
	   (unsigned)codepoint < ISO9984_TABLE_START + lengthof(ISO9984_table)) ;
}

//----------------------------------------------------------------------

inline bool in_ISO9985_table(wchar_t codepoint)
{
   return ((unsigned)codepoint >= ISO9985_TABLE_START && 
	   (unsigned)codepoint < ISO9985_TABLE_START + lengthof(ISO9985_table)) ;
}

//----------------------------------------------------------------------

inline bool in_ISO11940_table(wchar_t codepoint)
{
   return ((unsigned)codepoint >= ISO11940_TABLE_START && 
	   (unsigned)codepoint < ISO11940_TABLE_START + lengthof(ISO11940_table)) ;
}

//----------------------------------------------------------------------

inline bool in_ISO15919_table(wchar_t codepoint)
{
   return ((unsigned)codepoint >= ISO15919_TABLE_START && 
	   (unsigned)codepoint < ISO15919_TABLE_START + lengthof(ISO15919_table)) ;
}

/************************************************************************/
/************************************************************************/

bool romanizable_codepoint(wchar_t codepoint)
{
   if (in_ISO9_table(codepoint))
      {
      if (ISO9_table[codepoint - ISO9_TABLE_START].codepoint1)
	 return true ;
      }
   else if (in_ISO233_table(codepoint))
      {
      if (ISO233_table[codepoint - ISO233_TABLE_START].codepoint1)
	 return true ;
      }
   else if (in_ISO259_table(codepoint))
      {
      if (ISO259_table[codepoint - ISO259_TABLE_START].codepoint1)
	 return true ;
      }
   else if (in_ISO843_table(codepoint))
      {
      if (ISO843_table[codepoint - ISO843_TABLE_START].codepoint1)
	 return true ;
      }
   else if (in_ISO843ext_table(codepoint))
      {
      if (ISO843ext_table[codepoint - ISO843EXT_TABLE_START].codepoint1)
	 return true ;
      }
   else if (in_ISO9984_table(codepoint))
      {
      if (ISO9984_table[codepoint - ISO9984_TABLE_START].codepoint1)
	 return true ;
      }
   else if (in_ISO9985_table(codepoint))
      {
      if (ISO9985_table[codepoint - ISO9985_TABLE_START].codepoint1)
	 return true ;
      }
   else if (in_ISO11940_table(codepoint))
      {
      if (ISO11940_table[codepoint - ISO11940_TABLE_START].codepoint1)
	 return true ;
      }
   else if (in_ISO15919_table(codepoint))
      {
      if (ISO15919_table[codepoint - ISO15919_TABLE_START].codepoint1)
	 return true ;
      }
   return false ;
}

//----------------------------------------------------------------------

unsigned romanize_codepoint(wchar_t codepoint, wchar_t &romanized1, wchar_t &romanized2)
{
   const ISO9Element *table = 0 ;
   if (in_ISO9_table(codepoint))
      {
      table = ISO9_table ;
      codepoint -= ISO9_TABLE_START ;
      }
   else if (in_ISO233_table(codepoint))
      {
      table = ISO233_table ;
      codepoint -= ISO233_TABLE_START ;
      }
   else if (in_ISO259_table(codepoint))
      {
      table = ISO259_table ;
      codepoint -= ISO259_TABLE_START ;
      }
   else if (in_ISO843_table(codepoint))
      {
      table = ISO843_table ;
      codepoint -= ISO843_TABLE_START ;
      }
   else if (in_ISO843ext_table(codepoint))
      {
      table = ISO843ext_table ;
      codepoint -= ISO843EXT_TABLE_START ;
      }
   else if (in_ISO9984_table(codepoint))
      {
      table = ISO9984_table ;
      codepoint -= ISO9984_TABLE_START ;
      }
   else if (in_ISO9985_table(codepoint))
      {
      table = ISO9985_table ;
      codepoint -= ISO9985_TABLE_START ;
      }
   else if (in_ISO11940_table(codepoint))
      {
      table = ISO11940_table ;
      codepoint -= ISO11940_TABLE_START ;
      }
   else if (in_ISO15919_table(codepoint))
      {
      table = ISO15919_table ;
      codepoint -= ISO15919_TABLE_START ;
      }
   if (table)
      {
      romanized1 = table[codepoint].codepoint1 ;
      if (romanized1)
	 {
	 romanized2 = table[codepoint].codepoint2 ;
	 return romanized2 ? 2 : 1 ;
	 }
      }
   romanized1 = codepoint ;
   return 1 ;
}

//----------------------------------------------------------------------

int romanize_codepoint(wchar_t codepoint, char *buffer)
{
   wchar_t romanized1 ;
   wchar_t romanized2 ;
   int points = romanize_codepoint(codepoint, romanized1, romanized2) ;
   int bytes = 0 ;
   if (points)
      {
      bool byteswap = false ;
      bytes = Fr_Unicode_to_UTF8(romanized1,buffer,byteswap) ;
      if (points == 2)
	 {
	 int bytes2 = Fr_Unicode_to_UTF8(romanized2,buffer+bytes,byteswap) ;
	 if (bytes2)
	    bytes += bytes2 ;
	 else
	    bytes = 0 ;
	 }
      }
   return bytes ;
}

//----------------------------------------------------------------------

unsigned get_UTF8_codepoint(const char *buf, wchar_t &codepoint)
{
   unsigned len = 0 ;
   codepoint = 0 ;
   if (buf)
      {
      unsigned cp = ((unsigned char*)buf)[0] ;
      if ((cp & 0x80) == 0)
	 {
	 codepoint = cp ;
	 return 1 ;
	 }
      else if ((cp & 0xE0) == 0xC0)
	 {
	 len = 2 ;
	 cp &= 0x1F ;
	 }
      else if ((cp & 0xF0) == 0xE0)
	 {
	 len = 3 ;
	 cp &= 0x0F ;
	 }
      else if ((cp & 0xF8) == 0xF0)
	 {
	 len = 4 ;
	 cp &= 0x07 ;
	 }
      else if ((cp & 0xFC) == 0xF8)
	 {
	 len = 5 ;
	 cp &= 0x03 ;
	 }
      else if ((cp & 0xFE) == 0xFC)
	 {
	 len = 6 ;
	 cp &= 0x01 ;
	 }
      else
	 {
	 // high bits are 10, which is a continuation byte and invalid at
	 //   this point
	 codepoint = 0xFFFF ;
	 return 0 ;
	 }
      for (size_t i = 1 ; i < len ; i++)
	 {
	 if ((buf[i] & 0xC0) != 0x80)
	    {
	    codepoint = 0xFFFF ;
	    return 0 ; // invalid byte sequence
	    }
	 cp = (cp << 6) | (buf[i] & 0x3F) ;
	 }
      codepoint = cp ;
      }
   return len ;
}

// end of file roman.C //
