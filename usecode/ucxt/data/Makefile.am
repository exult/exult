ucxtdir = $(datadir)/exult

if BUILD_TOOLS
if CROSS_COMPILING
head2data = $(HEAD2DATA)
else
head2data = ../head2data
endif
u7bgintrinsics.data u7siintrinsics.data u7sibetaintrinsics.data: $(head2data) ../../bgintrinsics.h ../../siintrinsics.h ../../sibetaintrinsics.h
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
