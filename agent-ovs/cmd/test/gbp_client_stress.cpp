/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation of standalone GRPC server for serving opflex policy.
 * It can be used to serve policy file to mock_server.
 * usage: gbp_client_stress <object-count-per-msg> <wait-internval-between-msgs> <policy-file>
 *
 * Copyright (c) 2019 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <iostream>
#include <string>
#include <cstdio>
#include <sstream>
#include <cstdint>
#include <thread>
#include <chrono>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include "rapidjson/stringbuffer.h"
#include <rapidjson/prettywriter.h>
#include <fstream>

#include <grpcpp/grpcpp.h>
#include "gbp.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using gbpserver::GBP;
using gbpserver::GBPOperation;
using gbpserver::Version;
using gbpserver::GBPObject;
using gbpserver::Property;
using gbpserver::Reference;

using namespace rapidjson;
using namespace std;
using rapidjson::Value;

class GbpServerImpl final : public GBP::Service {
public:
    GbpServerImpl(int objectsInMsg_, int sleepDuration_,
                  const std::string& filename) :
        objectsInMsg(objectsInMsg_), sleepDuration(sleepDuration_) {
        FILE* pfile = fopen(filename.c_str(), "r");
        char buffer[1024];
        rapidjson::FileReadStream f(pfile, buffer, sizeof(buffer));
        doc.ParseStream<0, rapidjson::UTF8<>, rapidjson::FileReadStream>(f);
        JsonDump(doc);
    }

    Status ListObjects(ServerContext* context,
                       const Version* version,
                       ServerWriter<GBPOperation>* writer) override {
        Value::ConstValueIterator moit;
        GBPOperation oper;
        oper.set_opcode(GBPOperation::REPLACE);
        size_t i = 0;
        for (moit = doc.Begin(); moit != doc.End(); ++ moit) {
            const Value& mo = *moit;
            GBPObject* gbp = oper.add_object_list();

            if (mo.HasMember("subject")) {
                const Value& subject = mo["subject"];
                gbp->set_subject(subject.GetString());
            } else {
                std::cerr << "Missing subject, skipping mo" << std::endl;
                continue;
            }

            if (mo.HasMember("uri")) {
                const Value& uri = mo["uri"];
                gbp->set_uri(uri.GetString());
            } else {
                std:cerr << "Missing uri, skipping mo" << std::endl;
                continue;
            }

            if (mo.HasMember("properties")) {
                const Value& properties = mo["properties"];
                if (properties.IsArray()) {
                    for (SizeType i = 0; i < properties.Size(); ++i) {
                        const Value& prop = properties[i];

                        if (prop.HasMember("name")) {

                            Property* property = gbp->add_properties();

                            const Value& name = prop["name"];
                            property->set_name(name.GetString());
                            const Value& data = prop["data"];
                            if (data.IsInt()) {
                                property->set_intval(data.GetInt());
                            } else if (data.IsString()) {
                                property->set_strval(data.GetString());
                            } else {
                                assert(data.IsObject());
                                Reference *reference = property->mutable_refval();
                                const Value& subject = data["subject"];
                                const Value& reference_uri = data["reference_uri"];
                                reference->set_subject(subject.GetString());
                                reference->set_reference_uri(reference_uri.GetString());
                            }
                        }
                    }
                }
            }

            if (mo.HasMember("children")) {
                const Value& children = mo["children"];
                if (children.IsArray()) {
                    for (SizeType i = 0; i < children.Size(); ++i) {
                        const Value& child = children[i];
                        if (child.IsString())
                            gbp->add_children(child.GetString());
                    }
                }
            }

            if (mo.HasMember("parent_subject")) {
                const Value& parent_subject = mo["parent_subject"];
                gbp->set_parent_subject(parent_subject.GetString());
            }
            if (mo.HasMember("parent_uri")) {
                const Value& parent_uri = mo["parent_uri"];
                gbp->set_parent_uri(parent_uri.GetString());
            }
            if (mo.HasMember("parent_relation")) {
                const Value& parent_relation = mo["parent_relation"];
                gbp->set_parent_relation(parent_relation.GetString());
            }

            i += 1;
            if (i % objectsInMsg == 0) {
                std::cout << "read " << i << " objects from file" << std::endl;
                writer->Write(oper);
                oper.clear_object_list();
                std::this_thread::sleep_for(std::chrono::seconds(sleepDuration));
            }
        }
        if (oper.object_list_size())
            writer->Write(oper);

        std::cout << "read " << i << " objects from file" << std::endl;
        return Status::OK;
    }

    void JsonDump(Document& d) {
        StringBuffer buffer;
        buffer.Clear();

        PrettyWriter<StringBuffer> writer(buffer);
        d.Accept(writer);

        std::cout << buffer.GetString() << std::endl;
    }

private:
    Document doc;
    int objectsInMsg;
    int sleepDuration;
};

void Run(int objectsInMsg, int sleepDuration, const std::string& filename) {
    std::string server_address("0.0.0.0:19999");
    GbpServerImpl service(objectsInMsg, sleepDuration, filename);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char **argv) {
    if (argc < 4) {
        std::cerr << "Usage: "  << argv[0]
                  << " <object-count-per-msg> <wait-internval-between-msgs>"
                  << " <policy.json>"
                  << std::endl;
        return 1;
    }
    Run(atoi(argv[1]), atoi(argv[2]), std::string(argv[3]));
    return 0;
}
