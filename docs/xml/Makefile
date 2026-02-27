top_srcdir := ../..
DOCS_DIR := $(top_srcdir)/docs
WEB_TMP := ./web_tmp

HTML_OBJS := $(patsubst %.xml,%.html,$(wildcard *.xml))
TEXT_OBJS := $(patsubst %.xml,%.txt,$(wildcard *.xml))

DOCS_HTMLS := $(DOCS_DIR)/ReadMe.html $(DOCS_DIR)/faq.html $(DOCS_DIR)/exult_studio.html
DOCS_TEXTS := $(DOCS_DIR)/exult_studio.txt
TOP_TEXTS := $(top_srcdir)/FAQ $(top_srcdir)/README
WEB_TMP_HTMLS := $(WEB_TMP)/docs.html $(WEB_TMP)/faq.html $(WEB_TMP)/studio.html

ifdef WEB_DIR
	WEB_TEXTS := $(patsubst %.xml,$(WEB_DIR)/%.txt,$(wildcard *.xml))
	WEB_HTMLS := $(WEB_DIR)/docs.html $(WEB_DIR)/faq.html $(WEB_DIR)/studio.html
else
	WEB_TEXTS :=
	WEB_HTMLS :=
endif

define build_output_files
	xsltproc -o $(1) $(2) $(3)
	sed -i'' 's/[[:blank:]]*$$//' $(1)
endef

# Rules for transforming the XML source
%.html: %.xml html.xsl docs.dtd
	$(call build_output_files, $@, html.xsl, $<)

%.txt: %.xml text.xsl docs.dtd
	$(call build_output_files, $@, text.xsl, $<)

all: html text web-tmp $(top_srcdir)/FAQ $(top_srcdir)/README $(WEB_TMP_HTMLS) $(WEB_TEXTS) $(WEB_HTMLS)

# Create web_tmp directory if it doesn't exist
$(WEB_TMP):
	mkdir -p $(WEB_TMP)

# Rules for generating web HTML files with web_html.xsl in web_tmp
$(WEB_TMP)/docs.html: docs.xml web_html.xsl docs.dtd | $(WEB_TMP)
	$(call build_output_files, $@, web_html.xsl, $<)

$(WEB_TMP)/faq.html: faq.xml web_html.xsl docs.dtd | $(WEB_TMP)
	$(call build_output_files, $@, web_html.xsl, $<)

$(WEB_TMP)/studio.html: exult_studio.xml web_html.xsl docs.dtd | $(WEB_TMP)
	$(call build_output_files, $@, web_html.xsl, $<)

.SUFFIXES: .xml .html .txt

define update_files_template
$(1): $(2)
	cp $(2) $(1)
endef

# Rule for updating files in the docs directory
$(eval $(call update_files_template, $(DOCS_DIR)/ReadMe.html, docs.html))
$(eval $(call update_files_template, $(DOCS_DIR)/faq.html, faq.html))
$(eval $(call update_files_template, $(DOCS_DIR)/exult_studio.html, exult_studio.html))
$(eval $(call update_files_template, $(DOCS_DIR)/exult_studio.txt, exult_studio.txt))

# Rules for updating files in the top directory
$(eval $(call update_files_template, $(top_srcdir)/FAQ, faq.txt))
$(eval $(call update_files_template, $(top_srcdir)/README, docs.txt))

ifdef WEB_DIR
$(foreach file, $(TEXT_OBJS), $(eval $(call update_files_template, $(WEB_DIR)/$(file), $(file))))
$(eval $(call update_files_template, $(WEB_DIR)/docs.html, $(WEB_TMP)/docs.html))
$(eval $(call update_files_template, $(WEB_DIR)/faq.html, $(WEB_TMP)/faq.html))
$(eval $(call update_files_template, $(WEB_DIR)/studio.html, $(WEB_TMP)/studio.html))
endif

# remove all generated files
clean:
	-rm -f *.html *.txt
	-rm -rf $(WEB_TMP)

html: $(HTML_OBJS) $(DOCS_HTMLS) $(DOCS_TEXTS)
text: $(TEXT_OBJS) $(TOP_TEXTS)
web-tmp: $(WEB_TMP_HTMLS)

.PHONY: all html text web-tmp clean update