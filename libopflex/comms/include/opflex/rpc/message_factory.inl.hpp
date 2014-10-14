#pragma once
#ifndef _____COMMS__INCLUDE__OPFLEX__RPC__MESSAGE_FACTORY_INL_HPP
#define _____COMMS__INCLUDE__OPFLEX__RPC__MESSAGE_FACTORY_INL_HPP

#include <opflex/rpc/methods.hpp>

namespace opflex { namespace rpc {

using namespace opflex::rpc::method;

template <MethodName * M>
OutReq<M> * opflex::rpc::MessageFactory::newReq(
        opflex::comms::internal::CommunicationPeer const & peer,
        PayloadGenerator payloadGenerator
    ) {

    OutReq<M> * newReq = new (std::nothrow)
        opflex::rpc::OutReq<M>(
                peer,
                payloadGenerator
               );

    return newReq;
}

}}

#endif /* _____COMMS__INCLUDE__OPFLEX__RPC__MESSAGE_FACTORY_INL_HPP */
