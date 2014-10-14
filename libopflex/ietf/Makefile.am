#
# libopflex: a framework for developing opflex-based policy agents
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
###########
#
# Process this file with automake to produce a Makefile.in

EXTRA_DIST = \
	draft-smith-opflex.xml \
	rfc2629.dtd \
	rfc2629-noinc.xsl \
	rfc2629-other.ent \
	rfc2629-xhtml.ent

BUILT_SOURCES =
MOSTLYCLEANFILES =

if BUILD_SPECIFICATIONS
BUILT_SOURCES += draft-smith-opflex.html draft-smith-opflex.txt
MOSTLYCLEANFILES += draft-smith-opflex.html draft-smith-opflex.txt
endif

if LINT_SPECIFICATIONS
BUILT_SOURCES += draft-smith-opflex.lint
MOSTLYCLEANFILES += draft-smith-opflex.lint
endif

%.html: %.full
	@XML2RFC@ --html $< $@

%.txt: %.full
	@XML2RFC@ --text $< $@

%.chk: rfc2629-refchk.xsl %.xml
	@XSLTPROC@ $^ >$@

%.full: rfc2629-noinc.xsl %.xml
	@XSLTPROC@ $^ >$@

%.lint: %.xml
	@XMLLINT@ --noout --valid $< 2>&1 | tee $@
