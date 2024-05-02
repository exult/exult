HTML_OBJS := $(patsubst %.xml,%.html,$(wildcard *.xml))
TEXT_OBJS := $(patsubst %.xml,%.txt,$(wildcard *.xml))
PHP_OBJS := $(patsubst %.xml,%.dat,$(wildcard *.xml))

all: html text php ReadMe.html FAQ README

define build_output_files
	xsltproc -o $(1) $(2) $(3)
endef

define build_php_dats
	xsltproc -o $(1) $(2) $(3)
	sed -i'' -e '1s@<test xmlns="http://www.w3.org/1999/xhtml">@@; $$s@</test>@@' $(1)
endef

# Rules for transforming the XML source
%.html: %.xml html.xsl docs.dtd
	$(call build_output_files, $@, html.xsl, $<)

%.txt: %.xml text.xsl docs.dtd
	$(call build_output_files, $@, text.xsl, $<)

%.dat: %.xml php.xsl docs.dtd
	$(call build_php_dats, $@, php.xsl, $<)

.SUFFIXES: .xml .html .txt .dat

# rule for renaming docs.html to ReadMe.html
ReadMe.html: docs.html
	cp docs.html ReadMe.html

FAQ: faq.txt
	cp faq.txt FAQ

README: docs.txt
	cp docs.txt README

# remove all generated files
clean:
	-rm -f *.html *.txt *.dat FAQ README

html: $(HTML_OBJS)
text: $(TEXT_OBJS)
php: $(PHP_OBJS)

.PHONY: all html text php clean
