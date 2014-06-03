bin_PROGRAMS += opflexd/opflexd
opflexd_SOURCES = \
	opflex_conf.c opflex_conf.h \
	opflex_server.c

# set the include path found by configure
# INCLUDES= $(all_includes)

opflexd_CFLAGS=-g -Wall -I/usr/local/include -I../lib 
opflexd_CPPFLAGS=-DBSD_COMP=1 

# the library search path.
opflexd_LDADD=../lib/.libs/libopflexproxy.so.a ../lib/opflexagent/common/src/libopflexagent/lib/libopflexSDK.so.1.0.0
opflexd_LDFLAGS = $(all_libraries)
# opflexd_LDFLAGS = -L/usr/lib -lnsl -lm -lpthread -L/usr/local/lib

DISTCLEANFILES += opflex/opflexd 
DISTCLEANFILES += opflex/*.o

