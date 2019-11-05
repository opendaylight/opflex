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
#include <opflex/modb/PropertyInfo.h>

#include <modelgbp/gbp/EpGroup.hpp>
#include <modelgbp/span/LocalEp.hpp>
#include <boost/optional/optional_io.hpp>
#include <opflex/modb/Mutator.h>

#include <modelgbp/epr/L2Universe.hpp>
#include <modelgbp/epdr/EndPointToGroupRSrc.hpp>

namespace opflexagent {

    using namespace std;
    using namespace modelgbp::epr;
    using namespace modelgbp::gbp;
    using namespace opflex::modb;
    using boost::optional;
    using opflex::modb::class_id_t;
    using opflex::modb::URI;
    using boost::posix_time::milliseconds;
    using opflex::modb::Mutator;

    using namespace modelgbp::epdr;

    SpanManager::SpanManager(opflex::ofcore::OFFramework &framework_,
                             boost::asio::io_service& agent_io_) :
            spanUniverseListener(*this), framework(framework_),
            taskQueue(agent_io_){}

    template<typename K, typename V>
    static void print_map(unordered_map<K,V> const& m) {
        for (auto const &pair: m) {
            LOG(DEBUG) << "{" << pair.first << ":" << pair.second << "}\n";
        }
    }

    template<typename K>
    static void print_set(unordered_set<K> const& m) {
        for (auto const &elem: m) {
            LOG(DEBUG) << "{" << elem << "}\n";
        }
    }

    static void print_set(SessionState::srcEpSet const& m) {
        for (auto const &elem: m) {
             LOG(DEBUG) << "{" << elem << "}\n";
        }
    }

    ostream& operator<<(ostream& os,
             const SourceEndPoint& sEp) {
        os << "name: " << sEp.getName() << " port: " << sEp.getPort()
                << " dir: " << sEp.getDirection();
        return os;
    }

    void SpanManager::start() {
        LOG(DEBUG) << "starting span manager";
        Universe::registerListener(framework, &spanUniverseListener);
        Session::registerListener(framework,&spanUniverseListener);
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

        // updates on parent container for session are received for
        // session creation. Deletion and modification updates are
        // sent to the object itself.
        if (classId == Universe::CLASS_ID) {
            optional <shared_ptr<Universe>> univ_opt =
                    Universe::resolve(spanmanager.framework);
            if (univ_opt) {
                vector <shared_ptr<Session>> sessVec;
                univ_opt.get()->resolveSpanSession(sessVec);
                for (shared_ptr <Session> sess : sessVec) {
                    auto itr = spanmanager.sess_map.find(sess->getURI());
                    if (itr == spanmanager.sess_map.end()) {
                        LOG(DEBUG) << "creating session " << sess->getURI();
                        processSession(sess);
                    }
                    spanmanager.notifyUpdate.insert(sess->getURI());
                 }
            }
        } else if (classId == LocalEp::CLASS_ID) {
            optional<shared_ptr<LocalEp>> lEp =
                    LocalEp::resolve(spanmanager.framework, uri);
            if (lEp) {
                optional<shared_ptr<SrcMember>> pSmem =
                                spanmanager.findSrcMem(lEp.get());
                if (pSmem) {
                    optional<const unsigned char> dir = pSmem.get()->getDir();
                    if (dir) {
                        srcMemInfo sMem { .uri = uri, .dir = dir.get()};
                        processLocalEp(sMem);
                    }
                }
            }
        } else if (classId == L2Ep::CLASS_ID) {
            if (L2Ep::resolve(spanmanager.framework, uri)) {
                shared_ptr <L2Ep> l2Ep = L2Ep::resolve(
                        spanmanager.framework, uri).get();
                processL2Ep(l2Ep);
            }
        } else if (classId == Session::CLASS_ID) {
            optional<shared_ptr<Session>> sess =
                    Session::resolve(spanmanager.framework, uri);
            if (sess) {
                LOG(DEBUG) << "update on session " << sess.get()->getURI();
                processSession(sess.get());
                spanmanager.notifyUpdate.insert(uri);
            } else {
                LOG(DEBUG) << "session removed " << uri;
                shared_ptr<SessionState> sessState;
                auto itr = spanmanager.sess_map.find(uri);
                if (itr != spanmanager.sess_map.end()) {
                    spanmanager.notifyDelete.insert(itr->second);
                    spanmanager.sess_map.erase(itr);
                    LOG(DEBUG) << "dst map size " << itr->second->getDstEndPointMap().size();
                }
            }
        }
        // notify all listeners. put it on a task Q for non blocking notification.
        for (URI uri : spanmanager.notifyUpdate) {
            spanmanager.taskQueue.dispatch(uri.toString(), [=]() {
                spanmanager.notifyListeners(uri);
            });
        }
        spanmanager.notifyUpdate.clear();
        for (shared_ptr<SessionState> seSt : spanmanager.notifyDelete) {
            spanmanager.taskQueue.dispatch(seSt->getName(), [=]() {
                spanmanager.notifyListeners(seSt);
            });
        }
        spanmanager.notifyDelete.clear();
    }

