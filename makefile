## makefile for LA-Strings

INCDIR=./framepac
BE_INCDIR=./bulk_ext
BE_APIDIR=./bulk_ext/src/be13_api
BULK_EXT_SO=

DESTDIR=/usr/bin
DBDIR=/usr/share/langident

OBJS = charset.o extract.o language.o score.o

DISTFILES = COPYING README CHANGELOG makefile manual.txt *.C *.h \
	test/*.txt test/combine.sh test/Copyright test/README \
	framepac/*.h framepac/*.C framepac/makefile framepac/makefile.?? \
	framepac/makefile.??? \
	langident/*.h langident/*.C langident/makefile langident/*.txt \
	util/test-random.sh util/icuconv.C util/icutrans.C util/interleave.c \
	util/dehtmlize.C util/score.C util/mktestset.sh util/eval.sh \
	util/prepare_file.sh util/counts.sh util/add-random.sh util/make*.sh \
	util/sort-err.sh util/makefile \
	Crubadan/MANIFEST Crubadan/README \
	Crubadan/High/* Crubadan/Med/* Crubadan/Low/* Crubadan/Rare/* \
	models/makefile models/*.lid models/MANIFEST

LIBRARY = la-strings.a

#########################################################################
## define the compiler

CC=g++
CCLINK=$(CC)

#########################################################################
## define compilation options

ifdef USE_BULK_EXT
BULK_EXT=-fPIC -DBULK_EXTRACTOR -I$(BE_INCDIR) -I$(BE_INCDIR)/src -I$(BE_APIDIR)
BULK_EXT_SO=scan_strings.so
THREADS=1
MAKE_SHAREDLIB=1
endif

ifdef THREADS
MULTITHREAD=-DFrMULTITHREAD -DSHARED -pthread
endif

ifndef RELEASE
RELPATH=LAStrings
ZIPNAME=lastrings.zip
else
RELPATH=LAStrings-$(RELEASE)
ZIPNAME=lastrings-$(RELEASE).zip
endif

ifeq ($(NOICONV),1)
ICONV=-DNO_ICONV
else
ICONV=
endif

ifndef $(BITS)
#BITS=32
BITS=64
endif

ifeq ($(DO_PROFILE),1)
PROFILE=-pg
else
ifndef $(PROFILE)
PROFILE=
endif
endif

ifeq ($(DEBUG),1)
CFLAGS=-O0 -Wall -Wextra -ggdb3 -m$(BITS) $(PROFILE) $(ICONV) $(MULTITHREAD)
LINKFLAGS=-ggdb3 $(MULTITHREAD)
else
ifeq ($(DEBUG),-1)
CFLAGS=-O9 -fexpensive-optimizations -Wall -Wextra -ggdb3 -m$(BITS) $(PROFILE) $(ICONV) $(MULTITHREAD)
LINKFLAGS=-ggdb3 $(MULTITHREAD)
else
CFLAGS=-O9 -fexpensive-optimizations -Wall -Wextra -m$(BITS) $(PROFILE) $(ICONV) $(MULTITHREAD)
LINKFLAGS=$(MULTITHREAD)
endif
endif
CFLAGEXE=-o $@ -m$(BITS) $(PROFILE)

CPUTYPE = -D__586__ -D__886__

#########################################################################
## define the programs needed to create the target library

LIBRARIAN=ar
LIBFLAGS=rucl
LIBINDEXER = ranlib
LIBIDXFLAGS = $(LIBRARY)

RM=rm -f
CP?=/bin/cp

#########################################################################
## standard targets

.PHONY: default
default: la-strings $(BULK_EXT_SO) langident/mklangid languages.db charsets.db
	@echo "top100.db and crubadan.db are not built by default -- use 'make all'"

.PHONY: help
help:
	@echo "Standard build targets:"
	@echo "  default	program files plus default language database"
	@echo "  all		program files plus all language databases"
	@echo "  install	copy program to DESTDIR and databases to DBDIR"
	@echo "  zip		distribution archive"
	@echo "  tags		run etags over source"
	@echo "  clean		clean up build files in top-level directory"
	@echo "  allclean	clean up build files in all directories"
	@echo ""
	@echo "Language database build targets:"
	@echo "  languages.db	  default database, built by default"
	@echo "  top100		  100 languages (also built by 'make all')"
	@echo "  noutf16	  all/top100 languages, but omit UTF16BE and UTF16LE"

.PHONY: all clean allclean tags zip lib install top100 noutf16 top100-noutf16

all:  default crubadan.db top100.db top100-charsets.db

top100: default top100.db top100-charsets.db

noutf16: default top100-noutf16 lang-noutf16.db noutf16-charsets.db

top100-noutf16: top100-noutf16.db top100-noutf16-charsets.db

clean:
	-$(RM) *.o scan_*.so la-strings

allclean: clean
	-( cd framepac ; $(MAKE) clean )
	-( cd langident ; $(MAKE) clean )

tags:
	etags --c++ *.h *.C

install: la-strings languages.db top100.db
	-mkdir -p $(DBDIR)
	$(CP) -p languages.db $(DBDIR)
	$(CP) -p top100.db $(DBDIR)
	-$(CP) -p crubadan.db $(DBDIR)
	$(CP) -p la-strings $(DESTDIR)

zip:	la-strings langident/mklangid langident/whatlang
	-strip la-strings langident/mklangid langident/whatlang
	-$(RM) $(ZIPNAME)
	mkdir $(RELPATH)
	-cp -ip --parents $(DISTFILES) $(RELPATH)
	-( cd $(RELPATH) ; md5sum ${DISTFILES} >MD5SUM )
	-( cd $(RELPATH) ; sha1sum ${DISTFILES} >SHA1SUM )
	zip -mro9q $(ZIPNAME) $(RELPATH)/

lib:	$(LIBRARY)

#########################################################################
## executables

la-strings: la-strings.o $(LIBRARY) langident/langident.a framepac/framepac.a
	$(CCLINK) $(LINKFLAGS) $(CFLAGEXE) la-strings.o $(LIBRARY) \
		langident/langident.a framepac/framepac.a

langident/mklangid:
	( cd langident ; $(MAKE) MAKE_SHAREDLIB=$(MAKE_SHAREDLIB) THREADS=$(THREADS) all )

whatlang: langident

#########################################################################
## data files

languages.db: models/MANIFEST langident/mklangid
	-$(RM) $@
	@echo "*** NOTE: Building the language database requires about 1 GB of RAM for MkLangID ***"
	-langident/mklangid =$@ -v -f ./models/*.lid

charsets.db: languages.db
	-$(RM) $@
	@echo "*** NOTE: Building the charset database requires about 1.5 GB of RAM ***"
	-langident/mklangid ==$^ -v -C 0.0,$@

top100.db: models/top100/MANIFEST langident/mklangid
	-$(RM) $@
	-langident/mklangid =$@ -v -f ./models/top100/*.lid

top100-charsets.db: top100.db
	-$(RM) $@
	-langident/mklangid ==$^ -v -C 0.0,$@

lang-noutf16.db: models/noutf16/MANIFEST langident/mklangid
	-$(RM) $@
	@echo "*** NOTE: Building the language database requires about 1 GB of RAM for MkLangID ***"
	-langident/mklangid =$@ -v -f ./models/noutf16/*.lid

noutf16-charsets.db: lang-noutf16.db
	-$(RM) $@
	-langident/mklangid ==$^ -v -C 0.0,$@

top100-noutf16.db: models/top100noutf16/MANIFEST langident/mklangid
	-$(RM) $@
	@echo "*** NOTE: Building the language database requires about 1 GB of RAM for MkLangID ***"
	-langident/mklangid =$@ -v -f ./models/top100noutf16/*.lid

top100-noutf16-charsets.db: top100-noutf16.db
	-$(RM) $@
	-langident/mklangid ==$^ -v -C 0.0,$@

crubadan.db: Crubadan/MANIFEST langident/mklangid
	-$(RM) $@
	@echo "*** NOTE: Building the language database requires about 1 GB of RAM for MkLangID ***"
	-langident/mklangid =$@ -v -f ./models/*.lid \
		-fc -d 1.15 -fc ./Crubadan/High/*.3gm \
		-d 1.25 -fc ./Crubadan/Med/*.3gm \
		-d 1.45 -fc ./Crubadan/Low/*.3gm \
		-d 1.75 -fc ./Crubadan/Rare/*.3gm

crubadan-charsets.db: crubadan.db
	-$(RM) $@
	-langident/mklangid ==$^ -v -C 0.0,$@

# create any derived files from the language models in the distribution
models/MANIFEST:
	( cd models ; $(MAKE) MANIFEST )

# extract just the non-UTF16 models
models/noutf16/MANIFEST: models/MANIFEST
	-mkdir -p models/noutf16
	-$(RM) -f models/noutf16/*.lid
	(cd models/noutf16 ; ln -s `ls -1 ../*.lid | fgrep -v -e utf16 -e ASCII-16` .)
	(cd models/noutf16 ; ls *.lid >MANIFEST)

# extract just the top-100 models
models/top100/MANIFEST: models/MANIFEST
	-mkdir -p models/top100
	-$(RM) -f models/top100/*.lid
	(cd models/top100 ; ln -s ../{gan,hak,cmn,nan,wuu,hsn,yue,es,en,ar,hi,bn,pt,ru,ja,de,jv,pn,te,vi,mr,fr,kr,ta,it,ur,tr,gu,pl,mly,bh,bho,awa,uk,ml,kn,mai,su,my,ori,fa,prs,pes,rwr,pa,fil,ha,tl,ro,id,nl,snd,th,pst,uz,raj,hoj,mup,yo,az,azb,azj,ig,am,hne,gaz,gax,hae,asm,hr,sr,ku,ckb,kmr,sdh,ceb,si,rkt,tts,zha,mg,ne,so,khm,mad,bar,el,ctg,bgc,mag,dcc,hu,ful,fub,fuc,ca,sna,zu,syl,quf,quh,que,bjj,cs,lmo,bg,ug,nya,bel,kk,kaz,se,aka,xh,bfy,ht,kok,kin,kik,nap,bal,bcc,ilo,vah,pnb,tk,da,he,sl,sk,fi,no,lt,NUM}.*.lid .)
	(cd models/top100 ; rm -f *.\*.lid)
	(cd models/top100 ; ls *.lid >MANIFEST)

# extract just the non-UTF16 top-100 models
models/top100noutf16/MANIFEST: models/top100/MANIFEST
	-mkdir -p models/top100noutf16
	-$(RM) -f models/top100noutf16/*.lid
	(cd models/top100noutf16 ; ln -s `ls -1 ../top100/*.lid | fgrep -v -e utf16 -e ASCII-16` .)
	(cd models/top100noutf16 ; ls *.lid >MANIFEST)

#########################################################################
## object modules

la-strings.o: la-strings.C charset.h extract.h la-strings.h langident/langid.h

charset.o: charset.C charset.h language.h langident/roman.h

extract.o: extract.C extract.h charset.h score.h langident/trie.h langident/langid.h

language.o: language.C language.h charset.h

scan_strings.o: scan_strings.C charset.h extract.h la-strings.h langident/langid.h

scan_strings.so: scan_strings.o $(LIBRARY) langident/langident.a framepac/framepac.a
	$(CC) -shared -Wl,-soname,$@ -o $@ $^

score.o: score.C score.h charset.h

#########################################################################
## header files -- touching to ensure proper recompilation

charset.h:	language.h langident/trie.h
	touch $@

extract.h:	language.h
	touch $@

wordhash.h: config.h
	touch $@

#########################################################################
## libraries


$(LIBRARY): $(OBJS)
	-$(RM) $(LIBRARY)
	$(LIBRARIAN) $(LIBFLAGS) $(LIBRARY) $(OBJS)
	$(LIBINDEXER) $(LIBIDXFLAGS)

framepac/framepac.a: nonexistentfile
	( cd framepac ; $(MAKE) MAKE_SHAREDLIB=$(MAKE_SHAREDLIB) THREADS=$(THREADS) lib )

langident/langident.a:  nonexistentfile
	( cd langident ; $(MAKE) MAKE_SHAREDLIB=$(MAKE_SHAREDLIB) THREADS=$(THREADS) lib )

nonexistentfile:

#########################################################################
## default compilation rule

.C.o: ; $(CC) $(CFLAGS) $(CPUTYPE) -I$(INCDIR) $(BULK_EXT) -c $<
