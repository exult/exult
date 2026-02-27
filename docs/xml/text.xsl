<?xml version="1.0"?>
<!DOCTYPE stylesheet
[
<!ENTITY space "&#x20;">
<!ENTITY cr "&#xa;">
<!ENTITY tab "&#x9;">
]>
<xsl:stylesheet version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns="http://www.w3.org/1999/xhtml">
	<!-- FIX ME - <br> is translated but then immediately filtered out again.
	|   Furthermore, indention of list entries etc. is not done at all. We should
	|   fix this somehow, but how?
	+-->
	<xsl:strip-space elements="*" />
	<xsl:output
		method="text"
		indent="no"
		encoding="UTF-8" />
	<!-- Keys -->
	<xsl:key name="sub_ref" match="sub" use="@name" />
	<xsl:key name="section_ref" match="section" use="@title" />
	<!-- br Line Break trickery Templates
	|   Because we achieve proper formatting through using normalize-space we strip all line breaks.
	|   By changing the ones we really want to Ä first, we can change them back after normalizing
	+-->
	<xsl:template match="br">
		<xsl:text>Ä</xsl:text>
	</xsl:template>
	<!-- Faq Templates -->
	<xsl:template name="TOC">
		<xsl:for-each select="section">
			<xsl:number level="multiple"
				count="section"
				format="1. " />
			<xsl:value-of select="@title" />
			<xsl:text>&#xA;</xsl:text>
			<xsl:for-each
				select="sub">
				<xsl:number level="multiple"
					count="section|sub"
					format="1."
					value="count(ancestor::section/preceding-sibling::section)+1" />
				<xsl:number
					format="1. " />
				<xsl:apply-templates select="header" />
			</xsl:for-each>
			<xsl:text>&#xA;</xsl:text>
		</xsl:for-each>
	</xsl:template>
	<xsl:template match="faqs">
		<xsl:value-of select="@title" />
		<xsl:text> F.A.Q. (frequently asked questions)&#xA;</xsl:text>
		<xsl:text>last changed: </xsl:text>
		<xsl:value-of
			select="@changed" />
		<xsl:text>&#xA;&#xA;</xsl:text>
		<xsl:text>The latest version of this document can be found at https://exult.info/faq.html&#xA;</xsl:text>
		<xsl:text>&#xA;&#xA;</xsl:text>
		<!-- BEGIN TOC -->
		<xsl:call-template
			name="TOC" />
		<!-- END TOC -->
		<!-- BEGIN CONTENT -->
		<xsl:apply-templates select="section" />
		<!-- END CONTENT -->
	</xsl:template>
	<!-- Readme Templates -->
	<xsl:template match="readme">
		<xsl:value-of select="@title" />
		<xsl:text> Documentation&#xA;</xsl:text>
		<xsl:text>last changed: </xsl:text>
		<xsl:value-of
			select="@changed" />
		<xsl:text>&#xA;&#xA;</xsl:text>
		<xsl:text>The latest version of this document can be found at https://exult.info/docs.html&#xA;</xsl:text>
		<xsl:text>&#xA;&#xA;</xsl:text>
		<!-- BEGIN TOC -->
		<xsl:call-template
			name="TOC" />
		<!-- END TOC -->
		<!-- BEGIN CONTENT -->
		<xsl:apply-templates select="section" />
		<!-- END CONTENT -->
	</xsl:template>
	<!-- Studio Docs Templates -->
	<xsl:template match="studiodoc">
		<xsl:value-of select="@title" />
		<xsl:text> Studio Documentation&#xA;</xsl:text>
		<xsl:text>last changed: </xsl:text>
		<xsl:value-of
			select="@changed" />
		<xsl:text>&#xA;&#xA;</xsl:text>
		<xsl:text>The latest version of this document can be found at https://exult.info/studio.html&#xA;</xsl:text>
		<xsl:text>&#xA;&#xA;</xsl:text>
		<!-- BEGIN TOC -->
		<xsl:call-template
			name="TOC" />
		<!-- END TOC -->
		<!-- BEGIN CONTENT -->
		<xsl:apply-templates select="section" />
		<!-- END CONTENT -->
	</xsl:template>
	<!-- section Template -->
	<xsl:template match="section">
		<xsl:text>&#xA;</xsl:text>
		<xsl:text>--------------------------------------------------------------------------------&#xA;</xsl:text>
		<xsl:text>&#xA;</xsl:text>
		<xsl:number format="1. " />
		<xsl:value-of
			select="@title" />
		<xsl:text>&#xA;</xsl:text>
		<xsl:apply-templates select="sub" />
	</xsl:template>
	<!-- Entry Template -->
	<xsl:template match="sub">
		<xsl:variable name="num_idx">
			<xsl:number level="multiple"
				count="section|sub"
				format="1."
				value="count(ancestor::section/preceding-sibling::section)+1" />
			<xsl:number
				format="1. " />
		</xsl:variable>
		<xsl:value-of select="$num_idx" />
		<xsl:apply-templates
			select="header" />
		<xsl:text>&#xA;</xsl:text>
		<xsl:apply-templates select="body" />
		<xsl:text>&#xA;&#xA;</xsl:text>
	</xsl:template>
	<xsl:template match="header">
		<!-- In order to do proper formatting, we have to apply a little trick -
		|   we first store the result tree fragment (RTF) in a variable, then
		|   we can apply normalize-space to this variable. Nifty, hu? ;)
		|   In order to get our wanted line breaks back we translate
		|   Ä to line breaks *after* the normalization. see "br" template on top
		+-->
		<xsl:variable name="data">
			<xsl:apply-templates />
		</xsl:variable>
		<xsl:value-of
			select="translate(normalize-space($data), 'Ä', '&#xA;')" />
		<xsl:text>&#xA;</xsl:text>
	</xsl:template>
	<xsl:template match="body">
		<xsl:apply-templates />
	</xsl:template>
	<!--=========================-->
	<!-- Internal Link Templates -->
	<!--=========================-->
	<xsl:template match="ref">
		<xsl:value-of
			select="count(key('sub_ref',@target)/parent::section/preceding-sibling::section)+1" />
		<xsl:text>.</xsl:text>
		<xsl:value-of
			select="count(key('sub_ref',@target)/preceding-sibling::sub)+1" />
		<xsl:text>.</xsl:text>
	</xsl:template>
	<xsl:template match="ref1">
		<xsl:text>&#xA;</xsl:text>
		<xsl:value-of
			select="count(key('sub_ref',@target)/parent::section/preceding-sibling::section)+1" />
		<xsl:text>.</xsl:text>
		<xsl:value-of
			select="count(key('sub_ref',@target)/preceding-sibling::sub)+1" />
		<xsl:text>. </xsl:text>
		<xsl:apply-templates
			select="key('sub_ref',@target)/child::header" />
	</xsl:template>
	<xsl:template match="section_ref">
		<xsl:text>&#xA;</xsl:text>
		<xsl:value-of
			select="count(key('section_ref',@target)/preceding-sibling::section)+1" />
		<xsl:text>. </xsl:text>
		<xsl:apply-templates
			select="key('section_ref',@target)/@title" />
	</xsl:template>
	<!-- External Link Template -->
	<xsl:template match="extref">
		<xsl:choose>
			<xsl:when test="count(child::node())>0">
				<xsl:value-of select="." />
			</xsl:when>
			<xsl:when test="@doc='faq'">
				<xsl:text>FAQ.txt</xsl:text>
			</xsl:when>
			<xsl:when test="@doc='docs'">
				<xsl:text>ReadMe.txt</xsl:text>
			</xsl:when>
			<xsl:when test="@doc='studio'">
				<xsl:text>exult_studio.txt</xsl:text>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="@target" />
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>
	<!--================-->
	<!-- Misc Templates -->
	<!--================-->
	<xsl:template match="para">
		<!-- Same trick as in the header template -->
		<xsl:variable name="data">
			<xsl:apply-templates />
		</xsl:variable>
		<xsl:value-of
			select="translate(normalize-space($data), 'Ä', '&#xA;')" />
		<xsl:text>&#xA;</xsl:text>
	</xsl:template>
	<xsl:template match="cite">
		<xsl:if test="position()!=1">
			<xsl:text>&#xA;</xsl:text>
		</xsl:if>
		<xsl:value-of select="@name" />
		<xsl:text>:&#xA;</xsl:text>
		<!-- Same trick as in the header template -->
		<xsl:variable
			name="data">
			<xsl:apply-templates />
		</xsl:variable>
		<xsl:value-of
			select="translate(normalize-space($data), 'Ä', '&#xA;')" />
		<xsl:text>&#xA;</xsl:text>
	</xsl:template>
	<xsl:template match="ol">
		<xsl:for-each select="li">
			<xsl:text></xsl:text>
			<xsl:number format="1. " />
			<xsl:variable name="data">
				<xsl:apply-templates />
			</xsl:variable>
			<xsl:value-of
				select="translate(normalize-space($data), 'Ä', '&#xA;')" />
			<xsl:text>&#xA;</xsl:text>
		</xsl:for-each>
		<xsl:text>&#xA;</xsl:text>
	</xsl:template>
	<xsl:template match="ul">
		<xsl:for-each select="li">
			<xsl:text>* </xsl:text>
			<xsl:variable name="data">
				<xsl:apply-templates />
			</xsl:variable>
			<xsl:value-of
				select="translate(normalize-space($data), 'Ä', '&#xA;')" />
			<xsl:text>&#xA;</xsl:text>
		</xsl:for-each>
		<xsl:text>&#xA;</xsl:text>
	</xsl:template>
	<xsl:template match="key">'<xsl:value-of select="." />'</xsl:template>
	<xsl:template match="Exult">
		<xsl:text>Exult</xsl:text>
	</xsl:template>
	<xsl:template match="Studio">
		<xsl:text>Exult Studio</xsl:text>
	</xsl:template>
	<xsl:template match="Pentagram">
		<xsl:text>Pentagram</xsl:text>
	</xsl:template>
	<xsl:template match="TM">
		<xsl:text>™&#160;</xsl:text>
	</xsl:template>
	<!--=======================-->
	<!-- Key Command Templates -->
	<!--=======================-->
	<xsl:template match="keytable">
		<table border="0" cellpadding="0" cellspacing="0" width="0">
			<tr>
				<th colspan="3" class="left-aligned">
					<xsl:text>&#xA;</xsl:text>
					<xsl:value-of select="@title" />
					<xsl:text>&#xA;</xsl:text>
				</th>
			</tr>
			<xsl:apply-templates select="keydesc" />
		</table>
	</xsl:template>
	<xsl:template match="keydesc">
		<tr>
			<td>
				<span class="highlight">
					<xsl:value-of select="@name" />
				</span>
			</td>
			<td>
				<xsl:text> : </xsl:text>
			</td>
			<td>
				<xsl:value-of select="." />
			</td>
			<xsl:text>&#xA;</xsl:text>
		</tr>
	</xsl:template>
	<!--========================-->
	<!-- Config Table Templates -->
	<!--========================-->
	<xsl:template match="configdesc">
		<table border="0" cellpadding="0" cellspacing="0">
			<xsl:apply-templates select="configtag" />
		</table>
	</xsl:template>
	<xsl:strip-space elements="configtag" />
	<xsl:template match="configtag">
		<xsl:param name="indent"></xsl:param>
		<xsl:value-of select="$indent" />
		<xsl:text>&lt;</xsl:text>
		<xsl:value-of
			select="@name" />
		<xsl:text>&gt;&cr;</xsl:text>
		<xsl:choose>
			<xsl:when test="count(child::configtag)>0">
				<xsl:apply-templates select="configtag">
					<xsl:with-param name="indent">
						<xsl:value-of select="$indent" /><xsl:text>&space;&space;</xsl:text>
					</xsl:with-param>
				</xsl:apply-templates>
			</xsl:when>
			<xsl:otherwise>
				<xsl:value-of select="$indent" />
				<xsl:value-of select="normalize-space(text())" /><xsl:text>&cr;</xsl:text>
				<xsl:apply-templates select="comment" />
			</xsl:otherwise>
		</xsl:choose>
		<xsl:if
			test="@closing-tag='yes'">
			<xsl:value-of select="$indent" />
			<xsl:text>&lt;/</xsl:text>
			<xsl:value-of select="@name" />
			<xsl:text>&gt;&cr;</xsl:text>
		</xsl:if>
	</xsl:template>
	<xsl:template match="comment"><xsl:text>&tab;&tab;&tab;&tab;</xsl:text>
		<xsl:apply-templates /><xsl:text>&cr;</xsl:text>
	</xsl:template>
	<!-- Clone template. Allows one to use any XHTML in the source file -->
	<!--
	<xsl:template match="@*|node()">
		<xsl:copy>
			<xsl:apply-templates select="@*|node()"/>
		</xsl:copy>
	</xsl:template>
	-->
</xsl:stylesheet>