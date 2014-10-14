#!/bin/csh -f
set maxbytes=1500000
set train="python langid/train/train.py"
set dir=langidpy-model
set modeldir=""
set me="$0"
set quiet=""
set base="train"
set use_subsample=""
set num_feat=400
set maxn=6
set threads=4
set buckets=128
set chsize=8
set numtypes=10000
set no_domain_ig="--no_domain_ig"
set split=""
unset per_encoding
unset run_training
unset make_latin1
unset file
unset cs
unset lang

while ($#argv > 0)
   switch ($1)
      case '-b':
         shift
	 set maxbytes="$1"
	 breaksw
      case '-d':
         shift
	 set dir="$1"
	 breaksw
      case '-q':
	 set quiet="-q"
	 breaksw
      case '-s':
         set use_subsample="-s"
	 breaksw
      case '-S':
	 shift
	 set split="split -l$1"
	 setenv MAKELANGIDPY_SPLIT "$1"
	 breaksw
      case "-train":
	 # run the actual training, assume data has already been prepared
	 shift
	 set run_training
	 set modeldir="$1"
	 breaksw
      case "-feat":
      case "-features":
	 shift
	 set num_feat="$1"
	 breaksw
      case "-maxn":
	 shift
	 set maxn="$1"
	 breaksw
      case "-per_enc":
      case "-per_encoding":
	 set per_encoding
	 breaksw
      case "-enc_ig":
	 set no_domain_ig=""
	 breaksw
      case "-buckets":
	 shift
	 set buckets="$1"
	 breaksw
      case "-types":
	 shift
	 set numtypes="$1"
	 breaksw
      case "-j":
	 shift
	 set threads="$1"
	 breaksw
## internal options for sub-invocation
      case '-c':
	 shift
	 set cs="$1"
	 breaksw
      case '-f':
	 shift
	 set file="$1"
	 breaksw
      case '-l':
	 shift
	 set lang="$1"
	 breaksw
      case '-latin1':
	 set make_latin1
	 breaksw
      case '-n':
	 shift
	 set base="$1"
	 breaksw
      default:
         echo "Unrecognized option: $1"
	 echo "Usage:"
	 echo "  $0:t [-b N] [-d DIR] [-s] [-per_enc]"
	 echo "  $0:t -train MDIR [-d DIR] [-maxn N] [-feat K]"
	 echo "Options:"
	 echo "  -b N      restrict training to first N bytes (default $maxbytes)"
	 echo "  -d DIR    store prepared training data in directory DIR (default $dir)"
	 echo "  -s	   subsample instead of using strictly the first N bytes"
	 echo "  -per_enc  distinguish between different encodings"
	 echo "  -train MDIR    run actual model training, storing result in MDIR"
	 echo "     -maxn N     ...with longest ngram set to N (default $maxn)"
         echo "     -feat K     ...using K features per language (default $num_feat)"
	 echo "     -types T	...from top T types per n-gram order (default $numtypes)"
	 echo "     -enc_ig     ...computing inter-encoding information gain"
         echo "     -j N        ...running with N threads (default $threads)"
	 echo "     -buckets B  ...using B buckets for features (default $buckets)"
	 exit 1
   endsw
   shift
end

if ( "x$split" == "x" && $?MAKELANGIDPY_SPLIT ) then
   set split="split -l$MAKELANGIDPY_SPLIT"
endif

## is this the final training run?
if ( $?run_training ) then
   if ( "x$modeldir" == "x" ) then
      set modeldir="${dir}.model"
   endif

   if ( ! -e $dir ) then
      "$me" -b $maxbytes -d "$dir" $use_subsample
   endif
   echo Running langid.py training script
   $train --model $modeldir --feats_per_lang $num_feat --max_order $maxn --buckets $buckets --chunksize $chsize --df_tokens $numtypes $no_domain_ig -j $threads $dir
   exit
endif

set domain="any_encoding"

## is this a sub-invocation?
if ( $?file && $?cs && $?lang && $?base ) then

   set xpand=zcat
   if ( $file:e == xz ) set xpand=xzcat
   if ( x$use_subsample != x ) then
      set samp = `perl -e 'print 1.05 * '$maxbytes' . "\n"'`
      set subsample="subsample -b $samp"
      if ( -e ./subsample ) set subsample="./subsample -b $samp"
   else
      set subsample=cat
   endif
   if ( $?per_encoding) set domain="$cs"
   set lng="$lang"
   if ( -e "$dir/$domain/$lang" ) then
      foreach i (`seq 1 99`)
         if ( ! -e "$dir/$domain/${lang}$i" ) then
	   set lng="${lang}$i"
	   break
	 endif
      end
   endif
   mkdir -p "$dir/$domain/$lng"
   set dest="$dir/$domain/$lng/${file:t:r:r}.txt"
   if ( $quiet == "" ) echo Preparing -- cs= $cs l= $lang -- $base
   if ( "x$split" == "x" ) then
      $xpand "$file" | $subsample | head -c $maxbytes >"$dest"
   else
      $xpand "$file" | $subsample | head -c $maxbytes | ( cd "$dest:h" ; $split )
   endif
   if ( $cs != utf8  && $cs != ASCII && $cs != ISCII ) then
      set from_cs=$cs
      if ( $cs == latin-1) set from_cs=iso-8859-1
      if ( $cs == latin-2) set from_cs=iso-8859-2
      if ( $cs == latin-5) set from_cs=iso-8859-5

      if ( $?per_encoding) set domain="utf8"
      set lng="$lang"
      if ( -e "$dir/$domain/$lang" ) then
         foreach i (`seq 1 99`)
            if ( ! -e "$dir/$domain/${lang}$i" ) then
	      set lng="${lang}$i"
	      break
	    endif
         end
      endif
      mkdir -p "$dir/$domain/$lng"
      set dest="$dir/$domain/$lng/${file:t:r:r}.txt"
      if ( $quiet == "" ) echo Preparing -- cs= utf8 l= $lang -- $base
      if ( "x$split" == "x" ) then
         $xpand "$file" | iconv -f $from_cs -t utf-8//TRANSLIT//IGNORE | \
   	     head -c $maxbytes >"$dest"
      else
         $xpand "$file" | iconv -f $from_cs -t utf-8//TRANSLIT//IGNORE | \
   	     head -c $maxbytes | ( cd "$dest:h" ; $split )
      endif
   endif

   if ( $cs == utf8 && $?make_latin1 ) then
      if ( $?per_encoding ) set domain="latin-1"
      set lng="$lang"
      if ( -e "$dir/$domain/$lang" ) then
         foreach i (`seq 1 99`)
            if ( ! -e "$dir/$domain/${lang}$i" ) then
	      set lng="${lang}$i"
	      break
	    endif
         end
      endif
      mkdir -p "$dir/$domain/$lng"
      set dest="$dir/$domain/$lng/${file:t:r:r}.txt"
      if ( $quiet == "" ) echo Preparing -- cs= latin1 l= $lang -- $base
      if ( "x$split" == "x") then
         $xpand "$file" | iconv -f utf-8 -t iso-8859-1//TRANSLIT//IGNORE | \
	     head -c $maxbytes >"$dest"
      else
         $xpand "$file" | iconv -f utf-8 -t iso-8859-1//TRANSLIT//IGNORE | \
	     head -c $maxbytes | ( cd "$dest:h" ; $split )
      endif
   endif

   exit 0
endif

mkdir -p "$dir"
setenv LC_ALL C

rm -f "$dir/conf.txt"

foreach i (??_*.[gx]z ???_*.[gx]z ????_??.*.[gx]z ???-?_??.*.[gx]z ???-??_??.*.[gx]z)
   set lang=`echo $i|sed -e 's/_.*//'`
   ## fix up a few mis-named (for historical reasons) files
   if ( $lang == zh ) set lang=cmn
   if ( $lang == esk ) set lang=ipk
   set cs=utf8

   set root="${i:r:r}"
   set base="$root:e"
   while ( $root:r != $root )
      set base="$root:e"
      set root="$root:r"
   end
   set base="$base:s/-train//:s/-bible//"

   if ( "$i" =~ *latin1* ) set cs=latin-1
   if ( "$i" =~ *latin2* ) set cs=latin-2
   if ( "$i" =~ *latin5* ) set cs=latin-5
   if ( "$i" =~ *.big5* ) set cs=big5
   if ( "$i" =~ *.gbk* ) set cs=gbk
   if ( "$i" =~ *cp437* ) set cs=cp437
   if ( "$i" =~ *win1252* ) set cs=cp1252
   if ( "$i" =~ *.ascii* ) set cs=ASCII
   if ( "$i" =~ *.ASCII* ) set cs=ASCII
   if ( "$i" =~ *euc-kr* ) set cs=EUC-KR
   if ( "$i" =~ *euc-jp* ) set cs=EUC-JP-MS
   if ( "$i" =~ *.iscii* ) set cs=ISCII

   "$me" -b $maxbytes -d "$dir" $quiet -c "$cs" -l "$lang" -f "$i" -n "$base" $use_subsample
end

"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l abt-maprik -f Ambulas_Maprik-train.utf8.gz -n Maprik
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l abt-wosera -f Ambulas_Wosera-Kamu-train.utf8.gz -n Wosera
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l aoj-filifita -f Filifita-train.utf8.gz -n Filifita
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l rwo-karo -f Karo_Rawa-train.utf8.gz -n Karo
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l rwo-rawa -f Rawa-train.utf8.gz -n Rawa
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l snp-lambau -f Slane-Lambau-train.utf8.gz -n Slane-Lambau
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l knv-aramia -f Tabo_Aramia-train.utf8.gz -n Tabo_Aramia
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l knv-flyriver -f Tabo_Fly_River-train.utf8.gz -n Tabo_Fly_River
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l ubu-andale -f Umbu-Ungu_Andale-train.utf8.gz -n Andale
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l ubu-kala -f Umbu-Ungu_Kala-train.utf8.gz -n Kala
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l wed-topura -f Wedau_Topura-train.utf8.gz -n Wedau_Topura
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l yss-yamano -f Yamano_Yessan-Mayo-train.utf8.gz -n Yamano
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l yss-yawu -f Yawu_Yessan-Mayo-train.utf8.gz -n Yawu
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l hi -f hi_IN.Hindi-bible-train.utf8.gz -n Hindi-Bible
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l lif -f lif_NP.Limbu-deva-train.utf8.gz -n Limbu-Devanagari
"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l jiv -f jiv_EC.Shuar-train.utf8.gz -n Shuar -latin1

#"$me" -b $maxbytes -d "$dir" $quiet -c utf8 -l 

exit 0