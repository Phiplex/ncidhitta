PROG        = ncidd
SRC         = $(PROG).c nciddconf.c nciddalias.c nciddhangup.c poll.c nciddhitta.c
DIST        = $(PROG).conf-in
HEADER      = $(PROG).h nciddconf.h nciddalias.h nciddhangup.h poll.h
ETCFILE     = ncidd.conf ncidd.alias ncidd.blacklist ncidd.whitelist
SOURCE      = $(SRC) $(DIST) $(HEADER)
FILES       = README.server Makefile $(SOURCE) $(ETCFILE)

VERSION := $(shell sed 's/.* //; 1q' ../VERSION)
API := $(shell sed '1d; 2q' ../VERSION)

# the prefix must end in a - (if part of a name) or a / (if part of a path)
MIPSXCOMPILE = mips-TiVo-linux-
PPCXCOMPILE  = /usr/local/tivo/bin/

# prefix and prefix2 are used on a make, install, and making a package
# prefix3 is used on install to make a package
prefix       = /usr/local
prefix2      = $(prefix)
prefix3      =

settag       = NONE
setname      = NONE

BASH         = /bin/bash

BIN          = $(prefix)/bin
SBIN         = $(prefix)/sbin
SHARE        = $(prefix)/share
ETC          = $(prefix2)/etc
DEV          = $(prefix3)/dev
VAR          = $(prefix3)/var

NCIDDIR      = $(SHARE)/ncid
MODDIR       = $(NCIDDIR)/modules
SYSDIR       = $(NCIDDIR)/sys
ANNDIR       = $(NCIDDIR)/recordings

CONFDIR      = $(ETC)/ncid
CONF         = $(CONFDIR)/$(PROG).conf
ALIAS        = $(CONFDIR)/ncidd.alias
BLACKLIST    = $(CONFDIR)/ncidd.blacklist
WHITELIST    = $(CONFDIR)/ncidd.whitelist
RECORDING    = $(ANNDIR)/NumberDisconnected.rmd

TTYPORT      = $(DEV)/modem

LOGDIR       = $(VAR)/log
LOGFILE      = $(LOGDIR)/$(PROG).log
CIDLOG       = $(LOGDIR)/cidcall.log
DATALOG      = $(LOGDIR)/ciddata.log

RUNDIR       = $(VAR)/run
PIDFILE      = $(RUNDIR)/$(PROG).pid

LOCKDIR      = $(VAR)/lock/lockdev
LOCKFILE     = $(LOCKDIR)/LCK..

NCIDUPDATE   = $(BIN)/cidupdate
NCIDUTIL     = $(BIN)/ncidutil

SITE         = $(DIST:-in=)

OBJECT       = $(SRC:.c=.o)

DEFINES      = -DCIDCONF=\"$(CONF)\" \
               -DCIDALIAS=\"$(ALIAS)\" \
               -DBLACKLIST=\"$(BLACKLIST)\" \
               -DWHITELIST=\"$(WHITELIST)\" \
               -DRECORDING=\"$(RECORDING)\" \
               -DCIDLOG=\"$(CIDLOG)\" \
               -DTTYPORT=\"$(TTYPORT)\" \
               -DDATALOG=\"$(DATALOG)\" \
               -DLOGFILE=\"$(LOGFILE)\" \
               -DPIDFILE=\"$(PIDFILE)\" \
               -DLOCKFILE=\"$(LOCKFILE)\" \
               -DNCIDUPDATE=\"$(NCIDUPDATE)\" \
               -DNCIDUTIL=\"$(NCIDUTIL)\"

# local additions to CFLAGS
MFLAGS       = -Wmissing-declarations -Wunused-variable -Wparentheses \
               -Wreturn-type -Wpointer-sign -Wformat #-Wunused-but-set-variable

CFLAGS       = -O2 -I. -I.. -I/usr/include/libxml2 -lxml2 -lcurl $(DEFINES) $(MFLAGS) $(EXTRA_CFLAGS)

