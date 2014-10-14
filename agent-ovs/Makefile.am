#
# agent-ovs: Opflex agent for integrating Group-based policy with OVS
# Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v1.0 which accompanies this distribution,
# and is available at http://www.eclipse.org/legal/epl-v10.html
#
###########
#
# Process this file with automake to produce a Makefile.in

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS=agent_ovs

agent_ovs_CXXFLAGS = $(libopflex_CFLAGS) $(libmodelgbp_CFLAGS)
agent_ovs_SOURCES = \
	src/main.cpp
agent_ovs_LDADD = $(libopflex_LIBS) $(libmodelgbp_LIBS)

doc/html: $(model_include_HEADERS) doc/Doxyfile 
	cd doc && ${DOXYGEN} Doxyfile
doc: doc/html

clean-doc:
	rm -rf doc/html doc/latex
clean-local: clean-doc
	rm -f *.rpm
