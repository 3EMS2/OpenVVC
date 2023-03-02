SUBDIRS = libovvc examples
ACLOCAL_AMFLAGS = -Im4

AM_LIBS = @PTHREAD_LIBS@ @LIBS@
AM_CFLAGS = @CFLAGS@ @PTHREAD_CFLAGS@
AM_CC = @PTHREAD_CC@

all: update-build-number all-am

.PHONY: update-build-number
update-build-number:
	$(AM_V_GEN)$(srcdir)/version.sh $(srcdir)/VERSION libovvc/ovversion.h ./
