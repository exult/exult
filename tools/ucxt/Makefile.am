AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir)/tools/ucxt/include -I$(top_srcdir)/conf -I$(top_srcdir) \
        -I$(top_srcdir)/usecode $(INCDIRS) $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) \
        $(CPPFLAGS) -DEXULT_DATADIR=\"$(EXULT_DATADIR)\"

if BUILD_TOOLS
SUBDIRS = . src include data Docs

noinst_PROGRAMS = head2data
endif

head2data_SOURCES = head2data.cc
