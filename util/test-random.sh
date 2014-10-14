#!/bin/csh -f
set db=""
set min=4
unset show_encodings

while ($#argv > 1 && x$1 =~ x-*)
   switch ($1)
      case --db:
	  shift
	  set db="$1"
	  shift
	  breaksw
      case --min:
	  shift
	  set min="$1"
	  shift
	  breaksw
      case --enc:
	  shift
	  set show_encodings
	  breaksw
      default:
          echo "Unknown option $1"
	  exit 1
	  breaksw
   endsw
end
if ($#argv < 1) then
   echo "Usage: $0:t [options] iterations"
   echo "Options:"
   echo "  --db F    use language database in file F instead of default"
   echo "  --enc     show statistics on encodings used by la-strings"
   echo "  --min N   set minimum string length to N characters"
   exit 1
endif

onintr cleanup

set lastrings="`where la-strings|head -1`"
if (x$lastrings == x) set lastrings=./la-strings

echo "# false positive bytes from 10,000,000 bytes of random data"
echo "# strings: -es	-eS	-el	la-strings: -S4	-S7	-S10"
set tmp=/tmp/random$$
@ total1 = 0
@ total2 = 0
@ total3 = 0
@ total4 = 0
@ total5 = 0
@ total6 = 0
foreach iter (`seq 1 $1`)
   head -10000000c /dev/urandom >$tmp
   set s1=(`strings -es -n$min $tmp | wc -l -c`)
   set s2=(`strings -eS -n$min $tmp | wc -l -c`)
   set s3=(`strings -el -n$min $tmp | wc -l -c`)
   set s4=(`$lastrings -n$min -i$db -I0 -S4 $tmp | wc -l -c`)
   set s5=(`$lastrings -n$min -i$db -I0 -S7 $tmp | wc -l -c`)
   set s6=(`$lastrings -n$min -i$db -I0 -S10 $tmp | wc -l -c`)
   @ bytes1 = $s1[2] - $s1[1]
   @ bytes2 = $s2[2] - $s2[1]
   @ bytes3 = $s3[2] - $s3[1]
   @ bytes4 = $s4[2] - $s4[1]
   @ bytes5 = $s5[2] - $s5[1]
   @ bytes6 = $s6[2] - $s6[1]
   echo "$bytes1	$bytes2	$bytes3			$bytes4	$bytes5	$bytes6"
   @ total1 = $total1 + $bytes1
   @ total2 = $total2 + $bytes2
   @ total3 = $total3 + $bytes3
   @ total4 = $total4 + $bytes4
   @ total5 = $total5 + $bytes5
   @ total6 = $total6 + $bytes6
   if ($?show_encodings) then
      echo "strings: $s1[1] -es   $s2[1] -eS    $s3[1] -el"
      foreach i (4 7 10)
         echo S${i}: `la-strings -i$db -n$min -S$i -E -I0 $tmp|(setenv LC_ALL C;sed -e 's/	.*//')|sort|uniq -c|sort -nr|tr -d '\n'`
      end
   endif
end
echo "#------ Averages -------"
@ avg1 = $total1 / $1
@ avg2 = $total2 / $1
@ avg3 = $total3 / $1
@ avg4 = $total4 / $1
@ avg5 = $total5 / $1
@ avg6 = $total6 / $1
echo "$avg1	$avg2	$avg3			$avg4	$avg5	$avg6"

cleanup:
rm $tmp
exit 0
