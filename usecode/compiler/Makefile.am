AM_CPPFLAGS = -I$(srcdir)/../../headers -I$(srcdir)/.. -I$(srcdir)/../../files -I$(srcdir)/../.. \
	$(INCDIRS) $(DEBUG_FLAGS) $(CPPFLAGS)

AM_YFLAGS = -d -v				# Want ucparse.h.

ucc_SOURCES = ucparse.yy uclex.ll ucmain.cc \
	basic_block.h	\
	ucclass.cc	\
	ucclass.h	\
	ucexpr.cc	\
	ucexpr.h	\
	ucfun.cc	\
	ucfun.h		\
	ucloc.cc	\
	ucloc.h		\
	ucparse.h	\
	ucstmt.cc	\
	ucstmt.h	\
	ucsym.cc	\
	ucsym.h

if BUILD_COMPILER
bin_PROGRAMS = ucc
endif

ucc_LDADD = 			\
	../libusecode.la	\
	$(SYSLIBS)

ucparse.h: ucparse.cc
	test -e ucparse.hh && cp ucparse.hh ucparse.h

uclex.o: uclex.cc ucparse.h

EXTRA_DIST = \
	test1.uc \
	ucdefs.h

CLEANFILES = *~
MAINTAINERCLEANFILES = uclex.cc ucparse.cc ucparse.h ucparse.hh
