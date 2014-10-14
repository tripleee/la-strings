#!/bin/tcsh -f
# LastEdit: 11aug2014

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

# which operation to perform
set method="whatlang"

# where to find stuff
set usrdir=/home/ralf/src
set basedir="$usrdir/la-strings"
set p="$0:h"
if ("$p" == "$0") set p="${cwd}"
if ("x$p" !~ x/*) set p="${cwd}/$p"
set d="$p"
if ("$d:t" == bin ) set d="$d:h"

if ( ! $?TMP ) setenv TMP /tmp

# program executables
set prep=""
if ( -e "$p/prepare_file.sh" ) set prep="$p/prepare_file.sh"
if ( -e "$p/bin/prepare_file.sh" ) set prep="$p/bin/prepare_file.sh"

set score="$basedir/util/score"
if ( -e "$p/score" ) set score="$p/score"
if ( -e "$p/bin/score" ) set score="$p/bin/score"
set whatlang="`where whatlang|head -1`"
if ( "x$whatlang" == x && -e "$basedir/langident/whatlang" ) set whatlang="$basedir/langident/whatlang"
if ( -e "$p/whatlang" && ! -d "$p/whatlang" ) set whatlang="$p/whatlang"
if ( -e "$p/bin/whatlang" ) set whatlang="$p/bin/whatlang"
set strings="`where la-strings|head -1`"
if ( "x$strings" == x && -e "$basedir/la-strings" ) set strings="$basedir/la-strings"
if ( -e "$p/la-strings" ) set strings="$p/la-strings"
if ( -e "$p/bin/la-strings" ) set strings="$p/bin/la-strings"
set mguesser="`where mguesser|head -1`"
if ( -e "$p/mguesser" && ! -d "$p/mguesser" ) set mguesser="$p/mguesser"
if ( -e "$p/bin/mguesser" ) set mguesser="$p/bin/mguesser"
set textcat="`where testtextcat|head -1`"
if ( -e "$p/testtextcat" ) set textcat="$p/testtextcat"
if ( -e "$p/bin/testtextcat" ) set textcat="$p/bin/testtextcat"
#set langidpy="python $p/langid.py"
set langidpy=""
if ( -e "$p/langid-rb.py" ) set langidpy="python $p/langid-rb.py"
if ( -e "$p/bin/langid-rb.py" ) set langidpy="python $p/bin/langid-rb.py"
set yali="$p/yali-identifier"
if ( -e "$p/bin/yali-identifier" ) set yali="$p/bin/yali-identifier"
set yalimod=""
if ( -e "$p/yali-models.sh" ) set yalimod="$p/yali-models.sh"
if ( -e "$p/bin/yali-models.sh" ) set yalimod="$p/bin/yali-models.sh"
set java=java
set ldjar=""
if ( -e "$usrdir/langdetect/lib/langdetect.jar" ) set ldjar="$usrdir/langdetect/lib/langdetect.jar"
if ( -e "$p/lib/langdetect.jar" ) set ldjar="$p/lib/langdetect.jar"
if ( -e "${p:h}/lib/langdetect.jar" ) set ldjar="${p:h}/lib/langdetect.jar"
set ldopts="-Xbatch -XX:+AggressiveOpts -XX:+UseLargePages" #"-Xprof"
set ldopts="-Xbatch" #"-Xprof"

# program options
set twoletter		# force two-letter ISO codes for YALI?
set whatlang_len=1
set langdetect_len="--line"
set langidpy_len="--line"
set mguesser_len=-L
set textcat_len=-l
set textcat_fp=
set ldminmem=2500
set ldmem=11700
set lddir="$d/langdetect-profiles"
if ( -e "$d/models/langdetect-profiles" ) set lddir="$d/models/langdetect-profiles"
set mgdir="$d/mguesser-maps"
if ( -e "$d/models/mguesser-maps" ) set mgdir="$d/models/mguesser-maps"
set tcdir="$d/textcat-lm"
if ( -e "$d/models/textcat-lm" ) set tcdir="$d/models/textcat-lm"
set lpymodel="model"
set ldequiv="sed -f $TMP/eq_$$"
set lpyequiv="sed -f $TMP/eq_$$"
set yaliequiv="sed -f $TMP/eq_$$"
set yalidir="$d/yali-models"
if ( -e "$d/models/yali-models" ) set yalidir="$d/models/yali-models"
set db="$basedir/languages.db"
if ( -e "$d/models/languages.db" ) set db="$d/models/languages.db"
set equiv=cat
set keyonly="cut -f1"
set convertto=""
set cvt=""
unset smoothing
unset batch_mode

# where to put temporary files
set tmp1="$TMP/eval$$"
set tmp2="$TMP/eval2_$$"
set tmp3="$TMP/eval3_$$"
set tmpkey="$TMP/evalkey_$$"
set str="$TMP/str$$"

# evaluation configuration (defaults)
set minlen=0  ## minimum line length to score
set maxlen=0  ## minimum line length to score, 0=unlimited
set minlines=100
set maxlines=1000
set thresh=""
set weights=""
set bwt=""
set swt=""
#set thresh="-S10"
set embed="0"
unset raw_only
unset show_misses
unset keep_extract

onintr cleanup

while ($#argv > 1 && x$1 =~ x-* )
   switch ($1)
      case "--batch":
         set batch_mode
	 breaksw
      case "--db":
      case "--whatlang":
	 shift
	 set db="$1"
         breaksw
      case "--file":
         set langdetect_len="--file"
	 set langidpy_len=""
         set mguesser_len=""
	 set textcat_len="-f"
	 set whatlang_len="500000"
	 breaksw
      case "--misses":
	 set show_misses
	 breaksw
      case "-S":
      case "--thresh":
         shift
	 set thresh="-S$1"
	 breaksw
      case "--embed":
         # embed N random bytes between lines of text
         shift
	 set embed="$1"
	 set show_misses
	 breaksw
      case "--equiv":
         set equiv="sed -f '$TMP/eq$$'"
	 breaksw
      case "--keep":
         set keep_extract
	 breaksw
      case "-S7":
      case "-S10":
         set thresh=$1
	 breaksw
      case "--bigrams":
         shift
	 set bwt="b$1"
	 breaksw
      case "--stopgrams":
         shift
	 set swt="s$1"
	 breaksw
      case "--smooth":
         shift
	 set smoothing="$1"
	 breaksw
      case "--overhead":
         set method="overhead"
	 breaksw
      case "--raw-only":
         set raw_only
	 breaksw
      case "--langdetect":
         set method="langdetect"
	 shift
	 set lddir="$1"
	 breaksw
      case "--ldmem":
	 shift
	 set ldmem="$1"
	 breaksw
      case "--ldminmem":
	 shift
	 set ldminmem="$1"
	 breaksw
      case "--mguesser":
         set method="mguesser"
	 shift
	 set mgdir="$1"
	 breaksw
      case "--textcat":
         set method="textcat"
	 shift
	 set tcdir="$1"
	 breaksw
      case "--langid.py":
         set method="langid.py"
	 shift
	 if ( "x$1" != "x" ) set lpymodel="$1"
	 breaksw
      case "--yali":
         set method="YALI"
         shift
         set yalidir="$1"
	 breaksw
      case "--fpsize":
	 shift
         set textcat_fp="-t$1"
	 breaksw
      case "--min":
         shift
         set minlines="$1"
	 breaksw
      case "--max":
         shift
         set maxlines="$1"
	 breaksw
      case "--minlen":
         shift
         set minlen="$1"
	 breaksw
      case "--maxlen":
         shift
         set maxlen="$1"
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
      default:
         echo "Unknown option $1"
	 breaksw
   endsw
   shift
end

if ( $?batch_mode && "x$convertto" =~ xutf16* ) then
   echo "can't combine --batch and --utf16be/le"
   exit 1
endif

if ( "x$prep" == x ) then
   echo Did not find prepare_file.sh.
   exit 1
endif

switch ("$method")
   case "langdetect":
       if ( "x$ldjar" == x ) then
          echo Did not find langdetect.jar.
	  exit 1
       endif
       if ( "x$lddir" == x || ! -e "$lddir" ) then
          echo "Language model '$lddir' does not exist."
	  exit 2
       endif
       breaksw
   case "langid.py":
       if ( "x$langidpy" == x ) then
          echo Did not find langid-rb.py.
	  exit 1
       endif
       if ( "x$lpymodel" == x || ! -e "$lpymodel" ) then
          echo "Language model '$lpymodel' does not exist."
	  exit 2
       endif
       breaksw
   case "mguesser":
       if ( "x$mguesser" == x ) then
          echo Did not find mguesser.
	  exit 1
       endif
       if ( "x$mgdir" == x || ! -e "$mgdir" ) then
          echo "Language model '$mgdir' does not exist."
	  exit 2
       endif
       breaksw
   case "textcat":
       if ( "x$textcat" == x ) then
          echo Did not find testtextcat.
	  exit 1
       endif
       if ( "x$tcdir" == x || ! -e "$tcdir" ) then
          echo "Language model '$tcdir' does not exist."
	  exit 2
       endif
       breaksw
   case "whatlang":
       if ( "x$whatlang" == x ) then
          echo Did not find whatlang.
	  exit 1
       endif
       if ( ! $?raw_only && "x$strings" == x ) then
          echo Did not find la-strings.
	  exit 1
       endif
       if ( "x$db" == x || ! -e "$db" ) then
          echo "Language model '$db' does not exist."
	  exit 2
       endif
       breaksw
   case "YALI":
       if ( "x$yali" == x ) then
          echo Did not find yali-identifier.
	  exit 1
       endif
       if ( "x$yalimod" == x ) then
          echo Did not find yali-models.sh.
	  exit 1
       endif
       if ( "x$yalidir" == x || ! -e "$yalidir" ) then
          echo "Language model '$yalidir' does not exist."
	  exit 2
       endif
       breaksw
endsw

## set LangDetect's random seed to 0 so that the results won't vary from run to run
set langdetect="$java -Xms${ldminmem}m -Xmx${ldmem}m $ldopts -jar $ldjar --detectlang -s 0"
if ("$bwt" != "" || "$swt" != "") then
   set weights="-W$bwt,$swt"
endif

@ files = 0
@ totalraw = 0
@ totalsm = 0
@ totallines = 0
@ rawerr = 0
@ smootherr = 0
@ totalmiss = 0
@ totalextra = 0

if ( "$equiv" != "cat" ) then
cat >"$TMP/eq$$" <<EOF
s/^acm/ar/
s/^apd/ar/
s/^ary/ar/
s/^arz/ar/
s/^shu/ar/
s/^gsw/de/
s/^pfl/de/
s/^pdc/de/
s/^bar/de/
s/^vls/nl/
s/^zea/nl/
s/^jai/jac/
s/^id	/zsm	/
s/^bs	/hr	/
s/^p[er]s/fas/
s/^glk/fas/
s/^kpv/kv/
s/^kaz/kk/
s/^tz[cesu]/tzz/
s/^qu[bfghjlptwyz]/qu/
s/^qv[cehimnoswz]/qu/
s/^qwh/qu/
s/^qx[hnor]/qu/
s/azz/nah/
s/nc[hjl]/nah/
s/ngu/nah/
s/nh[eiw]/nah/
s/^csy/ctd/
s/^pck/ctd/
s/^zom/ctd/
s/^yue/cmn/
s/^zh	/cmn	/
s/^wuu/cmn/
s/^gan/cmn/
s/^plt/mg/
s/^tpi/pe/
s/^pcm/pe/
s/^ckb/ku/
s/^kmr/ku/
s/^sdh/ku/
s/^tot/tku/
s/^acc/acr/
EOF
### Egyptian, Chadian, and Moroccan Arabic -> Arabic (MSA)
### SwissGerman, Bavarian, Palatinate, PADutch -> German
### Vlaams, Zealandic -> Dutch
### Indonesian -> Standard Malay
### Bosnian -> Croatian
### Eastern/Western Farsi, Gilaki -> Farsi macro-language
### Komi-Zyrian -> Komi
### various Tzotzils -> Tzotzil Zinacantan
### all Quechuas and Quichuas -> Quechua macro-language
### all Nahuatls -> Aztecan
### Sorani(Central) Kurdish and Kurmanji (Northern Kurdish) -> Kurdish macro-l
### Sizang/Paite Chin and Zomi -> Tedim Chin (all Northern Kuki-Chin languages)
### Wu/Gan Chinese and Cantonese -> Mandarin Chinese
### Plateau Malagasy -> Malagasy
### Tok Pisin and Nigerian Pidgin to Pidgin English
### Patla-Chicontla Totonac (obsoleted ISO) -> Upper Necaxa Totonac
### Achi-de-Cubulco (obsoleted ISO) -> Achi Rabinal
endif

if ( $method == "langdetect" ) then
# reformat output and remove model-specific digits
cat >"$TMP/eq_$$" <<EOF
s@^[^:]*:\[\([^:]*\):[0-9.]*\]\$@\1	@
s@^\([^	0-9]*\)[0-9][0-9]*	@\1	@
EOF
endif

if ( $method == "langid.py" ) then
# reformat output and remove model-specific digits
cat >"$TMP/eq_$$" <<EOF
s@^[(]'\([^']*\)',.*[)]\$@\1	@
s@^\([^	0-9]*\)[0-9][0-9]*	@\1	@
EOF
endif

if ( $method == "YALI" ) then
   # build the model list if it doesn't yet exist
   if ( ! -e "$yalidir/models" ) then
      "$yalimod" "$yalidir"
   endif
   # ensure non-empty ids and a tab on each line so that subsequent
   #   processing works correctly; also remove model-specific digits
   cat >"$TMP/eq_$$" <<EOF
s@^\$@UNK@
s@\$@	@
s@^\([^	0-9]*\)[0-9][0-9]*	@\1	@
EOF
   if ( $?twoletter ) then
      cat >>"$TMP/eq_$$" <<EOF
s@^afr@af@
#s@^@am@
s@^arg@an@
s@^ara@ar@
#s@^@ay@
#s@^@az@
#s@^@ba@
s@^bul@bg@
#s@^@bn@
#s@^@bo@
#s@^@br@
s@^bos@bs@
s@^cat@ca@
#s@^@ce@
#s@^@ch@
#s@^@co@
s@^ces@cs@
#s@^@cv@
s@^cym@cy@
s@^dan@da@
s@^deu@de@
#s@^@dv@
#s@^@el@
s@^eng@en@
#s@^@eo@
s@^spa@es@
s@^est@et@
s@^eus@eu@
s@^fin@fi@
#s@^@fo@
s@^fra@fr@
#s@^@ga@
#s@^@gd@
#s@^@gl@
#s@^@gu@
#s@^@gv@
s@^hat@ha@
s@^heb@he@
#s@^@hi@
s@^hrv@hr@
#s@^@ht@
#s@^@hu@
s@^hye@hy@
#s@^@id@
#s@^@ig@
#s@^@io@
s@^isl@is@
s@^ita@it@
#s@^jpn@ja@
#s@^@jv@
#s@^@ka@
#s@^@kg@
#s@^@kk@
#s@^@kl@
#s@^@kn@
s@^kor@kr@
#s@^@ku@
#s@^@kv@
#s@^@kw@
#s@^@ky@
#s@^@la@
#s@^@lt@
#s@^@lv@
#s@^@mg@
#s@^@mi@
s@^mkd@mk@
#s@^@ml@
#s@^@mn@
#s@^@mr@
#s@^@mt@
#s@^@my@
#s@^@nd@
#s@^@ne@
#s@^@nl@
s@^nno@nn@
s@^nor@no@
#s@^@oc@
s@^oss@os@
#s@^@pa@
s@^pol@pl@
s@^por@pt@
s@^ron@ro@
s@^rus@ru@
#s@^@rw@
#s@^@sc@
#s@^@se@
#s@^@si@
s@^slk@sk@
s@^slv@sl@
#s@^@so@
s@^srp@sr@
#s@^@st@
#s@^@su@
s@^swe@sv@
s@^swa@sw@
s@^tam@ta@
s@^tel@te@
#s@^@tg@
s@^tha@th@
#s@^@ti@
#s@^@tk@
s@^tgl@tl@
#s@^@tn@
#s@^@to@
s@^tur@tr@
#s@^@ts@
#s@^@ug@
s@^ukr@uk@
s@^urd@ur@
s@^uzb@uz@
s@^vie@vi@
#s@^@vo@
#s@^@wa@
#s@^@wo@
s@^xho@xh@
s@^yid@yi@
s@^yor@yo@
s@^zho@zh@
s@^zul@zu@
EOF
   endif
endif

if ( $?smoothing ) then
   switch ( $method )
      case "overhead":
         breaksw
      case "langdetect":
          set smoothing="-S $smoothing"
	  breaksw
      case "langid.py":
          echo Frequency smoothing ignored for langid.py -- Not yet implemented
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

setenv LC_ALL C
if ( $?batch_mode ) then
   rm -f "$tmp3" "$tmpkey"
   foreach file ($*)
      "$prep" "$file" "$tmp1" "$minlines" "$maxlines" "$minlen" "$maxlen" N "$convertto" "$embed"
      if ( -e "$tmp1" ) then
         sed -e 's/	.*/	/' "$tmp1" | $equiv >>"$tmpkey"
         sed -e 's/^[^	]*	//' "$tmp1" >>"$tmp3"
         rm -f "$tmp1"
      endif
   end
   switch ( $method )
      case "overhead":
         paste "$tmpkey" "$tmp3" | $equiv | $keyonly >"$tmp2"
	 breaksw
      case "langdetect":
	 $langdetect $langdetect_len -d $lddir $smoothing "$tmp3" | $ldequiv | $equiv | $keyonly >"$tmp2"
	 breaksw
      case "langid.py":
         $langidpy $langidpy_len -m "$lpymodel" <"$tmp3" | $lpyequiv | $equiv | $keyonly >"$tmp2"
	 breaksw
      case "mguesser":
         "$mguesser" -d$mgdir $mguesser_len -n1 $smoothing <"$tmp3" | $equiv | $keyonly >"$tmp2"
	 breaksw
      case "textcat":
         "$textcat" ${tcdir}/conf.txt $textcat_len $textcat_fp $smoothing <"$tmp3" | $equiv | $keyonly >"$tmp2"
	 breaksw
      case "YALI":
         "$yali" --classes "$yalidir/models" $smoothing --each --input "$tmp3" | $yaliequiv | $equiv | $keyonly >"$tmp2"
	 breaksw
      case "whatlang":
	 if ( ! $?raw_only && ${whatlang_len} == 1 ) then
	    $whatlang -t -b2 -n1 "-l$db" $cvt $weights "$tmp3" | $equiv | $keyonly >"$tmp2"
	 else
	    $whatlang -t -b${whatlang_len} -n1 "-l$db" $cvt $weights "$tmp3" | $equiv | $keyonly >"$tmp2"
	 endif
	 breaksw
      default:
         echo "Unknown method '$method'"
	 exit 1
	 breaksw
   endsw
   $score "$tmpkey" "$tmp2"
   if ( $?keep_extract ) then
      mv -n "$tmp2" "identified-languages"
   endif
   rm -f "$tmp2" "$tmp3" "$tmpkey"
   exit 0
endif

foreach file ($*)
   "$prep" "$file" "$tmp1" "$minlines" "$maxlines" "$minlen" "$maxlen" Y "$convertto" "$embed"
   if ( ! -e "$tmp1" ) continue
   if ( "x$convertto" =~ xutf16* ) then
      set lines=`iconv -f $convertto -t utf8 <"$tmp1" | wc -l`
   else
      set lines=`wc -l <"$tmp1"`
   endif
   set langr=`head -1 "$file"| $equiv | sed -e 's/	.*//'`
   set langs=`head -1 "$file"| $equiv | sed -e 's/	.*//' -e 's/^\(.......\).*/\1/'`
   switch ($method)
      case "overhead":
         set raw=`cat <"$tmp1" | $equiv | grep -vc "^${langr}	"`
	 breaksw
      case "langdetect":
         set raw=`$langdetect $langdetect_len -d $lddir $smoothing "$tmp1" | $ldequiv | $equiv | grep -vc "^${langr}	"`
	 breaksw
      case "langid.py":
         set raw=`$langidpy $langidpy_len -m "$lpymodel" <"$tmp1" | $lpyequiv | $equiv | grep -vc "^${langr}	"`
	 breaksw
      case "mguesser":
         set raw=`"$mguesser" -d$mgdir $mguesser_len -n1 $smoothing <"$tmp1" | $equiv | grep -vc "^${langr}	"`
	 breaksw
      case "textcat":
         set raw=`"$textcat" ${tcdir}/conf.txt $textcat_len $textcat_fp $smoothing <"$tmp1" | $equiv | grep -vc "^${langr}	"`
	 breaksw
      case "YALI":
         set raw=`"$yali" --classes "$yalidir/models" $smoothing --each --input "$tmp1" | $yaliequiv | $equiv | grep -vc "^${langr}	"`
	 breaksw
      case "whatlang":
         set raw=`$whatlang -t -b${whatlang_len} -n1 "-l$db" $cvt $weights "$tmp1" | $equiv | grep -vc "^${langr}	"`
	 breaksw
      default:
         echo "Unknown method '$method' (2)"
	 exit 1
	 breaksw
   endsw
   if ( $?show_misses ) then
      $strings -i$db -I1 $thresh -u $weights "$tmp1" | $equiv >$str
      set smoothed=`grep -vc "^${langs}	" <$str`
      @ extracted = `wc -l <$str`
      if ( $extracted <= $lines ) then
         @ miss = $lines - $extracted
         @ extra = 0
      else
         @ miss = 0
         @ extra = $extracted - $lines
      endif
   else if ( $method == "whatlang" && ! $?raw_only ) then
      $whatlang -t -b2 -n1 "-l$db" $cvt $weights "$tmp1" | $equiv >$str
      set smoothed=`grep -vc "^${langr}	" <$str`
      @ miss = 0
      @ extra = 0
   else
      set smoothed=0
      @ miss = 0
      @ extra = 0
   endif
   rm -f "$tmp1"
   if ( $?keep_extract ) then
      mv -n $str ${file}.str
      rm -f $str >/dev/null
   else
      rm -f $str
   endif
   set l=$lines
   if ($l == 0) set l=1
   set percentr=`perl -e "printf "\""%.2f"\"", 100.0 * ${raw} / ${l}"`
   set percents=`perl -e "printf "\""%.2f"\"", 100.0 * ${smoothed} / ${l}"`
   if ( $?show_misses ) then
      echo "${file:t:r}: (${langr}) ${raw}/${smoothed} of ${lines} incorrect (${percentr}/${percents}%), $miss misses, $extra extra"
   else
      echo "${file:t:r}: (${langr}) ${raw}/${smoothed} of ${lines} incorrect (${percentr}/${percents}%)"
   endif
   set r=`echo ${percentr:s/.//}|sed -e 's/^0*\(.\)/\1/'`
   set s=`echo ${percents:s/.//}|sed -e 's/^0*\(.\)/\1/'`
   @ totalraw = $totalraw + ${r}
   @ totalsm = $totalsm + ${s}
   @ files = $files + 1
   @ rawerr = $rawerr + $raw
   @ smootherr = $smootherr + $smoothed
   @ totallines = $totallines + $lines
   @ totalmiss = $totalmiss + $miss
   @ totalextra = $totalextra + $extra
end
if ($totallines == 0) set totallines = 1
if ($files != 0) then
   set err_r=`perl -e "printf "\""%.3f"\"", ${totalraw} / ${files} * 0.01"`
   set err_s=`perl -e "printf "\""%.3f"\"", ${totalsm} / ${files} * 0.01"`
   echo "Processed ${files} files, macro-average error rates = $err_r/${err_s}%"
   set rawrate=`perl -e "printf "\""%.3f"\"", 100.0 * ${rawerr} / ${totallines}"`
   set smoothrate=`perl -e "printf "\""%.3f"\"", 100.0 * ${smootherr} / ${totallines}"`
   set missrate=`perl -e "printf "\""%.3f"\"", 100.0 * ${totalmiss} / ${totallines}"`
   set false=`perl -e "printf "\""%.3f"\"", 100.0 * ${totalextra} / ${totallines}"`
   echo "Total errors: $rawerr/$smootherr of $totallines (${rawrate}/${smoothrate}%)"
   if ( $?show_misses ) then
      echo "Overall misses = ${totalmiss} (${missrate}%), false alarms = ${false}%"
   endif
endif

cleanup:
rm -f "$TMP/eq$$" "$TMP/eq_$$" "$TMP/eval$$" "$tmp3" "$tmpkey"

exit 0

