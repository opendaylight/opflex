/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for SpanManager class.
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */


#include <opflexagent/SpanManager.h>
#include <opflexagent/logging.h>
#include <modelgbp/span/Universe.hpp>
#include <modelgbp/domain/Config.hpp>
#include <opflex/modb/PropertyInfo.h>

#include <modelgbp/gbp/EpGroup.hpp>
#include <modelgbp/span/LocalEp.hpp>
#include <boost/optional/optional_io.hpp>
#include <opflex/modb/Mutator.h>

namespace opflexagent {

    using namespace std;
    using namespace modelgbp::epr;
    using namespace opflex::modb;
    using boost::optional;
    using opflex::modb::class_id_t;
    using opflex::modb::URI;
    using boost::posix_time::milliseconds;
    using opflex::modb::Mutator;

    SpanManager::SpanManager(opflex::ofcore::OFFramework &framework_,
                             boost::asio::io_service& agent_io_) :
            spanUniverseListener(*this), framework(framework_),
            taskQueue(agent_io_){

    }

    template<typename K, typename V>
    static void print_map(unordered_map<K,V> const& m) {
        for (auto const &pair: m) {
            LOG(DEBUG) << "{" << pair.first << ":" << pair.second << "}\n";
        }
    }

    void SpanManager::start() {
        LOG(DEBUG) << "starting span manager";
        Universe::registerListener(framework, &spanUniverseListener);
        LocalEp::registerListener(framework, &spanUniverseListener);
        L2Ep::registerListener(framework, &spanUniverseListener);
    }

    void SpanManager::stop() {
        Universe::unregisterListener(framework, &spanUniverseListener);
        LocalEp::unregisterListener(framework, &spanUniverseListener);
        L2Ep::unregisterListener(framework, &spanUniverseListener);
    }

    SpanManager::SpanUniverseListener::SpanUniverseListener(SpanManager &spanManager) :
            spanmanager(spanManager) {}

    SpanManager::SpanUniverseListener::~SpanUniverseListener() {}

    void SpanManager::SpanUniverseListener::objectUpdated(class_id_t classId,
                                                          const URI &uri) {
        LOG(DEBUG) << "update on class ID " << classId << " URI " << uri;

        if (classId == Universe::CLASS_ID) {
            optional <shared_ptr<Universe>> univ_opt =
                    Universe::resolve(spanmanager.framework);
            if (univ_opt) {
                vector <shared_ptr<Session>> sessVec;
                univ_opt.get()->resolveSpanSession(sessVec);
                for (shared_ptr <Session> sess : sessVec) {
                    LOG(DEBUG) << "update on session " << sess->getURI();
                    processSession(sess);

                    vector <shared_ptr<SrcGrp>> srcGrpVec;
                    sess.get()->resolveSpanSrcGrp(srcGrpVec);
                    for (shared_ptr <SrcGrp> srcGrp : srcGrpVec) {
                        processSrcGrp(srcGrp);
                    }
                    vector <shared_ptr<DstGrp>> dstGrpVec;
                    sess.get()->resolveSpanDstGrp(dstGrpVec);
                    for (shared_ptr<DstGrp> dstGrp : dstGrpVec) {
                        processDstGrp(*dstGrp, *sess);
                    }
                    spanmanager.notifyUri.insert(sess->getURI());
                 }
            }
        } else if (classId == LocalEp::CLASS_ID) {
            processLocalEp(uri);
        } else if (classId == L2Ep::CLASS_ID) {
            if (L2Ep::resolve(spanmanager.framework, uri)) {
                shared_ptr <L2Ep> l2Ep = L2Ep::resolve(
                        spanmanager.framework, uri).get();
                processL2Ep(l2Ep);
            }
        }
        // notify all listeners. put it on a task Q for non blocking notification.
        for (URI uri : spanmanager.notifyUri) {
            spanmanager.taskQueue.dispatch(uri.toString(), [=]() { spanmanager.notifyListeners(uri); });
        }
        spanmanager.notifyUri.clear();

    }

    void SpanManager::registerListener(PolicyListener *listener) {
        LOG(DEBUG) << "registering listener";
        lock_guard <mutex> guard(listener_mutex);
        spanListeners.push_back(listener);
    }

    void SpanManager::unregisterListener(PolicyListener* listener) {
        lock_guard <mutex> guard(listener_mutex);
        spanListeners.remove(listener);
    }

    void SpanManager::notifyListeners(const URI& spanURI) {
        LOG(DEBUG) << "notifying listener";
        lock_guard<mutex> guard(listener_mutex);
        for (PolicyListener *listener : spanListeners) {
            listener->spanUpdated(spanURI);
        }
    }

    optional<shared_ptr<SpanManager::SessionState>>
        SpanManager::getSessionState(const URI& uri) const {
        unordered_map<URI, shared_ptr<SpanManager::SessionState>>
                ::const_iterator itr = sess_map.find(uri);
        if (itr == sess_map.end()) {
            return boost::none;
        } else {
            return itr->second;
        }
    }

    void SpanManager::SpanUniverseListener::processSession(shared_ptr<Session> sess) {
        shared_ptr<SessionState> sessState;
        auto itr = spanmanager.sess_map.find(sess->getURI());
        if (itr == spanmanager.sess_map.end()) {
            sessState = make_shared<SessionState>(sess->getURI(), sess->getName().get());
            spanmanager.sess_map.insert(make_pair(sess->getURI(), sessState));
        } else {
            sessState = itr->second;
        }
        print_map(spanmanager.sess_map);
    }

