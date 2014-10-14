#!/bin/csh -f
set bytes=1100000
set bytes16=1200000
set num_cpu=`num_cpus`
set mklangid="../../langident/mklangid"
set dbdir="/j/tmp/db"
#set sample=""	# use N contiguous bytes from start of input file
set sample="@"  # subsample input before limiting it to N bytes
set smoothing=""
set smoothvalue=""
set ngramlen=""
set ngramlengths="6+12"

## figure out which makefile and name prefixes to use based on the name
##   under which we were invoked
set make="make"
set prefix="lang"
set logpref="build"
if ( "$0" =~ *-top* ) then
   set make="$make -f makefile-top"
   set prefix="top"
   set logpref="buildtop"
else if ( "$0" =~ *-big8* ) then
   sed -e '/^models:/,/^other.db:/{' -e '/^	.*16[BbLl][Ee]/d' -e '}' makefile-big >makefile-big8
   set make="$make -f makefile-big8"
   set prefix="big8"
   set logpref="buildbig8"
else if ( "$0" =~ *-big* ) then
   set make="$make -f makefile-big"
   set prefix="big"
   set logpref="buildbig"
else if ( "$0" =~ *-8bit* ) then
   sed -e '/^models:/,/^other.db:/{' -e '/^	.*16[BbLl][Ee]/d' -e '}' makefile >makefile-8bit
   set  make="$make -f makefile-8bit"
   set prefix="lang8"
   set logpref="build8"
else if ( "$0" =~ *-devtest8* ) then
   sed -e '/^models:/,/^other.db:/{' -e '/^	.*16[BbLl][Ee]/d' -e '}' makefile-devtest >makefile-devtest8
   set make="$make -f makefile-devtest8"
   set prefix="dev8"
   set logpref="builddev8"
else if ( "$0" =~ *-devtest* ) then
   set make="$make -f makefile-devtest"
   set prefix="dev"
   set logpref="builddev"
endif

# what to build:
set do_all=0
set topK=0
set topK_nostop=0
set len3000=0
set len9000=0
set len15000=0
set maxbytes=0
set filter=0

