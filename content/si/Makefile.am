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

sidir = $(datadir)/exult/silverseed/mods

si_DATA = \
	curecantra.cfg

sipatchdir = $(sidir)/curecantra/patch

sipatch_DATA = \
	usecode	

EXTRA_DIST = \
	$(si_DATA) \
	Makefile.mingw \
	$(USECODE_OBJECTS)

CLEANFILES = \
	usecode	

$(UCC):
	+(cd $(UCCDIR);$(MAKE))


usecode: $(UCC) $(USECODE_OBJECTS)
	$(UCC) -I . -o usecode ./usecode.uc

