#!/bin/csh -f
if ($#argv < 2) then
   echo "Usage: $0:t outfile infile [infile ...]"
   exit 1
endif
set out="$1"
shift

set min=4800
set max=9600
set incr=800

@ rand = $max - $incr

rm -f "$out"
echo "=== === === $max random filler bytes follow this line === === ===" >>"$out"
head -${max}c /dev/urandom >>"$out"
foreach i ($*)
   set base="$i:t:r"
   set enc="${base:e}"
   set base="${base:r}"
   set src="${base:e}"
   set lang="${base:r}"
   echo "" >>"$out"
   echo "**** The following lines of text are $lang in $enc from $src ****" >> "$out"
   cat "$i" >>"$out"
   head -8c /dev/zero >>"$out"
   echo "**** There are $rand random bytes of filler following this line ****" >>"$out"
   head -${rand}c /dev/urandom >>"$out"
   head -16c /dev/zero >>"$out"
   @ rand = $rand + $incr
   if ($rand > $max) set rand=$min
end

