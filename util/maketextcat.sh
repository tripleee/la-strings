#!/bin/csh -f
set maxbytes=2000000  ## best amount to use according to devtest
set topn=500
set createfp="/home/ralf/src/libtextcat/libtextcat++/src/createfp"
set dir=textcat-lm
set me="$0"
set base="train"
set use_subsample=""
set quiet=""
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
      case '-t':
         shift
	 set topn="$1"
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
	 echo "  $0:t [-b N] [-d DIR] [-t N]"
	 echo "Options:"
	 echo "  -b N    restrict training to first N bytes (default $maxbytes)"
	 echo "  -d DIR  store models in directory DIR (default $dir)"
	 echo "  -s	 subsample instead of using strictly the first N bytes"
	 echo "  -t N    use top N n-grams for model (default $topn)"
	 exit 1
   endsw
   shift
end

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
   set lng="$lang"
   if ( -e "$dir/${lang}.${base}.${cs}.lm" ) then
      foreach i (`seq 1 99`)
         if ( ! -e "$dir/${lang}${i}.${base}.${cs}.lm" ) then
	   set lng="${lang}$i"
	   break
	 endif
      end
   endif
   if ( $quiet == "" ) echo createfp $topn -- cs= $cs l= $lang -- $base
   $xpand "$file" | $subsample | head -c $maxbytes | \
       $createfp $topn >"$dir/${lng}.${base}.${cs}.lm"
   echo "${lng}.${base}.${cs}.lm	${lang}" >>"$dir/conf.txt"
   if ( $cs != utf8  && $cs != ASCII && $cs != ISCII ) then
      set from_cs=$cs
      if ( $cs == latin-1) set from_cs=iso-8859-1
      if ( $cs == latin-2) set from_cs=iso-8859-2
      if ( $cs == latin-5) set from_cs=iso-8859-5

      set lng="$lang"
      if ( -e "$dir/${lang}.${base}.utf8.lm" ) then
         foreach i (`seq 1 99`)
            if ( ! -e "$dir/${lang}${i}.${base}.utf8.lm" ) then
 	       set lng="${lang}$i"
	       break
	    endif
         end
      endif
      if ( $quiet == "" ) echo createfp $topn -- cs= utf8 l= $lang -- $base
      $xpand "$file" | iconv -f $from_cs -t utf-8//TRANSLIT//IGNORE | \
	  head -c $maxbytes | \
	  $createfp $topn >"$dir/${lng}.${base}.utf8.lm"
      echo "${lng}.${base}.utf8.lm	${lang}" >>"$dir/conf.txt"
   endif

   if ( $cs == utf8 && $?make_latin1 ) then
      set lng="$lang"
      if ( -e "$dir/${lang}.${base}.latin1.lm" ) then
         foreach i (`seq 1 99`)
            if ( ! -e "$dir/${lang}${i}.${base}.latin1.lm" ) then
 	       set lng="${lang}$i"
	       break
	    endif
         end
      endif
      if ( $quiet == "" ) echo createfp $topn -- cs= latin1 l= $lang -- $base
      $xpand "$file" | iconv -f utf-8 -t iso-8859-1//TRANSLIT//IGNORE | \
	  head -c $maxbytes | \
	  $createfp $topn >"$dir/${lng}.${base}.latin1.lm"
      echo "${lng}.${base}.latin1.lm	${lang}" >>"$dir/conf.txt"
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

   "$me" -b $maxbytes -t $topn $quiet -d "$dir" -c "$cs" -l "$lang" -f "$i" -n "$base" $use_subsample
end

"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l abt-maprik -f Ambulas_Maprik-train.utf8.gz -n Maprik
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l abt-wosera -f Ambulas_Wosera-Kamu-train.utf8.gz -n Wosera
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l aoj-filifita -f Filifita-train.utf8.gz -n Filifita
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l rwo-karo -f Karo_Rawa-train.utf8.gz -n Karo
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l rwo-rawa -f Rawa-train.utf8.gz -n Rawa
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l snp-lambau -f Slane-Lambau-train.utf8.gz -n Slane-Lambau
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l knv-aramia -f Tabo_Aramia-train.utf8.gz -n Tabo_Aramia
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l knv-flyriver -f Tabo_Fly_River-train.utf8.gz -n Tabo_Fly_River
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l ubu-andale -f Umbu-Ungu_Andale-train.utf8.gz -n Andale
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l ubu-kala -f Umbu-Ungu_Kala-train.utf8.gz -n Kala
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l wed-topura -f Wedau_Topura-train.utf8.gz -n Wedau_Topura
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l yss-yamano -f Yamano_Yessan-Mayo-train.utf8.gz -n Yamano
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l yss-yawu -f Yawu_Yessan-Mayo-train.utf8.gz -n Yawu
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l hi -f hi_IN.Hindi-bible-train.utf8.gz -n Hindi-Bible
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l lif -f lif_NP.Limbu-deva-train.utf8.gz -n Limbu-Devanagari
"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l jiv -f jiv_EC.Shuar-train.utf8.gz -n Shuar -latin1

#"$me" -b $maxbytes -t $topn $quiet -d "$dir" -c utf8 -l 

exit 0
