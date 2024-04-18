HOSTARCH := $(shell uname -m | \
	sed -e s/i.86/i386/ \
	    -e s/sun4u/sparc64/ \
	    -e s/arm.*/arm/ \
	    -e s/sa110/arm/ \
	    -e s/macppc/ppc/)

HOSTOS := $(shell uname -s | tr A-Z a-z)

CROSS_COMPILE =
export	CROSS_COMPILE HOSTARCH

TOPDIR	:= $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)
export	TOPDIR
SRCDIR	:= $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)
export	SRCDIR

REVISION = $(shell cat ./include/version.h | grep VERSION_STRING | awk '{print $$5}' | sed -e s/version-//g | sed -e s/\"//g)
BUILDATE = $(shell cat ./include/version.h | grep DLAV_BUILD_DATE | awk '{print $$3}' | sed -e s/0x//g | sed -e s/\r//g)

REV = "dlav-$(REVISION)-$(BUILDATE)"

include $(TOPDIR)/Makefile.inc

EXE = dlavs

SUBDIRS	= libcom \
		  libdla \
		  app

SRCDIRS	= bin \
	include \
	include/ftd2xx \
	include/libcom \
	lib \
	lib/libftd2xx \
	libcom \
	libdla

CLEANDIR = $(SUBDIRS)


#########################################################################
# objects....order is important (i.e. start must be first)

OBJS  =	libcom/libcommon.a libdla/libdla.a 

all: $(EXE)

install: all

$(EXE):
	@for dir in $(SUBDIRS) ; do $(MAKE) -C $$dir || exit 1 ; done

subdirs:
	@for dir in $(SUBDIRS) ; do $(MAKE) -C $$dir || exit 1 ; done

depend dep:
	@for dir in $(SUBDIRS) ; do $(MAKE) -C $$dir .depend ; done

touch:
	@for dir in $(SRCDIRS) ; do touch $$dir/* ; done

clean:
	rm -f `find . -type f \
		\( -name 'core' -o -name '*.bak' -o -name '*~' \
		-o -name '*.o'  -o -name '*.a' -o -name '.depend' \) -print`
	@for d in $(CLEANDIR); do $(MAKE) -C $$d clean; done
	cp ./lib/libftd2xx/libftd2xx.a.org ./lib/libftd2xx/libftd2xx.a