while ($#argv > 0)
   switch ($1)
      case 'all':
          set do_all=1
	  breaksw
      case 'none':
          set do_all=0 topK=0 topK_nostop=0 len3000=0 len9000=0 len15000=0 maxbytes=0 filter=0
	  breaksw
      case 'topk':
          set topK=1
	  breaksw
      case 'topk_nostop':
          set topK_nostop=1
	  breaksw
      case 'len3000':
          set len3000=1
	  breaksw
      case 'len9000':
	  set len9000=1
	  breaksw
      case 'len15000':
	  set len15000=1
	  breaksw
      case 'bytes':
	  set maxbytes=1
	  breaksw
      case 'filter':
          set filter=1
	  breaksw
      case 'nosample':
          set sample=""
	  breaksw
      case 'fixedN':
          shift
          set ngramlen="MAXN=$1 MAXN16=$1 MAXN24=$1"
	  set ngramlengths="$1"
	  breaksw
      case 'smoothing':
          shift
	  set smoothing="SMOOTHING=-S$1"
	  set smoothvalue="-S$1"
	  breaksw
      case 'threads':
      	  shift
	  set num_cpu="$1"
	  breaksw
      case 'help':
      case '-h':
      case '--help':
	  echo "Valid build options are 'all', 'none', 'topk', 'len3000', 'len9000', 'len15000', 'bytes',"
	  echo "  and 'filter'."
	  echo "In addition, 'nosample' modifies 'bytes' to use the first N bytes instead"
	  echo "  of subsampling."
	  exit 1
	  breaksw
      default:
          echo "Unknown option '$1'"
	  echo "Valid options are 'all', 'none', 'topk', 'len3000', 'len9000', 'len15000', 'bytes',"
	  echo "  and 'filter'."
	  exit 1
	  breaksw
   endsw
   shift
end

set make="$make -j$num_cpu "

if ($do_all) then
   set topK=1
   set len3000=1
   set len9000=1
   set maxbytes=1
   set filter=1
endif

if ($topK) then
#foreach i (30000 15000 12000 10000 9000 8000 7000 6000 5500 5000 4500 4250 4000 3750 3500 3250 3000 2750 2500 2250 2000 1750 1500 1250 1000 900 800 700 600 500 400 300 250 200)
foreach i (5600 5000 4500 4000 3500 3250 3000 2750 2500 2250 2000 1750 1500 1250 1000 900 800 700 600 500 400 300 250 200)
   make allclean >&/dev/null
   echo " ===> $ngramlengths $i ${bytes}"
   set log="${dbdir}/${logpref}-${ngramlengths}-$i-${bytes}.log"
   set db="${dbdir}/${prefix}-${ngramlengths}-$i${smoothvalue}-${bytes}.db"
   time $make "MAXBYTES=-L${sample}${bytes}" "MAXBYTES16=-L${sample}${bytes16}" $smoothing $ngramlen "TOPK=-k$i" "TOPK16=-k$i" "TOPK_CJK=-k$i" >"${log}"
   rm -f "$db"
   time $mklangid "=$db" -f $smoothvalue *.lid >>"${log}"
   # $mklangid "=${db}" -f $smoothvalue \
   # 		{zh,gan,hak,cmn,nan,wuu,hsn,yue,es,en,ar,arz}.*.lid \
   # 		{hi,bn,pt,ru,ja,de,jv,pn,te,vi,mr,fr,kr,ta,it}.*.lid \
   # 		{ur,tr,gu,pl,mly,bh,bho,awa,uk,ml,kn,mai,su}.*.lid \
   # 		{my,ori,fa,prs,pes,rwr,pa,fil,ha,tl,ro,id,nl}.*.lid \
   # 		{snd,th,pst,uz,raj,hoj,mup,yo,az,azb,azj,ig}.*.lid \
   # 		{am,hne,gaz,gax,hae,asm,hr,sr,ku,ckb,kmr,sdh}.*.lid \
   # 		{ceb,si,rkt,tts,zha,mg,ne,so,khm,mad,bar,el}.*.lid \
   # 		{ctg,bgc,mag,dcc,hu,ful,fub,fuc,ca,sna,zu,syl}.*.lid \
   # 		{quf,quh,que,bjj,cs,lmo,bg,ug,nya,bel,kk,kaz}.*.lid \
   # 		{se,aka,xh,bfy,ht,kok,kin,kik,nap,bal,bcc,ilo}.*.lid \
   # 		{vah,pnb,tk,da,he,sl,sk,fi,no,lt}.*.lid >>&"$log"
end
endif

if ($topK_nostop) then
#foreach i (30000 15000 12000 10000 9000 8000 7000 6000 5500 5000 4500 4250 4000 3750 3500 3250 3000 2750 2500 2250 2000 1750 1500 1250 1000 900 800 700 600 500 400 300 250 200)
foreach i (5600 5000 4500 4000 3500 3250 3000 2750 2500 2250 2000 1750 1500 1250 1000 900 800 700 600 500 400 300 250 200)
   make allclean >&/dev/null
   echo " ===> $ngramlengths $i ${bytes}"
   set log="${dbdir}/${logpref}-${ngramlengths}-$i${smoothvalue}-nostop-${bytes}.log"
   set db="${dbdir}/${prefix}-${ngramlengths}-$i${smoothvalue}-nostop-${bytes}.db"
   time $make "MAXBYTES=-L${sample}${bytes}" "MAXBYTES16=-L${sample}${bytes16}" $smoothing $ngramlen "TOPK=-k$i" "TOPK16=-k$i" "TOPK_CJK=-k$i" "SIM=1.0" "SIMCJK=1.0" "SIMPG=1.0" "SIM16=1.0" "SIMMAX=1.0" >"${log}"
   rm -f "$db"
   time $mklangid "=$db" -f $smoothvalue *.lid >>"${log}"
   # $mklangid "=${db}" -f $smoothvalue \
   # 		{zh,gan,hak,cmn,nan,wuu,hsn,yue,es,en,ar,arz}.*.lid \
   # 		{hi,bn,pt,ru,ja,de,jv,pn,te,vi,mr,fr,kr,ta,it}.*.lid \
   # 		{ur,tr,gu,pl,mly,bh,bho,awa,uk,ml,kn,mai,su}.*.lid \
   # 		{my,ori,fa,prs,pes,rwr,pa,fil,ha,tl,ro,id,nl}.*.lid \
   # 		{snd,th,pst,uz,raj,hoj,mup,yo,az,azb,azj,ig}.*.lid \
   # 		{am,hne,gaz,gax,hae,asm,hr,sr,ku,ckb,kmr,sdh}.*.lid \
   # 		{ceb,si,rkt,tts,zha,mg,ne,so,khm,mad,bar,el}.*.lid \
   # 		{ctg,bgc,mag,dcc,hu,ful,fub,fuc,ca,sna,zu,syl}.*.lid \
   # 		{quf,quh,que,bjj,cs,lmo,bg,ug,nya,bel,kk,kaz}.*.lid \
   # 		{se,aka,xh,bfy,ht,kok,kin,kik,nap,bal,bcc,ilo}.*.lid \
   # 		{vah,pnb,tk,da,he,sl,sk,fi,no,lt}.*.lid >>&"$log"
end
endif

if ($len3000) then
foreach i (12 11 10 9 8 7 6 5 4 3)
   make allclean >&/dev/null
   echo " ===> $i 3000 ${bytes}"
   set log="${dbdir}/${logpref}-$i-3000-${bytes}.log"
   set db="${dbdir}/${prefix}-$i-3000${smoothvalue}-${bytes}.db"
   time $make "MAXBYTES=-L${sample}${bytes}" "MAXBYTES16=-L${sample}${bytes16}" "MAXN=$i" "MAXN16=$i" "MAXN24=$i" $smoothing "TOPK=-k3000" "TOPK16=-k3000" "TOPK_CJK=-k3000" >"$log"
   rm -f "$db"
   $mklangid "=${db}" -f $smoothvalue *.lid >>"$log"
   # $mklangid "=${db}" -f $smoothvalue \
   # 		{zh,gan,hak,cmn,nan,wuu,hsn,yue,es,en,ar,arz}.*.lid \
   # 		{hi,bn,pt,ru,ja,de,jv,pn,te,vi,mr,fr,kr,ta,it}.*.lid \
   # 		{ur,tr,gu,pl,mly,bh,bho,awa,uk,ml,kn,mai,su}.*.lid \
   # 		{my,ori,fa,prs,pes,rwr,pa,fil,ha,tl,ro,id,nl}.*.lid \
   # 		{snd,th,pst,uz,raj,hoj,mup,yo,az,azb,azj,ig}.*.lid \
   # 		{am,hne,gaz,gax,hae,asm,hr,sr,ku,ckb,kmr,sdh}.*.lid \
   # 		{ceb,si,rkt,tts,zha,mg,ne,so,khm,mad,bar,el}.*.lid \
   # 		{ctg,bgc,mag,dcc,hu,ful,fub,fuc,ca,sna,zu,syl}.*.lid \
   # 		{quf,quh,que,bjj,cs,lmo,bg,ug,nya,bel,kk,kaz}.*.lid \
   # 		{se,aka,xh,bfy,ht,kok,kin,kik,nap,bal,bcc,ilo}.*.lid \
   # 		{vah,pnb,tk,da,he,sl,sk,fi,no,lt}.*.lid >>&"$log"
end
endif

if ($len9000) then
foreach i (12 11 10 9 8 7 6 5 4 3)
   make allclean >&/dev/null
   echo " ===> $i 9000 ${bytes}"
   set log="${dbdir}/${logpref}-$i-9000-${bytes}.log"
   set db="${dbdir}/${prefix}-$i-9000${smoothvalue}-${bytes}.db"
   time $make "MAXBYTES=-L${sample}${bytes}" "MAXBYTES16=-L${sample}${bytes}" "MAXN=$i" "MAXN16=$i" "MAXN24=$i" $smoothing "TOPK=-k9000" "TOPK16=-k9000" "TOPK_CJK=-k9000" >"$log"
   rm -f "$db"
   $mklangid "=${db}" -f $smoothvalue *.lid >>"$log"
   # $mklangid "=${db}" -f $smoothvalue \
   # 		{zh,gan,hak,cmn,nan,wuu,hsn,yue,es,en,ar,arz}.*.lid \
   # 		{hi,bn,pt,ru,ja,de,jv,pn,te,vi,mr,fr,kr,ta,it}.*.lid \
   # 		{ur,tr,gu,pl,mly,bh,bho,awa,uk,ml,kn,mai,su}.*.lid \
   # 		{my,ori,fa,prs,pes,rwr,pa,fil,ha,tl,ro,id,nl}.*.lid \
   # 		{snd,th,pst,uz,raj,hoj,mup,yo,az,azb,azj,ig}.*.lid \
   # 		{am,hne,gaz,gax,hae,asm,hr,sr,ku,ckb,kmr,sdh}.*.lid \
   # 		{ceb,si,rkt,tts,zha,mg,ne,so,khm,mad,bar,el}.*.lid \
   # 		{ctg,bgc,mag,dcc,hu,ful,fub,fuc,ca,sna,zu,syl}.*.lid \
   # 		{quf,quh,que,bjj,cs,lmo,bg,ug,nya,bel,kk,kaz}.*.lid \
   # 		{se,aka,xh,bfy,ht,kok,kin,kik,nap,bal,bcc,ilo}.*.lid \
   # 		{vah,pnb,tk,da,he,sl,sk,fi,no,lt}.*.lid >>&"$log"
end
endif

if ($len15000) then
foreach i (12 11 10 9 8 7 6 5 4 3)
   make allclean >&/dev/null
   echo " ===> $i 15000 ${bytes}"
   set log="${dbdir}/${logpref}-$i-15000-${bytes}.log"
   set db="${dbdir}/${prefix}-$i-15000${smoothvalue}-${bytes}.db"
   time $make "MAXBYTES=-L${sample}${bytes}" "MAXBYTES16=-L${sample}${bytes}" "MAXN=$i" "MAXN16=$i" "MAXN24=$i" $smoothing "TOPK=-k15000" "TOPK16=-k15000" "TOPK_CJK=-k15000" >"$log"
   rm -f "$db"
   $mklangid "=${db}" -f $smoothvalue *.lid >>"$log"
   # $mklangid "=${db}" -f $smoothvalue \
   # 		{zh,gan,hak,cmn,nan,wuu,hsn,yue,es,en,ar,arz}.*.lid \
   # 		{hi,bn,pt,ru,ja,de,jv,pn,te,vi,mr,fr,kr,ta,it}.*.lid \
   # 		{ur,tr,gu,pl,mly,bh,bho,awa,uk,ml,kn,mai,su}.*.lid \
   # 		{my,ori,fa,prs,pes,rwr,pa,fil,ha,tl,ro,id,nl}.*.lid \
   # 		{snd,th,pst,uz,raj,hoj,mup,yo,az,azb,azj,ig}.*.lid \
   # 		{am,hne,gaz,gax,hae,asm,hr,sr,ku,ckb,kmr,sdh}.*.lid \
   # 		{ceb,si,rkt,tts,zha,mg,ne,so,khm,mad,bar,el}.*.lid \
   # 		{ctg,bgc,mag,dcc,hu,ful,fub,fuc,ca,sna,zu,syl}.*.lid \
   # 		{quf,quh,que,bjj,cs,lmo,bg,ug,nya,bel,kk,kaz}.*.lid \
   # 		{se,aka,xh,bfy,ht,kok,kin,kik,nap,bal,bcc,ilo}.*.lid \
   # 		{vah,pnb,tk,da,he,sl,sk,fi,no,lt}.*.lid >>&"$log"
end
endif

if ($maxbytes) then
foreach i (3000000 2750000 2500000 2250000 2000000 1750000 1600000 1500000 1400000 1300000 1250000 1200000 1150000 1100000 1050000 1000000 950000 900000 850000 800000 750000 700000 650000 600000 575000 550000 525000 500000 475000 450000 425000 400000 375000 350000 325000 300000 275000 250000 225000 200000 175000 150000 125000 110000 100000 90000 80000 70000 60000 50000 40000 30000 25000 20000 15000 10000)
   make allclean >&/dev/null
   echo " ===> ${ngramlengths} 3+7000 $i"
   set log="${dbdir}/${logpref}-${ngramlengths}-3+7000-$i.log"
   set db="${dbdir}/${prefix}-${ngramlengths}-3+7000${smoothvalue}-$i.db"
   time $make "MAXBYTES=-L${sample}${i}" "MAXBYTES16=-L${sample}${i}" $smoothing $ngramlen >"$log"
   rm -f "$db"
   $mklangid "=${db}" $smoothvalue -f *.lid >>"$log"
   # $mklangid "=${db}" -f $smoothvalue \
   # 		{zh,gan,hak,cmn,nan,wuu,hsn,yue,es,en,ar,arz}.*.lid \
   # 		{hi,bn,pt,ru,ja,de,jv,pn,te,vi,mr,fr,kr,ta,it}.*.lid \
   # 		{ur,tr,gu,pl,mly,bh,bho,awa,uk,ml,kn,mai,su}.*.lid \
   # 		{my,ori,fa,prs,pes,rwr,pa,fil,ha,tl,ro,id,nl}.*.lid \
   # 		{snd,th,pst,uz,raj,hoj,mup,yo,az,azb,azj,ig}.*.lid \
   # 		{am,hne,gaz,gax,hae,asm,hr,sr,ku,ckb,kmr,sdh}.*.lid \
   # 		{ceb,si,rkt,tts,zha,mg,ne,so,khm,mad,bar,el}.*.lid \
   # 		{ctg,bgc,mag,dcc,hu,ful,fub,fuc,ca,sna,zu,syl}.*.lid \
   #		{quf,quh,que,bjj,cs,lmo,bg,ug,nya,bel,kk,kaz}.*.lid \
   # 		{se,aka,xh,bfy,ht,kok,kin,kik,nap,bal,bcc,ilo}.*.lid \
   # 		{vah,pnb,tk,da,he,sl,sk,fi,no,lt}.*.lid >&"$log"
end
endif

if ($filter) then
foreach i (0.40 0.45 0.50 0.52 0.54 0.56 0.58 0.60 0.62 0.64 0.66 0.68 0.70 0.72 0.74 0.76 0.78 0.80 0.82 0.84 0.86 0.88 0.89 0.90 0.91 0.92 0.93 0.94 0.95 0.96 0.97 0.98 0.99 1.00 1.01)
   make allclean >&/dev/null
   echo " ===> ${ngramlengths} 3+7000 $i ${bytes}"
   set log="${dbdir}/${logpref}-${ngramlengths}-3+7000-a$i-defbytes.log"
   set db="${dbdir}/${prefix}-${ngramlengths}-3+7000-a$i${smoothvalue}.db"
   time $make "FILTER=-a$i" $smoothing $ngramlen >"$log"
   rm -f "$db"
   $mklangid "=${db}" -f $smoothvalue *.lid >>"$log"
   # $mklangid "=${db}" -f $smoothvalue \
   # 		{zh,gan,hak,cmn,nan,wuu,hsn,yue,es,en,ar,arz}.*.lid \
   # 		{hi,bn,pt,ru,ja,de,jv,pn,te,vi,mr,fr,kr,ta,it}.*.lid \
   # 		{ur,tr,gu,pl,mly,bh,bho,awa,uk,ml,kn,mai,su}.*.lid \
   # 		{my,ori,fa,prs,pes,rwr,pa,fil,ha,tl,ro,id,nl}.*.lid \
   # 		{snd,th,pst,uz,raj,hoj,mup,yo,az,azb,azj,ig}.*.lid \
   # 		{am,hne,gaz,gax,hae,asm,hr,sr,ku,ckb,kmr,sdh}.*.lid \
   # 		{ceb,si,rkt,tts,zha,mg,ne,so,khm,mad,bar,el}.*.lid \
   # 		{ctg,bgc,mag,dcc,hu,ful,fub,fuc,ca,sna,zu,syl}.*.lid \
   # 		{quf,quh,que,bjj,cs,lmo,bg,ug,nya,bel,kk,kaz}.*.lid \
   # 		{se,aka,xh,bfy,ht,kok,kin,kik,nap,bal,bcc,ilo}.*.lid \
   # 		{vah,pnb,tk,da,he,sl,sk,fi,no,lt}.*.lid >&"$log"
end
endif

exit 0
