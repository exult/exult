# This is an automatically generated file; please do not edit it manually.
# Instead, run makefile_builder.sh from the parent directory.

# Base of the exult source
UCCDIR:=$(top_srcdir)/tools/compiler
UCC:=$(UCCDIR)/ucc

EXPACKDIR:=$(top_srcdir)/tools
EXPACK:=$(EXPACKDIR)/expack

USECODE_OBJECTS :=	\
	usecode.uc

bgdir := $(datadir)/exult/forgeofvirtue/mods

bg_DATA :=	\
	lbjoin.cfg

bgpatchdir := $(bgdir)/lbjoin/patch

nodist_bgpatch_DATA :=	\
	usecode

EXTRA_DIST :=	\
	Makefile.mingw	\
	$(USECODE_OBJECTS)	\
	$(bg_DATA)

CLEANFILES :=	\
	usecode

$(UCC):
	+(cd $(UCCDIR);$(MAKE))


usecode: $(UCC) $(USECODE_OBJECTS)
	$(UCC) -I . -o usecode ./usecode.uc

