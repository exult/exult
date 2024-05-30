top_srcdir := ../..
DOCS_DIR := $(top_srcdir)/docs

HTML_OBJS := $(patsubst %.xml,%.html,$(wildcard *.xml))
TEXT_OBJS := $(patsubst %.xml,%.txt,$(wildcard *.xml))
PHP_OBJS := $(patsubst %.xml,%.dat,$(wildcard *.xml))

DOCS_HTMLS := $(DOCS_DIR)/ReadMe.html $(DOCS_DIR)/faq.html $(DOCS_DIR)/exult_studio.html
DOCS_TEXTS := $(DOCS_DIR)/exult_studio.txt
TOP_TEXTS := $(top_srcdir)/FAQ $(top_srcdir)/README

ifdef WEB_DIR
	WEB_TEXTS := $(patsubst %.xml,$(WEB_DIR)/%.txt,$(wildcard *.xml))
	WEB_DATS := $(patsubst %.xml,$(WEB_DIR)/content/%.dat,$(wildcard *.xml))
else
	WEB_TEXTS :=
	WEB_DATS :=
endif

define build_output_files
	xsltproc -o $(1) $(2) $(3)
	sed -i'' 's/[[:blank:]]*$$//' $(1)
endef

define build_php_dats
	xsltproc -o $(1) $(2) $(3)
	sed -i'' -e '1s@<test xmlns="http://www.w3.org/1999/xhtml">@@; $$s@</test>@@ ; s/[[:blank:]]*$$//' $(1)
endef

# Rules for transforming the XML source
%.html: %.xml html.xsl docs.dtd
	$(call build_output_files, $@, html.xsl, $<)

%.txt: %.xml text.xsl docs.dtd
	$(call build_output_files, $@, text.xsl, $<)

%.dat: %.xml php.xsl docs.dtd
	$(call build_php_dats, $@, php.xsl, $<)

.SUFFIXES: .xml .html .txt .dat

all: html text php $(top_srcdir)/FAQ $(top_srcdir)/README $(WEB_TEXTS) $(WEB_DATS)

define update_files_template
$(1): $(2)
	cp $(2) $(1)
endef

ifdef WEB_DIR
$(foreach file, $(TEXT_OBJS), $(eval $(call update_files_template, $(WEB_DIR)/$(file), $(file))))
$(foreach file, $(PHP_OBJS), $(eval $(call update_files_template, $(WEB_DIR)/content/$(file), $(file))))
endif

# Rule for updating files in the docs directory
$(eval $(call update_files_template, $(DOCS_DIR)/ReadMe.html, docs.html))
$(eval $(call update_files_template, $(DOCS_DIR)/faq.html, faq.html))
$(eval $(call update_files_template, $(DOCS_DIR)/exult_studio.html, exult_studio.html))
$(eval $(call update_files_template, $(DOCS_DIR)/exult_studio.txt, exult_studio.txt))

# Rules for updating files in the top directory
$(eval $(call update_files_template, $(top_srcdir)/FAQ, faq.txt))
$(eval $(call update_files_template, $(top_srcdir)/README, docs.txt))

# remove all generated files
clean:
	-rm -f *.html *.txt *.dat

html: $(HTML_OBJS) $(DOCS_HTMLS) $(DOCS_TEXTS)
text: $(TEXT_OBJS) $(TOP_TEXTS)
php: $(PHP_OBJS)

.PHONY: all html text php clean update
