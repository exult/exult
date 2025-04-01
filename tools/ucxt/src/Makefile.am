AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir)/tools/ucxt/include -I$(top_srcdir)/conf \
	-I$(top_srcdir)/usecode -I$(top_srcdir) $(INCDIRS) $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS) \
	-DEXULT_DATADIR=\"$(EXULT_DATADIR)\"

ucxt_SOURCES = \
	ops.cc \
	ucdata.cc \
	ucxt.cc \
	ucfunc.cc

ucxt_LDADD = \
	$(top_builddir)/conf/libconf.la \
	$(top_builddir)/gamemgr/libgamemgr.la \
	$(top_builddir)/files/libu7file.la \
	$(top_builddir)/files/zip/libminizip.la \
	$(top_builddir)/usecode/libusecode.la \
	$(ZLIB_LIBS) \
	$(SYSLIBS)

if BUILD_TOOLS
bin_PROGRAMS = ucxt
endif
