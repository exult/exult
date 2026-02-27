ucxtdir = $(datadir)/exult

if BUILD_TOOLS
if CROSS_COMPILING
head2data = $(HEAD2DATA_PROG)
else
head2data = $(top_builddir)/tools/ucxt/head2data
endif

ucxtdata_headers: \
	$(top_builddir)/usecode/bgintrinsics.h \
	$(top_builddir)/usecode/siintrinsics.h \
	$(top_builddir)/usecode/sibetaintrinsics.h

u7bgintrinsics.data u7siintrinsics.data u7sibetaintrinsics.data: $(head2data) $(ucxtdata_headers)
	$(head2data) u7bgintrinsics.data u7siintrinsics.data u7sibetaintrinsics.data
endif

ucxt_DATA = \
	u7opcodes.data	\
	u7misc.data \
	u7bgintrinsics.data \
	u7siintrinsics.data \
	u7sibetaintrinsics.data

EXTRA_DIST = \
	events.data	\
	flags.data	\
	u7opcodes.data	\
	u7misc.data
