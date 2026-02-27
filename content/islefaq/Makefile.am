# This is an automatically generated file; please do not edit it manually.
# Instead, run makefile_builder.sh from the parent directory.

# Base of the exult source
UCCDIR:=$(top_srcdir)/tools/compiler
UCC:=$(UCCDIR)/ucc

EXPACKDIR:=$(top_srcdir)/tools
EXPACK:=$(EXPACKDIR)/expack

USECODE_OBJECTS :=	\
	src/usecode.uc

FACES_VGA_OBJECTS :=	\
	src/graphics/faces.in	\
	src/graphics/faces/colourless.shp	\
	src/graphics/faces/dominus.shp	\
	src/graphics/faces/drcode.shp	\
	src/graphics/faces/fingolfin.shp	\
	src/graphics/faces/wjp.shp

SHAPES_VGA_OBJECTS :=	\
	src/graphics/shapes.in	\
	src/graphics/shapes/penguin.shp

islefaqdir := $(datadir)/exult/forgeofvirtue/mods

islefaq_DATA :=	\
	islefaq.cfg

islefaqpatchdir =	$(islefaqdir)/islefaq/patch

nodist_islefaqpatch_DATA :=	\
	patch/usecode	\
	patch/faces.vga	\
	patch/shapes.vga

islefaqpatch_DATA :=	\
	patch/equip.dat	\
	patch/initgame.dat	\
	patch/minimaps.vga	\
	patch/occlude.dat	\
	patch/ready.dat	\
	patch/shpdims.dat	\
	patch/textmsg.txt	\
	patch/tfa.dat	\
	patch/u7ifix0d	\
	patch/u7ifix64	\
	patch/u7ifix65	\
	patch/u7map	\
	patch/wgtvol.dat	\
	patch/wihh.dat

EXTRA_DIST :=	\
	Makefile.mingw	\
	$(USECODE_OBJECTS)	\
	$(FACES_VGA_OBJECTS)	\
	$(SHAPES_VGA_OBJECTS)	\
	$(islefaq_DATA)	\
	$(islefaqpatch_DATA)

CLEANFILES :=	\
	patch/usecode	\
	patch/faces.vga	\
	patch/faces_vga.h	\
	patch/shapes.vga	\
	patch/shapes_vga.h

$(EXPACK):
	+(cd $(EXPACKDIR);$(MAKE) expack)

$(UCC):
	+(cd $(UCCDIR);$(MAKE))


patch/usecode: $(UCC) $(USECODE_OBJECTS)
	$(UCC) -I src -o patch/usecode src/usecode.uc

patch/faces.vga: $(EXPACK) $(FACES_VGA_OBJECTS)
	$(EXPACK) -i $(srcdir)/src/graphics/faces.in

patch/shapes.vga: $(EXPACK) $(SHAPES_VGA_OBJECTS)
	$(EXPACK) -i $(srcdir)/src/graphics/shapes.in

