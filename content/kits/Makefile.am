# Usecode kits for mod authors: installed as source to include with
# ucc -I $(datadir)/exult/kits/shop.  The two example shopkeepers are
# compiled with the freshly built ucc on every build, which checks that
# the kit compiles for both games.  See docs/shop_kit.md.
# Base of the exult source
UCCDIR:=$(top_builddir)/tools/compiler
UCC:=$(UCCDIR)/ucc

kitdir := $(datadir)/exult/kits/shop
kit_DATA :=	\
	shop/shop_kit.uc	\
	shop/example_bg.uc	\
	shop/example_si.uc

EXTRA_DIST :=	\
	$(kit_DATA)

CLEANFILES :=	\
	example_bg.usecode	\
	example_si.usecode

$(UCC):
	+(cd $(UCCDIR);$(MAKE))

example_bg.usecode: $(UCC) $(srcdir)/shop/shop_kit.uc $(srcdir)/shop/example_bg.uc
	$(UCC) -I $(srcdir)/shop -o $@ $(srcdir)/shop/example_bg.uc

example_si.usecode: $(UCC) $(srcdir)/shop/shop_kit.uc $(srcdir)/shop/example_si.uc
	$(UCC) -I $(srcdir)/shop -o $@ $(srcdir)/shop/example_si.uc

all-local: example_bg.usecode example_si.usecode
