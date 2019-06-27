#pragma once
#ifndef _____COMMS__INCLUDE__OPFLEX__RPC__MESSAGE_FACTORY_INL_HPP
#define _____COMMS__INCLUDE__OPFLEX__RPC__MESSAGE_FACTORY_INL_HPP

#include <yajr/rpc/methods.hpp>

namespace yajr {
    namespace rpc {

using namespace yajr::rpc::method;

template <MethodName * M>
OutReq<M> * yajr::rpc::MessageFactory::newReq(
        PayloadGenerator const & payloadGenerator,
        yajr::Peer const * peer
    ) {

    OutReq<M> * newReq = new (std::nothrow)
        yajr::rpc::OutReq<M>(
                payloadGenerator,
                peer
               );

    return newReq;
}

} /* yajr::rpc namespace */
} /* yajr namespace */

#endif /* _____COMMS__INCLUDE__OPFLEX__RPC__MESSAGE_FACTORY_INL_HPP */

