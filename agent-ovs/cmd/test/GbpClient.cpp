/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation of GRPC client for configuring opflex policy.
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

/* This must be included before anything else */
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <grpcpp/grpcpp.h>

#include "gbp.grpc.pb.h"
#include "GbpClient.h"
#include <opflexagent/logging.h>
#include <opflex/gbp/Policy.h>

namespace opflexagent {

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using gbpserver::GBP;
using gbpserver::GBPOperation;
using gbpserver::Version;
using gbpserver::GBPObject;
using gbpserver::Property;
using gbpserver::Reference;

using rapidjson::Document;
using rapidjson::Value;
using rapidjson::kObjectType;
using rapidjson::kArrayType;
using rapidjson::StringRef;
using rapidjson::StringBuffer;
using rapidjson::PrettyWriter;

using opflex::gbp::PolicyUpdateOp;

class GbpClientImpl {
public:
    GbpClientImpl(std::shared_ptr<Channel> channel,
                  opflex::test::MockOpflexServer& server) :
        stub_(GBP::NewStub(channel)),
        server_(server) {
        thread_ = std::thread(&GbpClientImpl::ListObjects, this);
    }

    void Wait() { thread_.join(); }

private:
    void JsonDump(Document& d) {
        StringBuffer buffer;
        buffer.Clear();

        PrettyWriter<StringBuffer> writer(buffer);
        d.Accept(writer);

        LOG(DEBUG) << buffer.GetString();
    }

    void JsonDocAdd(Document& d, const GBPObject& gbp) {
        Document::AllocatorType& allocator = d.GetAllocator();
        Value o(kObjectType);
        int i;

        o.AddMember("subject", StringRef(gbp.subject()), allocator);
        o.AddMember("uri", StringRef(gbp.uri()), allocator);

        Value p(kArrayType);
        for (i = 0; i < gbp.properties_size(); i++) {
            const Property& property = gbp.properties(i);
            Value po(kObjectType);

            po.AddMember("name", StringRef(property.name()), allocator);
            switch (property.value_case()) {
            case Property::kStrVal:
                po.AddMember("data", StringRef(property.strval()), allocator);
                break;
            case Property::kIntVal:
                po.AddMember("data", property.intval(), allocator);
                break;
            case Property::kRefVal:
                {
                    Value r(kObjectType);
                    const Reference& reference = property.refval();
                    r.AddMember("subject", StringRef(reference.subject()), allocator);
                    r.AddMember("reference_uri", StringRef(reference.reference_uri()), allocator);
                    po.AddMember("data", r, allocator);
                break;
                }
            default:
                continue;
            }
            p.PushBack(po, allocator);
        }
        if (p.Size()) {
            o.AddMember("properties", p, allocator);
        }

        Value c(kArrayType);
        for (i = 0; i < gbp.children_size(); i++) {
             c.PushBack(StringRef(gbp.children(i)), allocator);
        }
        if (c.Size()) {
            o.AddMember("children", c, allocator);
        }

        if (gbp.parent_subject() != "")
            o.AddMember("parent_subject", StringRef(gbp.parent_subject()), allocator);
        if (gbp.parent_uri() != "")
            o.AddMember("parent_uri", StringRef(gbp.parent_uri()), allocator);
        if (gbp.parent_relation() != "")
            o.AddMember("parent_relation", StringRef(gbp.parent_relation()), allocator);

        d.PushBack(o, allocator);
    }

    void ListObjects() {
        GBPOperation oper;
        ClientContext context;
        Version version;
        Document jsonDoc;

        jsonDoc.SetArray();
        version.set_number(1);
        std::unique_ptr<ClientReader<GBPOperation> > reader(
            stub_->ListObjects(&context, version));
        // The read operation should block until data is available
        while (reader->Read(&oper)) {
            jsonDoc.Clear();
            LOG(DEBUG) << "Operation " << oper.opcode()
                      << " of size " << oper.object_list_size();
            for (int i = 0; i < oper.object_list_size(); i++) {
                const GBPObject& object = oper.object_list(i);
                JsonDocAdd(jsonDoc, object);
            }
            JsonDump(jsonDoc);
            PolicyUpdateOp op;
            switch (oper.opcode()) {
            case GBPOperation::ADD:
                op = PolicyUpdateOp::ADD;
                break;
            case GBPOperation::REPLACE:
                op = PolicyUpdateOp::REPLACE;
                break;
            case GBPOperation::DELETE:
                op = PolicyUpdateOp::DELETE;
                break;
            case GBPOperation::DELETE_RECURSIVE:
                op = PolicyUpdateOp::DELETE_RECURSIVE;
                break;
            default:
                LOG(DEBUG) << "Unknown operation " << oper.opcode();
                continue;
            }
            server_.updatePolicy(jsonDoc, op);
        }
        Status status = reader->Finish();
        if (status.ok()) {
            LOG(INFO) << "ListObjects rpc succeeded.";
        } else {
            LOG(INFO) << "ListObjects rpc failed.";
        }
    }

    std::unique_ptr<GBP::Stub> stub_;
    std::thread thread_;
    opflex::test::MockOpflexServer& server_;
};

GbpClient::GbpClient(const std::string& address,
                     opflex::test::MockOpflexServer& server) : server_(server) {
    thread_ = std::thread(&GbpClient::Start, this, address);
}

GbpClient::~GbpClient() {
    thread_.join();
}

void GbpClient::Start(const std::string& address) {
    while (true) {
        GbpClientImpl client(
            grpc::CreateChannel(address,
                                grpc::InsecureChannelCredentials()),
                                server_);
        client.Wait();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

} /* namespace opflexagent */
