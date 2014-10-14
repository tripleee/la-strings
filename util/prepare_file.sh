#!/bin/tcsh -f
# program executables
set convert=iconv

if ( $#argv != 9 ) then
echo $#argv
   echo "Usage: $0:t src prepared minlines maxlines minlen maxlen {Y|N} cvtto embedcount"
   exit 1
endif

set src="$1"
set prepared="$2"
set minlines="$3"
set maxlines="$4"
set minlen="$5"
set maxlen="$6"
unset strip_key
if ( x$7 == "xY" ) set strip_key
set convertto="$8"
set embed="$9"

set tmp=/tmp/prep$$

if ( $?strip_key ) then
   sed -e 's/^[^	]*	//' "$src" | head -$maxlines >"$prepared"
else
   head -$maxlines "$src" >"$prepared"
endif
set lines=`wc -l <"$prepared"`
if ($lines < $minlines) then
   rm -f "$prepared"
   exit 1
endif
if ($minlen != 0) then
   (setenv LC_ALL C ; egrep "^.{$minlen,}" "$prepared" >$tmp)
   mv $tmp "$prepared"
   set lines=`wc -l <"$prepared"`
endif
if ($maxlen != 0) then
   @ max = $maxlen + 1
   (setenv LC_ALL C ; egrep -v "^.{$max,}" "$prepared" >$tmp)
   mv $tmp "$prepared"
   set lines=`wc -l <"$prepared"`
endif
if ("x$convertto" != "x") then
   set enc=utf8
   if ( "$src:t" =~ *latin1* ) then
      set enc=iso-8859-1
   else if ( "$src:t" =~ *latin2* ) then
      set enc=iso-8859-2
   else if ( "$src:t" =~ *latin5* ) then
      set enc=iso-8859-5
   else if ( "$src:t" =~ *euc-jp* ) then
      set enc=euc-jp
   else if ( "$src:t" =~ *euc-kr* ) then
      set enc=euc-kr
   else if ( "$src:t" =~ *euc-tw* ) then
      set enc=euc-tw
   else if ( "$src:t" =~ *win1250* ) then
      set enc=windows-1250
   else if ( "$src:t" =~ *win1251* ) then
      set enc=windows-1251
   else if ( "$src:t" =~ *win1252* ) then
      set enc=windows-1252
   else if ( "$src:t" =~ *win1256* ) then
      set enc=windows-1256
   endif
   $convert -f $enc -t $convertto <"$prepared" >$tmp
   mv $tmp "$prepared"
endif
if ($embed != "0") then
   xargs -n1 '-d\n' <"$prepared" ./add-random.sh $embed >$tmp
   mv $tmp "$prepared"
endif
exit 0
