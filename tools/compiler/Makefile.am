AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir)/usecode -I$(top_srcdir)/files -I$(top_srcdir) \
	$(INCDIRS) $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS)

AM_YFLAGS = -d -v -Wno-yacc				# Want ucparse.h, need Bison extensions.

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
	$(top_builddir)/usecode/libusecode.la	\
	$(SYSLIBS)

ucparse.h: ucparse.cc
	test -e ucparse.hh && cp ucparse.hh ucparse.h

uclex.o: uclex.cc ucparse.h

EXTRA_DIST = \
	test1.uc \
	ucdefs.h

CLEANFILES = *~
MAINTAINERCLEANFILES = uclex.cc ucparse.cc ucparse.h ucparse.hh