    void SpanManager::registerListener(SpanListener *listener) {
        LOG(DEBUG) << "registering listener";
        lock_guard <mutex> guard(listener_mutex);
        spanListeners.push_back(listener);
        if (isDeletePending) {
            notifyListeners();
        }
        isDeletePending = false;
    }

    void SpanManager::unregisterListener(SpanListener* listener) {
        lock_guard <mutex> guard(listener_mutex);
        spanListeners.remove(listener);
    }

    void SpanManager::notifyListeners(const URI& spanURI) {
        LOG(DEBUG) << "notifying update listener";
        lock_guard<mutex> guard(listener_mutex);
        for (SpanListener *listener : spanListeners) {
            listener->spanUpdated(spanURI);
        }
    }

    void SpanManager::notifyListeners(shared_ptr<SessionState> seSt) {
        LOG(DEBUG) << "notifying delete listener";
        lock_guard<mutex> guard(listener_mutex);
        for (SpanListener *listener : spanListeners) {
            listener->spanDeleted(seSt);
        }
    }

    void SpanManager::notifyListeners() {
        LOG(DEBUG) << "notifying delete listener";
        lock_guard<mutex> guard(listener_mutex);
        for (SpanListener* listener : spanListeners) {
            listener->spanDeleted();
        }
    }