STRIP        = -s
LDFLAGS      = $(STRIP)
LDLIBS       = 

usage:
	@echo "to build a TiVo ppc binary for /var/hack: make tivo-s1"
	@echo "to build a TiVo mips binary for /var/hack: make tivo-s2"
	@echo "to build a TiVo mips binary for /usr/local: make tivo-mips"
	@echo "to build a Win/cygwin binary: make cygwin"
	@echo "to build a Linux, BSD, or Mac binary: make local"
	@echo "to install in /usr/local: make install"

tivo-s1:
	$(MAKE) tivo-ppc prefix=/var/hack

tivo-ppc:
	$(MAKE) server \
            CC=$(PPCXCOMPILE)gcc \
            MFLAGS="-DTIVO_S1 -D__need_timeval" \
            LD=$(PPCXCOMPILE)ld \
            RANLIB=$(PPCXCOMPILE)ranlib \
            TTYPORT=/dev/ttyS1 \
            LOCKFILE=/var/tmp/modemlock \
            setname="TiVo requires CLOCAL"
	touch tivo-ppc

tivo-s2:
	$(MAKE) tivo-mips prefix=/var/hack \

tivo-mips:
	$(MAKE) server \
            CC=$(MIPSXCOMPILE)gcc \
            MFLAGS="-std=gnu99" \
            LD=$(MIPSXCOMPILE)ld \
            RANLIB=$(MIPSXCOMPILE)ranlib \
            TTYPORT=/dev/ttyS3 \
            LOCKFILE=/var/tmp/modemlock \
            setname="TiVo requires CLOCAL"
	touch tivo-mips

cygwin:
	$(MAKE) server prefix=/usr prefix2= \
            CFLAGS="$(CFLAGS) -IC:/WpdPack/Include" \
            LDLIBS="$(LDLIBS) -LC:/WpdPack/Lib -lwpcap"

local:
	$(MAKE) server

server: $(PROG) site

site: $(SITE)

$(PROG): $(SRC) $(HEADER) ../version.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

../version.h: ../version.h-in
	sed "s/XXX/$(VERSION)/; s/api/$(API)/" $< > $@

install: $(PROG) dirs install-etc
	install -m 755 $(PROG) $(SBIN)

install-etc: site
	@if test -f $(CONF); \
     then install -m 644 $(PROG).conf $(CONF).new; \
     else install -m 644 $(PROG).conf $(CONFDIR); \
     fi
	@if test -f $(ALIAS); \
     then install -m 644 $(PROG).alias $(ALIAS).new; \
     else install -m 644 $(PROG).alias $(CONFDIR); \
     fi
	@if test -f $(BLACKLIST); \
     then install -m 644 $(PROG).blacklist $(BLACKLIST).new; \
     else install -m 644 $(PROG).blacklist $(CONFDIR); \
     fi
	@if test -f $(WHITELIST); \
     then install -m 644 $(PROG).whitelist $(WHITELIST).new; \
     else install -m 644 $(PROG).whitelist $(CONFDIR); \
     fi

dirs:
	@if ! test -d $(SBIN); then mkdir -p $(SBIN); fi
	@if ! test -d $(CONFDIR); then mkdir -p $(CONFDIR); fi
	@if ! test -d $(MODDIR); then mkdir -p $(MODDIR); fi

clean:
	rm -f *.o *.a

clobber: clean
	rm -f $(PROG) $(PROG).ppc-tivo $(PROG).mips-tivo tivo-ppc tivo-mips
	rm -f $(PROG).ppc-mac $(PROG).i386-mac
	rm -f $(SITE)
	rm -f a.out *.log *.zip *.tar.gz *.tgz

distclean: clobber

files:	$(FILES)

.SUFFIXES: -in

-in : *-in
	sed 's,/usr/local/share,$(prefix)/share,;s,/usr/local/etc,$(prefix2)/etc,;/$(settag)/s/# set/set/;/$(setname)/s/# set/set/' $< > $@
