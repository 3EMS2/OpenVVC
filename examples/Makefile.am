bin_PROGRAMS = dectest ovrate ovrmx

dectest_SOURCES = dectest.c
dectest_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/../libovvc -I ../libovvc
dectest_LDADD = ../libovvc/libovvc.la @SLHDR_LIBS@

ovrate_SOURCES = ovrate.c
ovrate_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/../libovvc -I ../libovvc
ovrate_LDADD = ../libovvc/libovvc.la @SLHDR_LIBS@

ovrmx_SOURCES = ovremuxer.c
ovrmx_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/../libovvc -I ../libovvc
ovrmx_LDADD = ../libovvc/libovvc.la @SLHDR_LIBS@
