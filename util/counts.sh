#!/bin/csh -f
# LastEdit: 12aug2014

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

# where to find stuff
set basedir=/home/ralf/src/la-strings
set p="$0:h"
if ("$p" == "$0") set p=.

set method="whatlang"

unset lddir
unset mgdir
unset tcdir
unset lpymodel
# program executables
set convertprog=iconv
set whatlang="$p/whatlang"
set mguesser="$p/mguesser"
set textcat="$p/testtextcat"
#set langidpy="python $p/langid.py"
set langidpy="python $p/langid-rb.py"
set yali="$p/yali-identifier"
set ldjar="/home/ralf/src/langdetect/lib/langdetect.jar"
set convert=cat

# program options
set db=$basedir/languages.db
set yalidir="$p/yali-models"
set ldmem=9600
set max=2000
set smooth="-b1"
set weights=""
set cvt=""
set bwt=""
set swt=""
set convert=cat
set convertto=""
set langdetect="java -Xmx${ldmem}m -jar $ldjar --detectlang -s 0"
unset smoothing

while ($#argv > 1 && x$1 =~ x-* )
   switch ($1)
      case "--db":
      case "--whatlang":
         set method="whatlang"
         shift
	 set db="$1"
	 breaksw
      case "--max":
         shift
	 set max="$1"
	 breaksw
      case "--smoothscores":
         set smooth="-b2"
	 breaksw
      case "--bigrams":
         shift
	 set bwt="b$1"
	 breaksw
      case "--stopgrams":
         shift
	 set swt="s$1"
	 breaksw
      case "--mguesser":
         shift
	 set mgdir="$1"
         breaksw
      case "--textcat":
         set method="textcat"
         shift
	 set tcdir="$1"
         breaksw
      case "--langdetect":
         set method="langdetect"
	 shift
	 set lddir="$1"
	 breaksw
      case "--langid.py":
	 set method="langid.py"
         shift
	 set lpymodel="$1"
	 breaksw
      case "--yali":
         set method="YALI"
	 shift
	 set yalidir="$1"
	 breaksw
      case "--utf8":
         # force all input files to UTF-8 encoding
         set convertto="utf8//TRANSLIT//IGNORE"
	 breaksw
      case "--utf16be":
         set convertto="utf16be//TRANSLIT//IGNORE"
	 set cvt="-16b"
	 breaksw
      case "--utf16le":
         set convertto="utf16le//TRANSLIT//IGNORE"
	 set cvt="-16l"
	 breaksw
      case "--smoothfreq":
         shift
	 set smoothing="$1"
	 breaksw
      default:
         echo "Unknown option $1"
         breaksw
   endsw
   shift
end

onintr cleanup

if ("$bwt" != "" || "$swt" != "") then
   set weights="-W$bwt,$swt"
endif

# override executables with copies in the current directory if present
if ( -e ./whatlang ) set whatlang=./whatlang
if ( -e ./mguesser ) set mguesser=./mguesser
if ( -e ./testtextcat ) set textcat=./testtextcat
if ( -e ./yali-identifier ) set yali=./yali-identifier

if ( $method == "langid.py" ) then
# reformat output and remove model-specific digits
cat >/tmp/cnt$$ <<EOF
s@^[(]'\([^']*\)',.*[)]\$@\1	@
s@^\([^	0-9]*\)[0-9][0-9]*	@\1	@
EOF
else if ( $method == "langdetect" ) then
# reformat output and remove model-specific digits
cat >/tmp/cnt$$ <<EOF
s@^[^:]*:\[\([^:]*\):[0-9.]*\]\$@\1	@
s@^\([^	0-9]*\)[0-9][0-9]*	@\1	@
s/	.*//
EOF
else if ( $method == "YALI" ) then
   # build the model list if it doesn't yet exist
   if ( ! -e "$yalidir/models" ) then
      "$p/yali-models.sh" "$yalidir"
   endif
   # ensure non-empty ids and a tab on each line so that subsequent
   #   processing works correctly; also strip off per-model disambiguating digits
   cat >/tmp/cnt$$ <<EOF
s@^\$@UNK@
s@\$@	@
s@^\([^	0-9]*\)[0-9][0-9]*	@\1	@
EOF
endif

if ( $?smoothing ) then
   switch ( $method )
      case "overhead":
         breaksw
      case "langdetect":
          set smoothing="-S$smoothing"
          breaksw
      case "langid.py":
          echo Frequency smoothing ignored for $method -- Not Yet Implemented
	  breaksw
      case "mguesser":
          set smoothing="-s$smoothing"
	  breaksw
      case "textcat":
          set smoothing="-s$smoothing"
	  breaksw
      case "YALI":
	   set smoothing="--smooth $smoothing"
	   breaksw
      case "whatlang":
          echo Frequency smoothing ignored for whatlang -- must be applied while building models
	  breaksw
      deafult:
          echo "Unknown method '$method' for smoothing"
	  breaksw
   endsw
else
   set smoothing=""
endif

