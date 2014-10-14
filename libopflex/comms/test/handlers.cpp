/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include <opflex/rpc/methods.hpp>

namespace opflex { namespace rpc {

template<>
void InbReq<&opflex::rpc::method::send_identity>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::send_identity>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::send_identity>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::policy_resolve>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::policy_resolve>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::policy_resolve>::process() const {

}


template<>
void InbReq<&opflex::rpc::method::policy_unresolve>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::policy_unresolve>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::policy_unresolve>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::policy_update>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::policy_update>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::policy_update>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::endpoint_declare>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::endpoint_declare>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::endpoint_declare>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::endpoint_undeclare>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::endpoint_undeclare>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::endpoint_undeclare>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::endpoint_resolve>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::endpoint_resolve>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::endpoint_resolve>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::endpoint_unresolve>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::endpoint_unresolve>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::endpoint_unresolve>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::endpoint_update>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::endpoint_update>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::endpoint_update>::process() const {

}

template<>
void InbReq<&opflex::rpc::method::state_report>::process() const {

}
template<>
void InbRes<&opflex::rpc::method::state_report>::process() const {

}
template<>
void InbErr<&opflex::rpc::method::state_report>::process() const {

}

}}
