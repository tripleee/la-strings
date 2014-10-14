#!/bin/csh -f
# LastEdit: 08aug2014

#  (c) Copyright 2012,2013,2014 Ralf Brown/Carnegie Mellon University	
#      This program is free software; you can redistribute it and/or
#      modify it under the terms of the GNU General Public License as
#      published by the Free Software Foundation, version 3.
#
#      This program is distributed in the hope that it will be
#      useful, but WITHOUT ANY WARRANTY; without even the implied
#      warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
#      PURPOSE.  See the GNU General Public License for more details.
#
#      You should have received a copy of the GNU General Public
#      License (file COPYING) along with this program.  If not, see
#      http://www.gnu.org/licenses/

set dir="."
set language=""
set minlines=100
set char=""
#set char="-c"
unset quiet

if ( $?LC_ALL ) then
   if ( "x$LC_ALL" == xC ) then
      unsetenv LC_ALL
   endif
endif

while ($#argv > 0 && x$1 =~ x-* )
   switch ("$1")
      case '-b':
          set char="-b"
	  breaksw
      case '-d':
          shift
          set dir="$1"
	  breaksw
      case '-l':
	  shift
	  set language="$1"
	  breaksw
      case '-m':
          shift
	  set minlines="$1"
	  breaksw
      case '-q':
          set quiet
	  breaksw
      default:
          echo "Unrecognized flag $1"
	  exit 2
          breaksw
   endsw
   shift
end

if ($#argv < 3) then
   echo "Usage: $0:t [-b] [-d DIR] [-l lang] [-m minl] minwidth maxwidth file [file ...]"
   echo "  Word-wrap input files to at most 'maxwidth' characters (bytes if -b),"
   echo "  and at least 'minwidth' bytes.  If the result is less than 'minl'"
   echo "  (default $minlines) lines in length, merge all text into a single line"
   echo "  and then re-wrap the text.  When -l is specified, add 'lang' (derived"
   echo "  from the filename if 'auto') to the start of each output line.  If -d"
   echo "  is given, the output files will be stored in DIR instead of the current"
   echo "  directory."
   exit 1
endif

set minwidth="$1"
set maxwidth="$2"
shift
shift
while ($#argv > 0)
   set real_out="${dir}/${1:t:r:s/-test//}-test-${minwidth}-${maxwidth}.txt"
   set out=$real_out
   if ("x$1" == "x") continue
   set file="$1"
   if ( ! -e "$file" ) then
      if ( ! $?quiet ) echo Unable to access $file
      shift
      continue
   endif
   set enc="None"
   if ("$file" =~ *euc-jp*) then
      set enc="euc-jp"
   else if ("$file" =~ *euc-kr*) then
      set enc="euc-kr"
   else if ("$file" =~ *gb2312*) then
      set enc="gb2312"
   endif
   if ("$enc" != "None") then
      iconv -f $enc -t utf8 <"$file" >/tmp/mktin$$
      set file=/tmp/mktin$$
      set out=/tmp/mktout$$
   endif
   if ($language == "") then
      sed -e 's/^[ 	]*//' -e 's/[ 	]*$//' -e 's@[	][<]/a[>]@@' -e 's/$//' "$file" | \
	  fold -s $char -w $maxwidth | sed -e 's/﻿//' -e 's/^  *//' -e 's/  *$//' | \
	  grep -E "^.{$minwidth}" >"$out"
   else
      set lang="$language"
      if ($language == "auto") then
	 set lang=`echo $1:t:r:r:r:r:r|sed -e 's/_.*//'`
      endif
      sed -e 's/^[ 	]*//' -e 's/[ 	]*$//' -e 's/$//' "$file" | \
	  fold -s $char -w $maxwidth | sed -e 's/﻿//' -e 's/^  *//' -e 's/  *$//' | \
	  grep -E "^.{$minwidth}" >/tmp/mktest$$
      set lines=`wc -l </tmp/mktest$$`
      @ diff = $lines - $minlines - 1
      if ( x$diff =~ x-* ) then
         # re-do the wrapping using the entire input as one long line to avoid
         #  throwing out short lines as much as possible
         sed -e 's/^[ 	]*//' -e 's/[ 	]*$//' -e 's@[	 ][<]/a[>]@@' -e 's/$//' "$file" | tr '\n' ' ' | \
	     fold -s $char -w $maxwidth | \
	     sed -e 's/﻿//' -e 's/^  *//' -e 's/  *$//' | \
	     grep -E "^.{$minwidth}" >/tmp/mktest$$
         set lines=`wc -l </tmp/mktest$$`
      endif
      yes $lang | head -n $lines | paste - /tmp/mktest$$ >"$out"
      rm /tmp/mktest$$
   endif
   if ("$out" != "$real_out") then
      iconv -f utf8 -t $enc <"$out" >"$real_out"
      rm "$out"
   endif
   if ("$file" != "$1") then
      rm "$file"
   endif
   shift
end
exit 0