set files=$#argv
setenv LC_ALL C
if ($files == 0) then
   if ("x$convertto" != "x") then
      set convert="$convertprog -f utf8 -t $convertto"
   endif
   switch ($method)
      case "mguesser":
         head -${max} | sed -e 's/^[-a-zA-Z]*	//' | $convert \
	    | "$mguesser" -d"$mgdir" -n1 -L $smoothing \
	    | sed -e 's/	.*//' | sort | uniq -c | sort -nr
	 breaksw
      case "langdetect":
         head -${max} | sed -e 's/^[-a-zA-Z]*	//' | $convert >/tmp/ld$$
	 "$langdetect" -d "$lddir" --line /tmp/ld$$ \
	    | sed -f /tmp/cnt$$ | sort | uniq -c | sort -nr
	 breaksw
      case "textcat":
         head -${max} | sed -e 's/^[-a-zA-Z]*	//' | $convert \
	    | "$textcat" "$tcdir/conf.txt" -l $smoothing \
	    | sed -e 's/	.*//' | sort | uniq -c | sort -nr
	 breaksw
      case "langid.py":
	 head -${max} | sed -e 's/^[-a-zA-Z]*	//' | $convert \
	    | "$langidpy" -m $lpymodel --line | sed -f /tmp/cnt$$ \
	    | sed -e 's/	.*//' | sort | uniq -c | sort -nr
	 breaksw
      case "YALI":
	 head -${max} | sed -e 's/^[-a-zA-Z]*	//' | $convert \
	    | "$yali" --classes "$yalidir/models" --each --input - \
	    | sed -f /tmp/cnt$$ | sed -e 's/	.*//' | sort | uniq -c | sort -nr
	 breaksw
      case "whatlang":
	 head -${max} | sed -e 's/^[-a-zA-Z]*	//' | $convert \
	    | "$whatlang" -l$db $smooth $cvt $weights -t -n1 \
	    | sed -e 's/	.*//' | sort | uniq -c | sort -nr
	 breaksw
      default:
         echo "Unknown method '$method'"
	 breaksw
   endsw
else
   while ($#argv > 0)
      if ($files > 1) echo "=== $1 ==="
      if ("x$1" == "") continue
      if ("x$convertto" != "x") then
         set enc=utf8
	 if ("$1" =~ *latin1* ) then
	    set enc=iso-8859-1
         else if ( "$1" =~ *latin2* ) then
            set enc=iso-8859-2
         else if ( "$1" =~ *latin5* ) then
            set enc=iso-8859-5
         else if ( "$1" =~ *euc-jp* ) then
            set enc=euc-jp
         else if ( "$1" =~ *euc-kr* ) then
            set enc=euc-kr
         else if ( "$1" =~ *euc-tw* ) then
            set enc=euc-tw
         else if ( "$1" =~ *win1250* ) then
	    set enc=windows-1250
         else if ( "$1" =~ *win1251* ) then
	    set enc=windows-1251
         else if ( "$1" =~ *win1252* ) then
	    set enc=windows-1252
         else if ( "$1" =~ *win1256* ) then
	    set enc=windows-1256
	 endif
	 set convert="$convertprog -f $enc -t $convertto"
      endif
      switch ($method)
         case "mguesser":
            head -${max} "$1" | sed -e 's/^[-a-z]*	//' | $convert \
		| "$mguesser" -d"$mgdir" -n1 -L $smoothing \
		| sed -e 's/	.*//' | sort | uniq -c | sort -nr
	    breaksw
	 case "langdetect":
	    head -${max} "$1" | sed -e 's/^[-a-z]*	//' | $convert >/tmp/ld$$
	    $langdetect -d "$lddir" --line /tmp/ld$$ \
		| sed -f /tmp/cnt$$ | sort | uniq -c | sort -nr
	    breaksw
	 case "textcat":
	    head -${max} "$1" | sed -e 's/^[-a-z]*	//' | $convert \
		| "$textcat" "$tcdir/conf.txt" -l $smoothing \
		| sed -e 's/	.*//' | sort | uniq -c | sort -nr
	    breaksw
	 case "langid.py":
	    #echo $langidpy:t -m $lpymodel --line
	    head -${max} "$1" | sed -e 's/^[-a-zA-Z]*	//' \
		| $convert | $langidpy -m $lpymodel --line | sed -f /tmp/cnt$$ \
		| sed -e 's/	.*//' | sort | uniq -c | sort -nr
	    breaksw
	 case "YALI":
	    #echo $yali:t -c $yalidir --each
	    head -${max} "$1" | sed -e 's/^[-a-zA-Z]*	//' | $convert \
		| "$yali" --classes "$yalidir/models" --each --input - \
		| sed -f /tmp/cnt$$ | sed -e 's/	.*//' | sort \
		| uniq -c | sort -nr
	    breaksw
	 case "whatlang":
	    head -${max} "$1" | sed -e 's/^[-a-zA-Z]*	//' | $convert \
		| "$whatlang" -l$db $smooth $cvt $weights -t -n1 \
		| sed -e 's/	.*//' | sort | uniq -c | sort -nr
	    breaksw
	 default:
	    echo "Unknown method '$method' (2)"
	    breaksw
      endsw
      shift
   end
endif

cleanup:
rm -f /tmp/cnt$$ /tmp/ld$$
exit 0
