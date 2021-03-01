# This is an automatically generated file; please do not edit it manually.
# Instead, run makefile_builder.sh from the parent directory.

# Base of the exult source
SRC=../..

UCCDIR=$(SRC)/usecode/compiler
UCC=$(UCCDIR)/ucc

EXPACKDIR=$(SRC)/tools
EXPACK=$(EXPACKDIR)/expack

USECODE_OBJECTS = \
	usecode.uc	

bgdir = $(datadir)/exult/forgeofvirtue/mods

bg_DATA = \
	lbjoin.cfg

bgpatchdir = $(bgdir)/lbjoin/patch

bgpatch_DATA = \
	usecode	

EXTRA_DIST = \
	$(bg_DATA) \
	Makefile.mingw \
	$(USECODE_OBJECTS)

CLEANFILES = \
	usecode	

$(UCC):
	+(cd $(UCCDIR);$(MAKE))


usecode: $(UCC) $(USECODE_OBJECTS)
	$(UCC) -I . -o usecode ./usecode.uc

