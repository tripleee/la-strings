## makefile for LangIdent

INCDIR=../framepac
DESTDIR=/usr/bin
DBDIR=/usr/share/langident

SHAREDLIB=

OBJS = langid.o scan_langid.o mtrie.o pstrie.o ptrie.o roman.o smooth.o \
	trie.o trigram.o wildcard.o

DISTFILES = COPYING README makefile manual.txt *.C *.h \
	mklangid romanize whatlang

LIBRARY=langident.a

#########################################################################
## define the compiler

CC=g++
CCLINK=$(CC)

#########################################################################
## define compilation options

ifdef MAKE_SHAREDLIB
SHAREDLIB=-fPIC
endif

ifdef THREADS
MULTITHREAD=-DFrMULTITHREAD -pthread
endif

ifndef RELEASE
RELPATH=LangIdent
ZIPNAME=langident.zip
else
RELPATH=LangIdent-$(RELEASE)
ZIPNAME=langident-$(RELEASE).zip
endif

ifeq ($(NOICONV),1)
ICONV=-DNO_ICONV
else
ICONV=
endif

ifndef BITS
#BITS=32
BITS=64
endif

ifeq ($(DO_PROFILE),1)
PROFILE=-pg
else
ifndef PROFILE
PROFILE=
endif
endif

ifeq ($(DEBUG),1)
CFLAGS=-O0 -Wall -Wextra -ggdb3 -m$(BITS) $(PROFILE) $(ICONV) '-DDBDIR="$(DBDIR)"' $(MULTITHREAD)
CFLAGSLOOP=$(CFLAGS)
LINKFLAGS=-L$(INCDIR) -ggdb3 $(MULTITHREAD)
else
ifeq ($(DEBUG),-1)
CFLAGS=-O3 -fexpensive-optimizations -Wall -Wextra -ggdb3 -m$(BITS) $(PROFILE) $(ICONV) '-DDBDIR="$(DBDIR)"' $(MULTITHREAD)
CFLAGSLOOP=-fno-align-labels -fno-align-loops $(CFLAGS)
LINKFLAGS=-L$(INCDIR) -ggdb3 $(MULTITHREAD)
else
CFLAGS=-O3 -fexpensive-optimizations -fmerge-constants -Wall -Wextra -m$(BITS) $(PROFILE) $(ICONV) '-DDBDIR="$(DBDIR)"' $(MULTITHREAD)
CFLAGSLOOP=-fno-align-labels -fno-align-loops $(CFLAGS)
LINKFLAGS=-L$(INCDIR) $(MULTITHREAD)
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

#########################################################################
## standard targets

all:  mklangid romanize whatlang subsample

clean:
	-$(RM) $(LIBRARY) *.o mklangid whatlang romanize subsample

allclean:
	-$(RM) $(LIBRARY) *.o mklangid whatlang romanize subsample

tags:
	etags --c++ *.h *.C

install:
	$(CP) -p mklangid romanize whatlang $(DESTDIR)

zip: 	mklangid romanize whatlang
	-strip mklangid romanize whatlang
	-$(RM) $(ZIPNAME)
	mkdir $(RELPATH)
	-$(CP) -ip --parents $(DISTFILES) $(RELPATH)
	-( cd $(RELPATH) ; md5sum ${DISTFILES} >MD5SUM )
	-( cd $(RELPATH) ; sha1sum ${DISTFILES} >SHA1SUM )
	zip -mro9 $(ZIPNAME) $(RELPATH)/

lib:	$(LIBRARY)

#########################################################################
## executables

mklangid: mklangid.o $(LIBRARY) $(INCDIR)/framepac.a
	$(CCLINK) $(LINKFLAGS) $(CFLAGEXE) mklangid.o $(LIBRARY) $(INCDIR)/framepac.a

romanize: romanize.o $(LIBRARY) $(INCDIR)/framepac.a
	$(CCLINK) $(LINKFLAGS) $(CFLAGEXE) romanize.o $(LIBRARY) $(INCDIR)/framepac.a

whatlang: whatlang.o $(LIBRARY) $(INCDIR)/framepac.a
	$(CCLINK) $(LINKFLAGS) $(CFLAGEXE) whatlang.o $(LIBRARY) $(INCDIR)/framepac.a

subsample: subsample.o
	$(CCLINK) $(LINKFLAGS) $(CFLAGEXE) subsample.o

#########################################################################
## object modules

langid.o: langid.C langid.h
	$(CC) $(CFLAGSLOOP) $(CPUTYPE) -I$(INCDIR) $(SHAREDLIB) $(MULTITHREAD) -c $<

mklangid.o: mklangid.C langid.h trie.h mtrie.h

whatlang.o: whatlang.C langid.h

scan_langid.o: scan_langid.C langid.h

mtrie.o: mtrie.C mtrie.h

pstrie.o: pstrie.C pstrie.h mtrie.h wildcard.h

ptrie.o: ptrie.C ptrie.h mtrie.h

roman.o: roman.C roman.h

smooth.o: smooth.C langid.h

subsample.o: subsample.C

trie.o: trie.C trie.h wildcard.h

trigram.o: trigram.C langid.h trie.h

wildcard.o: wildcard.C wildcard.h

#########################################################################
## header files -- touching to ensure proper recompilation

langid.h:	mtrie.h
	touch langid.h

pstrie.h:	ptrie.h trie.h
	touch pstrie.h

#########################################################################
## library

$(LIBRARY): $(OBJS)
	-$(RM) $(LIBRARY)
	$(LIBRARIAN) $(LIBFLAGS) $(LIBRARY) $(OBJS)
	$(LIBINDEXER) $(LIBIDXFLAGS)

$(INCDIR)/framepac.a:
	( cd $(INCDIR) ; make lib )

#########################################################################
## default compilation rule

.C.o: ; $(CC) $(CFLAGS) $(CPUTYPE) -I$(INCDIR) $(SHAREDLIB) -c $<
