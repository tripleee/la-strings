CPUDEF=-march=athlon64 -m64 -D__886__

all: interleave icuconv icutrans dedup dehtmlize utf8hist

dedup: dedup.C
	gcc -O9 ${CPUDEF} -o $@ -Wall $<

dehtmlize: dehtmlize.C
	gcc -O9 ${CPUDEF} -o $@ -Wall $<

interleave: interleave.c
	gcc -O9 $(CPUDEF) -o $@ -Wall $<

icuconv: icuconv.C
	g++ -o $@ -Wall $< -licuuc

icutrans: icutrans.C
	g++ -o $@ -Wall $< -licuuc -licutu -licui18n

score: score.C ../framepac/framepac.a
	g++ -ggdb3 -o $@ -I../framepac -Wall $< ../framepac/framepac.a

utf8hist: utf8hist.C
	g++ -o $@ -Wall $<
