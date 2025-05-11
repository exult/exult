<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
	xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
	xmlns="http://www.w3.org/1999/xhtml">
	<xsl:output method="xml" indent="no" omit-xml-declaration="yes" />
	<!-- Keys -->
	<xsl:key name="sub_ref" match="sub" use="@name" />
	<xsl:key name="section_ref" match="section" use="@title" />
	<xsl:template match="br"><br/></xsl:template>
	<xsl:template match="hr"><hr/></xsl:template>
	<xsl:strip-space elements="*" />
	<!-- FAQ Templates -->
	<xsl:template name="TOC">
		<xsl:for-each select="section">
			<p>
				<a>
					<xsl:attribute name="href">#<xsl:value-of select="translate(normalize-space(@title), ' ', '-')" /></xsl:attribute>
					<xsl:number level="multiple" count="section" format="1. " />
					<xsl:value-of select="@title" />
				</a>
				<br />
				<xsl:for-each select="sub">
					<a>
						<xsl:attribute name="href">#<xsl:value-of select="translate(normalize-space(@name), ' ', '-')" /></xsl:attribute>
						<xsl:number level="multiple"
							count="section|sub"
							format="1."
							value="count(ancestor::section/preceding-sibling::section)+1" />
						<xsl:number format="1. " />
						<xsl:apply-templates select="header" />
					</a>
					<br />
				</xsl:for-each>
			</p>
		</xsl:for-each>
	</xsl:template>
	<!-- FAQ Template -->
	<xsl:template match="faqs">
		<test>
			<p>last changed:
				<xsl:value-of select="@changed" />
			</p>
			<hr />
			<p> A text only version can be found
				<a href="https://exult.info/faq.txt">here</a>
			</p>
			<br />
			<!-- BEGIN TOC -->
			<xsl:call-template name="TOC" />
			<!-- END TOC -->
			<!-- BEGIN CONTENT -->
			<xsl:apply-templates select="section" />
			<!-- END CONTENT -->
		</test>
	</xsl:template>
	<!-- Readme Template -->
	<xsl:template match="readme">
		<test>
			<p>last changed:
				<xsl:value-of select="@changed" />
			</p>
			<hr />
			<p> A text only version can be found
				<a href="https://exult.info/docs.txt">here</a>
			</p>
			<br />
			<!-- BEGIN TOC -->
			<xsl:call-template name="TOC" />
			<!-- END TOC -->
			<!-- BEGIN CONTENT -->
			<xsl:apply-templates select="section" />
			<!-- END CONTENT -->
		</test>
	</xsl:template>
	<!-- Studio Docs Template -->
	<xsl:template match="studiodoc">
		<test>
			<p>last changed:
				<xsl:value-of select="@changed" />
			</p>
			<hr />
			<p> A text only version can be found
				<a href="https://exult.info/exult_studio.txt">here</a>
			</p>
			<br />
			<!-- BEGIN TOC -->
			<xsl:call-template name="TOC" />
			<!-- END TOC -->
			<!-- BEGIN CONTENT -->
			<xsl:apply-templates select="section" />
			<!-- END CONTENT -->
		</test>
	</xsl:template>
	<!-- Group Template -->
	<xsl:template match="section">
		<hr class="full-width" />
		<table class="full-width">
			<tr><th class="left-aligned">
					<a>
						<xsl:attribute name="id"><xsl:value-of select="translate(normalize-space(@title), ' ', '-')" /></xsl:attribute>
						<xsl:number format="1. " />
						<xsl:value-of select="@title" />
					</a>
				</th>
			</tr>
			<xsl:apply-templates select="sub" />
		</table>
	</xsl:template>
	<!-- Entry Template -->
	<xsl:template match="sub">
		<xsl:variable name="num_idx">
			<xsl:number level="single"
				count="section"
				format="1."
				value="count(ancestor::section/preceding-sibling::section)+1" />
			<xsl:number format="1. " />
		</xsl:variable>
		<tr>
			<td>
				<xsl:text disable-output-escaping="yes">&amp;nbsp;</xsl:text>
			</td>
		</tr>
		<tr>
			<td>
				<strong>
					<a>
						<xsl:attribute name="id"><xsl:value-of select="translate(normalize-space(@name), ' ', '-')" /></xsl:attribute>
						<xsl:value-of select="$num_idx" />
						<xsl:apply-templates select="header" />
					</a>
				</strong>
			</td>
		</tr>
		<tr>
			<td>
				<xsl:apply-templates select="body" />
				<xsl:comment></xsl:comment>
			</td>
		</tr>
	</xsl:template>
	<xsl:template match="header">
		<!--
		<xsl:variable name = "data">
			<xsl:apply-templates/>
		</xsl:variable>
		<xsl:value-of select="normalize-space($data)"/>
		-->
		<xsl:apply-templates />
	</xsl:template>
	<xsl:template match="body">
		<!--
		<xsl:variable name = "data">
			<xsl:apply-templates/>
		</xsl:variable>
		<xsl:value-of select="normalize-space($data)"/>
		-->
		<xsl:apply-templates />
	</xsl:template>
	<!-- Internal Link Templates -->
	<xsl:template match="ref">
		<a>
			<xsl:attribute name="href">#<xsl:value-of select="translate(normalize-space(@target), ' ', '-')" /></xsl:attribute>
			<xsl:value-of
				select="count(key('sub_ref',@target)/parent::section/preceding-sibling::section)+1" />
			<xsl:text>.</xsl:text>
			<xsl:value-of select="count(key('sub_ref',@target)/preceding-sibling::sub)+1" />
			<xsl:text>.</xsl:text>
		</a>
	</xsl:template>
	<xsl:template match="ref1">
		<a>
			<xsl:attribute name="href">#<xsl:value-of select="translate(normalize-space(@target), ' ', '-')" /></xsl:attribute>
			<xsl:value-of
				select="count(key('sub_ref',@target)/parent::section/preceding-sibling::section)+1" />
			<xsl:text>.</xsl:text>
			<xsl:value-of select="count(key('sub_ref',@target)/preceding-sibling::sub)+1" />
			<xsl:text>. </xsl:text>
			<xsl:apply-templates select="key('sub_ref',@target)/child::header" />
		</a>
	</xsl:template>
	<xsl:template match="section_ref">
		<a>
			<xsl:attribute name="href">#<xsl:value-of select="translate(normalize-space(@target), ' ', '-')" /></xsl:attribute>
			<xsl:value-of select="count(key('section_ref',@target)/preceding-sibling::section)+1" />
			<xsl:text>. </xsl:text>
			<xsl:apply-templates select="key('section_ref',@target)/@title" />
		</a>
	</xsl:template>
	<!-- External Link Template -->
	<xsl:template match="extref">
		<a>
			<xsl:attribute name="href">
				<xsl:choose>
					<xsl:when test="@doc='faq'">
						<xsl:text>faq.php#</xsl:text>
					</xsl:when>
					<xsl:when test="@doc='docs'">
						<xsl:text>docs.php#</xsl:text>
					</xsl:when>
					<xsl:when test="@doc='studio'">
						<xsl:text>studio.php#</xsl:text>
					</xsl:when>
				</xsl:choose>
				<xsl:value-of select="translate(normalize-space(@target), ' ', '-')" />
			</xsl:attribute>
			<xsl:choose>
				<xsl:when test="count(child::node())>0">
					<xsl:value-of select="." />
				</xsl:when>
				<xsl:when test="@doc='faq'">
					<xsl:text>FAQ</xsl:text>
				</xsl:when>
				<xsl:when test="@doc='docs'">
					<xsl:text>Documentation</xsl:text>
				</xsl:when>
				<xsl:when test="@doc='studio'">
					<xsl:text>Studio Documentation</xsl:text>
				</xsl:when>
				<xsl:otherwise>
					<xsl:value-of select="@target" />
				</xsl:otherwise>
			</xsl:choose>
		</a>
	</xsl:template>
	<!-- Image Link Template -->
	<xsl:template match="img">
		<xsl:element name="{local-name()}">
			<xsl:for-each select="@*|node()">
				<xsl:copy />
			</xsl:for-each>
		</xsl:element>
	</xsl:template>
	<!-- Misc Templates -->
	<xsl:template match="Exult">
		<em>Exult</em>
	</xsl:template>
	<xsl:template match="Studio">
		<em>Exult Studio</em>
	</xsl:template>
	<xsl:template match="Pentagram">
		<em>Pentagram</em>
	</xsl:template>
	<xsl:template match="cite">
		<p>
			<xsl:value-of select="@name" />:<br />
			<cite>
				<xsl:apply-templates />
			</cite>
		</p>
	</xsl:template>
	<xsl:template match="para">
		<p>
			<xsl:apply-templates />
		</p>
	</xsl:template>
	<xsl:template match="key">'<span class="highlight"><xsl:value-of select="." /></span>'</xsl:template>
	<xsl:template match="kbd">
		<span class="highlight">
			<kbd>
				<xsl:apply-templates />
			</kbd>
		</span>
	</xsl:template>
	<xsl:template match="TM">
		<xsl:text disable-output-escaping="yes">&amp;trade;&amp;nbsp;</xsl:text>
	</xsl:template>
	<!-- ...................ol|dl|ul + em............... -->
	<xsl:template match="ul|ol|li|strong|q">
		<xsl:element name="{local-name()}">
			<xsl:apply-templates select="@*|node()" />
		</xsl:element>
	</xsl:template>
	<xsl:template match="em">
		<b>
			<i>
				<span style="font-size: larger">
					<xsl:apply-templates />
				</span>
			</i>
		</b>
	</xsl:template>
	<!-- Key Command Templates -->
	<xsl:template match="keytable">
		<table class="key_table wide-table">
			<tr>
				<th colspan="3" class="left-aligned">
					<xsl:value-of select="@title" />
				</th>
			</tr>
			<xsl:apply-templates select="keydesc" />
		</table>
	</xsl:template>
	<xsl:template match="keydesc">
		<tr>
			<td>
				<span class="highlight"><xsl:value-of select="@name" /></span>
				<xsl:comment></xsl:comment>
			</td>
			<td><xsl:comment></xsl:comment></td>
			<td>
				<xsl:value-of select="." />
				<xsl:comment></xsl:comment>
			</td>
		</tr>
	</xsl:template>
	<!-- Config Table Templates -->
	<xsl:template match="configdesc">
		<table class="borderless">
			<xsl:apply-templates select="configtag" />
		</table>
	</xsl:template>
	<xsl:template match="configtag">
		<xsl:param name="indent">0</xsl:param>
		<xsl:variable name="row-class">
			<xsl:if test="@manual">
		highlight</xsl:if>
		</xsl:variable>
		<xsl:choose>
			<xsl:when test="@manual">
				<xsl:choose>
					<xsl:when test="count(child::configtag)>0">
						<tr class="{$row-class}">
							<td style="text-indent:{$indent}pt">
								<xsl:text>&lt;</xsl:text>
								<xsl:value-of select="@name" />
								<xsl:text>&gt;</xsl:text>
								<xsl:comment></xsl:comment>
							</td>
							<td>
								<xsl:apply-templates select="comment"/>
							</td>
						</tr>
						<xsl:apply-templates select="configtag">
							<xsl:with-param name="indent">
								<xsl:value-of select="$indent+16" />
							</xsl:with-param>
						</xsl:apply-templates>
					</xsl:when>
					<xsl:otherwise>
						<tr class="{$row-class}">
							<td style="text-indent:{$indent}pt">
								<xsl:text>&lt;</xsl:text>
								<xsl:value-of select="@name" />
								<xsl:text>&gt;</xsl:text>
								<xsl:comment></xsl:comment>
							</td>
							<td rowspan="3">
								<xsl:apply-templates select="comment"/>
							</td>
						</tr>
						<tr class="{$row-class}">
							<td style="text-indent:{$indent}pt">
								<xsl:value-of select="text()" />
								<xsl:comment></xsl:comment>
							</td>
						</tr>
					</xsl:otherwise>
				</xsl:choose>
				<xsl:if
					test="@closing-tag='yes'">
					<tr class="{$row-class}">
						<td style="text-indent:{$indent}pt">
							<xsl:text>&lt;/</xsl:text>
							<xsl:value-of select="@name" />
							<xsl:text>&gt;</xsl:text>
							<xsl:comment></xsl:comment>
						</td>
					</tr>
				</xsl:if>
			</xsl:when>
			<xsl:otherwise>
				<xsl:choose>
					<xsl:when test="count(child::configtag)>0">
						<tr>
							<td style="text-indent:{$indent}pt">
								<xsl:text>&lt;</xsl:text>
								<xsl:value-of select="@name" />
								<xsl:text>&gt;</xsl:text>
								<xsl:comment></xsl:comment>
							</td>
							<td>
								<xsl:apply-templates select="comment"/>
							</td>
						</tr>
						<xsl:apply-templates select="configtag">
							<xsl:with-param name="indent">
								<xsl:value-of select="$indent+16" />
							</xsl:with-param>
						</xsl:apply-templates>
					</xsl:when>
					<xsl:otherwise>
						<tr>
							<td style="text-indent:{$indent}pt">
								<xsl:text>&lt;</xsl:text>
								<xsl:value-of select="@name" />
								<xsl:text>&gt;</xsl:text>
								<xsl:comment></xsl:comment>
							</td>
							<td rowspan="3">
								<xsl:apply-templates select="comment"/>
							</td>
						</tr>
						<tr>
							<td style="text-indent:{$indent}pt">
								<xsl:value-of select="text()" />
								<xsl:comment></xsl:comment>
							</td>
						</tr>
					</xsl:otherwise>
				</xsl:choose>
				<xsl:if
					test="@closing-tag='yes'">
					<tr>
						<td style="text-indent:{$indent}pt">
							<xsl:text>&lt;/</xsl:text>
							<xsl:value-of select="@name" />
							<xsl:text>&gt;</xsl:text>
							<xsl:comment></xsl:comment>
						</td>
					</tr>
				</xsl:if>
			</xsl:otherwise>
		</xsl:choose>
	</xsl:template>
	<xsl:template match="comment">
		<span class="non-selectable-comment">
			<xsl:apply-templates />
		</span>
	</xsl:template>
</xsl:stylesheet>