    optional<shared_ptr<SessionState>>
        SpanManager::getSessionState(const URI& uri) const {
        unordered_map<URI, shared_ptr<SessionState>>
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
        if (itr != spanmanager.sess_map.end()) {
            spanmanager.sess_map.erase(itr);
        }
        sessState = make_shared<SessionState>(sess->getURI(), sess->getName().get());
        spanmanager.sess_map.insert(make_pair(sess->getURI(), sessState));
        print_map(spanmanager.sess_map);

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
                        if (memRef->getTargetURI()) {
                            URI pUri = memRef->getTargetURI().get();
                            LOG(DEBUG) << pUri.toString();
                            optional<const unsigned char> dir = srcMem->getDir();
                            if (dir) {
                                srcMemInfo sInfo = { .uri = pUri, .dir = dir.get()};
                                processEpGroup(sInfo);
                            }
                        }
                    } else if (class_id == LocalEp::CLASS_ID) {
                        if (memRef->getTargetURI()) {
                            URI pUri = memRef->getTargetURI().get();
                            optional<const unsigned char> dir = srcMem->getDir();
                            if (dir) {
                                srcMemInfo sInfo = { .uri = pUri, .dir = dir.get()};
                                processLocalEp(sInfo);
                            }
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

    void SessionState::addDstEndPoint
                         (const URI& uri, shared_ptr<DstEndPoint> dEp) {
        LOG(DEBUG) << "Add dst IP " << dEp->getAddress();
        dstEndPoints.emplace(uri, dEp);
    }

    void SessionState::addSrcEndPoint
                   (shared_ptr<SourceEndPoint> srcEp) {
        srcEndPoints.emplace(srcEp);
    }

    void SpanManager::SpanUniverseListener::processLocalEp(const srcMemInfo& sInfo) {
        if (LocalEp::resolve(spanmanager.framework, sInfo.uri)) {
            shared_ptr<LocalEp> lEp = LocalEp::resolve(spanmanager.framework, sInfo.uri).get();
            if (lEp->resolveSpanLocalEpToEpRSrc()) {
                shared_ptr <LocalEpToEpRSrc> epRSrc = lEp->resolveSpanLocalEpToEpRSrc().get();
                if (epRSrc.get()->getTargetURI()) {
                    URI epUri = epRSrc.get()->getTargetURI().get();
                    if (L2Ep::resolve(spanmanager.framework, epUri)) {
                        shared_ptr <L2Ep> l2Ep = L2Ep::resolve(
                                spanmanager.framework, epUri).get();
                        addEndPoint(lEp, l2Ep, sInfo);
                    } else {
                        spanmanager.l2EpUri.emplace(epUri, lEp);
                        print_map(spanmanager.l2EpUri);
                    }
                }
            }
        }
    }

    void SpanManager::SpanUniverseListener::processEpGroup(const srcMemInfo& sInfo) {
        LOG(DEBUG) << "Epg uri " << sInfo.uri;
        // get the local end points that are part of this EP group
        std::vector<OF_SHARED_PTR<modelgbp::epr::L2Ep>> out;
        boost::optional<shared_ptr<L2Universe>> l2u = L2Universe::resolve(spanmanager.framework);
        l2u.get()->resolveEprL2Ep(out);
        vector<shared_ptr<L2Ep>> l2EpVec;
        for (auto ep : out) {
            LOG(DEBUG) << "ep " << ep->getURI();
            URI egUri(ep->getGroup().get());
            LOG(DEBUG) << "epg uri " << egUri;
            if (sInfo.uri == egUri) {
                LOG(DEBUG) << "found L2Ep " << ep->getURI();
                l2EpVec.push_back(ep);
            }
        }
        boost::optional<shared_ptr<modelgbp::gbp::EpGroup>> oEpGrp =
                EpGroup::resolve(spanmanager.framework, sInfo.uri);
        if (!oEpGrp) {
            LOG(DEBUG) << "EpGroup " << sInfo.uri << " not found";
            return;
        }
        if (l2EpVec.empty())
            return;
        // get the span sessions associated with this EP group and
        // add L2Ep to the source end point list for each session.
        shared_ptr<modelgbp::gbp::EpGroup> lEpGrp = oEpGrp.get();
        std::vector<OF_SHARED_PTR<modelgbp::gbp::EpGroupToSpanSessionRSrc>> vGrpToSess;
        lEpGrp->resolveGbpEpGroupToSpanSessionRSrc(vGrpToSess);
        for (auto sesRsrc : vGrpToSess) {
            unordered_map<opflex::modb::URI, shared_ptr<SessionState>>::iterator it;
            it = spanmanager.sess_map.find(sesRsrc->getTargetURI().get());
            if (it != spanmanager.sess_map.end()) {
                LOG(DEBUG) << "found session " << sesRsrc.get()->getTargetURI();
                for (auto ep : l2EpVec) {
                    shared_ptr<SourceEndPoint> srcEp =
                                 make_shared<SourceEndPoint>(ep->getURI().toString(),
                                                             ep->getInterfaceName().get());
                    srcEp->setDirection(sInfo.dir);
                    it->second->addSrcEndPoint(srcEp);
                }
                spanmanager.notifyUpdate.insert(sesRsrc->getTargetURI().get());
            }
        }
    }


    const SessionState::srcEpSet& SessionState::getSrcEndPointSet() {
        return srcEndPoints;
    }

    const unordered_map<URI, shared_ptr<DstEndPoint>>&
        SessionState::getDstEndPointMap() const {
        return dstEndPoints;
    }

    void SpanManager::SpanUniverseListener::
        addEndPoint(shared_ptr<LocalEp> lEp, shared_ptr<L2Ep> l2Ep, const srcMemInfo& sInfo) {
        LOG(DEBUG) << "get parent lEp " << (lEp ? "set" : "null") <<
                      " l2Ep " << (l2Ep ? "set" : "null");
        optional<URI> parent = spanmanager.getSession(lEp);
        if (parent) {
            spanmanager.notifyUpdate.insert(parent.get());
            optional<shared_ptr<Session>> sess = Session::resolve(spanmanager.framework,
                    parent.get());
            if (sess) {
                shared_ptr<SessionState> sesSt = spanmanager.sess_map[sess.get()->getURI()];
                shared_ptr<SourceEndPoint> srcEp =
                          make_shared<SourceEndPoint>(lEp->getName().get(),
                                                      l2Ep->getInterfaceName().get());
                srcEp->setDirection(sInfo.dir);
                sesSt->addSrcEndPoint(srcEp);
                spanmanager.notifyUpdate.insert(sess.get()->getURI());
                print_set(sesSt->getSrcEndPointSet());
            }
        }
    }

    /**
     * Find the span session URI by walking back the elements of the LocalEp
     * URI. The span session URI will be the one prior to the element "SpanLocalEp".
     */
    const optional<URI> SpanManager::getSession(shared_ptr<LocalEp> lEp) {
        string uriStr;
        vector<string> elements;
        lEp->getURI().getElements(elements);
        vector<string>::reverse_iterator rit = elements.rbegin();
        for (; rit != elements.rend(); ++rit) {
            if ((*rit).compare("SpanLocalEp") == 0) {
                rit++;
                for (;rit != elements.rend(); ++rit) {
                    string temp("/");
                    temp.append(*rit);
                    uriStr.insert(0, temp);
                }
                uriStr.append("/");
                break;
            }
        }
        if (uriStr.empty()) {
            optional<URI> uri;
            return uri;
        } else {
            LOG(DEBUG) << "uri " << uriStr;
            optional<URI> uri(uriStr);
            return uri;
        }
    }

    optional<shared_ptr<SrcMember>> SpanManager::findSrcMem(shared_ptr<LocalEp> lEp) {

        optional<URI> parent = getSession(lEp);
        optional<shared_ptr<SrcMember>> pSrcMem;
        if (parent)
        {
            optional<shared_ptr<Session>> sess = Session::resolve(framework,
                            parent.get());
            if (sess) {
                vector <shared_ptr<SrcGrp>> srcGrpVec;
                sess.get()->resolveSpanSrcGrp(srcGrpVec);
                for (shared_ptr <SrcGrp> srcGrp : srcGrpVec) {
                    vector<shared_ptr<SrcMember>> srcMemVec;
                    srcGrp.get()->resolveSpanSrcMember(srcMemVec);
                    for (shared_ptr<SrcMember> srcMem : srcMemVec) {
                        optional <shared_ptr<MemberToRefRSrc>> memRefOpt =
                                srcMem.get()->resolveSpanMemberToRefRSrc();
                        if (memRefOpt) {
                            shared_ptr <MemberToRefRSrc> memRef = memRefOpt.get();
                            if (memRef->getTargetURI()) {
                                URI uri = memRef->getTargetURI().get();
                                if (lEp->getURI() == uri) {
                                    pSrcMem = srcMem;
                                }
                            }
                        }
                    }
                 }
            }
        }
        return pSrcMem;
    }

    void SpanManager::SpanUniverseListener::processL2Ep(shared_ptr<L2Ep> l2Ep) {
        auto itr = spanmanager.l2EpUri.find(l2Ep->getURI());
        if (itr == spanmanager.l2EpUri.end()) {
            return;
        } else {

            optional<shared_ptr<SrcMember>> pSmem =
                            spanmanager.findSrcMem(itr->second);
            if (pSmem) {
                optional<const unsigned char> dir = pSmem.get()->getDir();
                if (dir) {
                    URI uri = itr->second->getURI();
                    srcMemInfo sMem { .uri = uri, .dir = dir.get()};
                    addEndPoint(itr->second, l2Ep, sMem);
                    spanmanager.l2EpUri.erase(itr);
                }
            }
        }
    }

    SessionState::SessionState(const URI& uri,
                               const string& name) : uri(uri), name(name) {}

    SourceEndPoint::SourceEndPoint(const string& name_,
            const string& port_) : name(name_), port(port_) {}

}


