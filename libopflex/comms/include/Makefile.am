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
# Install the yajr headers

comms_headers =
comms_headers += yajr/internal/comms.hpp
comms_headers += yajr/yajr.hpp
comms_headers += yajr/rpc/internal/fnv_1a_64.hpp
comms_headers += yajr/rpc/internal/json_stream_wrappers.hpp
comms_headers += yajr/rpc/message_factory.hpp
comms_headers += yajr/rpc/message_factory.inl.hpp
comms_headers += yajr/rpc/method_lookup.hpp
comms_headers += yajr/rpc/methods.hpp
comms_headers += yajr/rpc/send_handler.hpp
comms_headers += yajr/rpc/rpc.hpp
comms_headers += yajr/rpc/gen/echo.hpp
comms_headers += yajr/transport/engine.hpp
comms_headers += yajr/transport/PlainText.hpp
comms_headers += yajr/transport/ZeroCopyOpenSSL.hpp

if SEPARATE_COMMS
  nobase_include_HEADERS = $(comms_headers)

  codes_includedir = $(includedir)
endif

EXTRA_DIST = $(comms_headers)

uninstall-local: 
	@$(NORMAL_UNINSTALL)
	rm -rf "${DESTDIR}/${includedir}/yajr"