    void SpanManager::SpanUniverseListener::processSrcGrp(shared_ptr<SrcGrp> srcGrp) {
        vector<shared_ptr<SrcMember>> srcMemVec;
        srcGrp.get()->resolveSpanSrcMember(srcMemVec);
        for (shared_ptr<SrcMember> srcMem : srcMemVec) {
            optional <shared_ptr<MemberToRefRSrc>> memRefOpt =
                    srcMem.get()->resolveSpanMemberToRefRSrc();
            if (memRefOpt) {
                shared_ptr <MemberToRefRSrc> memRef = memRefOpt.get();
                if (memRef->getTargetClass()) {
                    class_id_t class_id = memRef->getTargetClass().get();
                    if (class_id == modelgbp::gbp::EpGroup::CLASS_ID) {
                    } else if (class_id == LocalEp::CLASS_ID) {
                        if (memRef->getTargetURI()) {
                            URI pUri = memRef->getTargetURI().get();
                            processLocalEp(pUri);
                        }
                    }
                }
            }
        }
    }

    void SpanManager::SpanUniverseListener::processDstGrp(DstGrp& dstGrp,
                                            Session& sess) {
        unordered_map<URI, shared_ptr<SessionState>>
            ::const_iterator seSt = spanmanager.sess_map.find(sess.getURI());
        if (seSt != spanmanager.sess_map.end()) {
            vector <shared_ptr<DstMember>> dstMemVec;
            dstGrp.resolveSpanDstMember(dstMemVec);
            for (shared_ptr <DstMember> dstMem : dstMemVec) {
                optional <shared_ptr<DstSummary>> dstSumm =
                        dstMem->resolveSpanDstSummary();
                if (dstSumm) {
                    optional<const string &> dest = dstSumm.get()->getDest();
                    if (dest) {
                        address ip = boost::asio::ip::address::from_string(dest.get());
                        shared_ptr<DstEndPoint> dEp = make_shared<DstEndPoint>(ip);
                        seSt->second->addDstEndPoint(dstSumm.get()->getURI(), dEp);
                    }
                }
            }
        }
    }

    void SpanManager::SessionState::addDstEndPoint
                         (const URI& uri, shared_ptr<DstEndPoint> dEp) {
        dstEndPoints.emplace(uri, dEp);
    }

    void SpanManager::SessionState::addSrcEndPoint
                   (const URI& uri, shared_ptr<SourceEndPoint> srcEp) {
        srcEndPoints.emplace(uri, srcEp);
    }

    void SpanManager::SpanUniverseListener::processLocalEp(const URI& pUri) {
            if (LocalEp::resolve(spanmanager.framework, pUri)) {
                shared_ptr <LocalEp> lEp = LocalEp::resolve(spanmanager.framework, pUri).get();
                if (lEp->resolveSpanLocalEpToEpRSrc()) {
                    shared_ptr <LocalEpToEpRSrc> epRSrc = lEp->resolveSpanLocalEpToEpRSrc().get();
                    if (epRSrc.get()->getTargetURI()) {
                        URI epUri = epRSrc.get()->getTargetURI().get();
                        if (L2Ep::resolve(spanmanager.framework, epUri)) {
                            shared_ptr <L2Ep> l2Ep = L2Ep::resolve(
                                    spanmanager.framework, epUri).get();
                        } else {
                            spanmanager.l2EpUri.emplace(epUri, lEp);
                            print_map(spanmanager.l2EpUri);
                        }
                    }
                }
            }
    }

    const unordered_map<URI, shared_ptr<SpanManager::SourceEndPoint>>&
        SpanManager::SessionState::getSrcEndPointMap() const {
        return srcEndPoints;
    }

    const unordered_map<URI, shared_ptr<SpanManager::DstEndPoint>>&
        SpanManager::SessionState::getDstEndPointMap() const {
        return dstEndPoints;
    }

    void SpanManager::SpanUniverseListener::
        addEndPoint(shared_ptr<LocalEp> lEp, shared_ptr<L2Ep> l2Ep) {
        LOG(DEBUG) << "get parent lEp " << (lEp ? "set" : "null") <<
                      " l2Ep " << (l2Ep ? "set" : "null");
        optional<URI> parent = spanmanager.framework.
                               getParent(lEp->CLASS_ID, lEp->getURI());
        if (parent) {
            spanmanager.notifyUri.insert(parent.get());
            optional<shared_ptr<Session>> sess = Session::resolve(spanmanager.framework,
                    parent.get());
            if (sess) {
                shared_ptr<SessionState> sesSt = spanmanager.sess_map[sess.get()->getURI()];
                shared_ptr<SpanManager::SourceEndPoint> srcEp =
                          make_shared<SpanManager::SourceEndPoint>(lEp->getName().get(),
                                                      l2Ep->getInterfaceName().get());
                sesSt->addSrcEndPoint(lEp->getURI(), srcEp);
                print_map(sesSt->getSrcEndPointMap());
            }
        }
    }

    void SpanManager::SpanUniverseListener::processL2Ep(shared_ptr<L2Ep> l2Ep) {
        LOG(DEBUG) << "processl2Ep shared_ptr " << (l2Ep ? "not null" : "null");
        auto val = spanmanager.l2EpUri.find(l2Ep->getURI());
        if (val == spanmanager.l2EpUri.end()) {
            return;
        } else {
            addEndPoint(val->second, l2Ep);
        }
    }

    SpanManager::SessionState::SessionState(const URI& uri,
                               const string& name) : uri(uri), name(name) {}

    SpanManager::SourceEndPoint::SourceEndPoint(const string& name_,
            const string& port_) : name(name_), port(port_) {}

    ostream& operator<<(ostream& os,
             const SpanManager::SourceEndPoint& sEp) {
        os << "name: " << sEp.getName() << " port: " << sEp.getPort();
        return os;
    }



}


