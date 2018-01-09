/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Implementation for NotifServer class
 *
 * Copyright (c) 2015 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include <opflexagent/NotifServer.h>
#include <opflexagent/logging.h>

#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/filesystem.hpp>

#define RAPIDJSON_ASSERT(x)
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/error/en.h>

#include <cstdio>
#include <sstream>
#include <functional>
#include <unistd.h>
#include <sys/types.h>
#include <grp.h>
#include <pwd.h>

namespace opflexagent {

namespace ba = boost::asio;
using ba::local::stream_protocol;
using std::bind;
using std::shared_ptr;
using rapidjson::Document;
using rapidjson::StringBuffer;
using rapidjson::Writer;
using rapidjson::Value;

NotifServer::NotifServer(ba::io_service& io_service_)
    : io_service(io_service_), running(false) {

}

NotifServer::~NotifServer() {
    stop();
}

void NotifServer::setSocketName(const std::string& name) {
    notifSocketPath = name;
}

void NotifServer::setSocketOwner(const std::string& owner) {
    notifSocketOwner = owner;
}

void NotifServer::setSocketGroup(const std::string& group) {
    notifSocketGroup = group;
}

void NotifServer::setSocketPerms(const std::string& perms) {
    notifSocketPerms = perms;
}

class NotifServer::session
    : public std::enable_shared_from_this<session> {
public:
    session(ba::io_service& io_service_, std::set<session_ptr>& sessions_)
        : io_service(io_service_), socket(io_service),
          sessions(sessions_) { }

    stream_protocol::socket& get_socket() {
        return socket;
    }

    void start() {
        LOG(INFO) << "New notification connection";
        sessions.insert(shared_from_this());
        read();
    }

    void close() {
        socket.close();
        sessions.erase(shared_from_this());
    }

    void read() {
        shared_ptr<session> s(shared_from_this());
        ba::async_read(socket,
                       ba::buffer(&msg_len, 4),
                       [s](const boost::system::error_code& ec,
                                 size_t b) {
                           s->handle_size(ec, b);
                       });
    }

    void handle_size(const boost::system::error_code& ec, size_t) {
        if (ec) {
            if (ec != ba::error::operation_aborted) {
                LOG(ERROR) << "Could not read from notif socket: "
                           << ec.message();
                close();
            }
            return;
        }

        msg_len = ntohl(msg_len);
        if (msg_len > 1024) {
            LOG(ERROR) << "Invalid message length: " << msg_len;
            close();
            return;
        }

        buffer.resize(msg_len+1);
        // rapidjson requires null-terminated string for Document::Parse
        buffer[msg_len] = '\0';

        shared_ptr<session> s(shared_from_this());
        ba::async_read(socket,
                       ba::buffer(buffer, msg_len),
                       [s](const boost::system::error_code& ec, size_t b) {
                           s->handle_body(ec, b);
                       });
    }

    void handle_body(const boost::system::error_code& ec, size_t) {
        if (ec) {
            if (ec != ba::error::operation_aborted) {
                LOG(ERROR) << "Could not read from notif socket: "
                           << ec.message();
                close();
            }
            return;
        }

        Document request;
        request.Parse(reinterpret_cast<const char*>(&buffer[0]));
        if (request.HasParseError()) {
            LOG(ERROR) << "Could not parse request at position "
                       << request.GetErrorOffset() << ": "
                       << rapidjson::GetParseError_En(request.GetParseError());
            close();
            return;
        }

        if (request.HasMember("method") && request["method"].IsString() &&
            request.HasMember("params") && request["params"].IsObject()) {
            const Value& m = request["method"];
            const Value& p = request["params"];
            if ("subscribe" == std::string(m.GetString())) {
                if (p.HasMember("type") && p["type"].IsArray()) {
                    const Value& types = p["type"];
                    Value::ConstValueIterator t_it;
                    for (t_it = types.Begin(); t_it != types.End(); ++t_it) {
                        const Value& t = *t_it;
                        if (t.IsString()) {
                            subscriptions.insert(t.GetString());
                            LOG(DEBUG) << "Subscribed to " << t.GetString();
                        }
                    }
                }

                if (request.HasMember("id")) {
                    shared_ptr<StringBuffer> sb(new StringBuffer());
                    Writer<StringBuffer> writer(*sb);
                    writer.StartObject();
                    writer.Key("result");
                    writer.StartObject();
                    writer.EndObject();
                    writer.Key("id");
                    request["id"].Accept(writer);
                    writer.EndObject();

                    write(sb);
                }
            }

            read();
        } else {
            LOG(ERROR) << "Malformed request";
            close();
            return;
        }
    }

    void handle_write(const boost::system::error_code& ec) {
        if (ec) {
            if (ec != ba::error::operation_aborted) {
                LOG(ERROR) << "Could not write to notif socket: "
                           << ec.message();
                close();
            }
            return;
        }
    }

    bool subscribed(const std::string& type) {
        return subscriptions.find(type) != subscriptions.end();
    }

    void dispatch(shared_ptr<StringBuffer> message) {
        shared_ptr<session> s(shared_from_this());
        io_service.dispatch([s, message]() { s->write(message); });
    }

private:
    ba::io_service& io_service;
    stream_protocol::socket socket;
    std::set<session_ptr>& sessions;

    std::unordered_set<std::string> subscriptions;
    uint32_t msg_len;
    std::vector<uint8_t> buffer;

    class write_ctx {
    public:
        write_ctx(shared_ptr<StringBuffer> message_,
                  session_ptr session_)
            : message(message_), session(session_) {}

        void handle_error(const boost::system::error_code& ec) {
            if (ec != ba::error::operation_aborted) {
                LOG(ERROR) << "Could not write to notif socket: "
                           << ec.message();
                session->close();
            }
        }
        void operator()(const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                handle_error(ec);
                return;
            }
        }

        shared_ptr<StringBuffer> message;
        session_ptr session;
    };

    void write(shared_ptr<StringBuffer> message) {
        // write message length to first 4 bytes of message
        *reinterpret_cast<uint32_t*>(const_cast<char*>(message->GetString())) =
            htonl(message->GetSize() - 4);
        ba::async_write(socket, ba::buffer(message->GetString(),
                                           message->GetSize()),
                        write_ctx(message, shared_from_this()));
    }
};

void NotifServer::accept() {
    session_ptr new_session(new session(io_service, sessions));
    acceptor->
        async_accept(new_session->get_socket(),
                     [this, new_session](const boost::system::error_code& ec) {
                         handle_accept(new_session, ec);
                     });
}

void NotifServer::handle_accept(session_ptr new_session,
                                const boost::system::error_code& ec) {
    if (ec) {
        if (ec != ba::error::operation_aborted) {
            LOG(ERROR) << "Could not listen to UNIX socket "
                       << notifSocketPath
                       << ": " << ec.message();
        }
        if (running) {
            new_session.reset();
            accept();
        }
        return;
    } else {
        new_session->start();
    }

    new_session.reset();
    accept();
}

void NotifServer::start() {
    if (notifSocketPath.length() == 0) return;

    running = true;
    std::remove(notifSocketPath.c_str());
    stream_protocol::endpoint ep(notifSocketPath);
    acceptor.reset(new stream_protocol::acceptor(io_service, ep));
    if (boost::filesystem::exists(notifSocketPath)) {
        size_t bufSize = sysconf(_SC_GETPW_R_SIZE_MAX);
        if (bufSize == (size_t)-1)
            bufSize = 16384;
        std::vector<char> buffer(bufSize);

        uid_t uid = 0;
        gid_t gid = 0;
        if (notifSocketOwner != "") {
            struct passwd pwd;
            struct passwd *result;
            getpwnam_r(notifSocketOwner.c_str(), &pwd,
                       &buffer[0], bufSize, &result);
            if (result == NULL) {
                LOG(WARNING) << "Could not find user " << notifSocketOwner;
            } else {
                uid = pwd.pw_uid;
            }
        }
        if (notifSocketGroup != "") {
            struct group gr;
            struct group *result;

            getgrnam_r(notifSocketGroup.c_str(), &gr,
                       &buffer[0], bufSize, &result);
            if (result == NULL) {
                LOG(WARNING) << "Could not find group " << notifSocketGroup;
            } else {
                gid = gr.gr_gid;
            }
        }

        if (notifSocketPerms != "") {
            mode_t perms;
            std::stringstream ss;
            ss << std::oct << notifSocketPerms;
            ss >> perms;

            chmod(notifSocketPath.c_str(), perms);
        }

        if (uid != 0 || gid != 0) {
            if (chown(notifSocketPath.c_str(),
                      uid ? uid : geteuid(),
                      gid ? gid : getegid())) {
                char buf[256];
                if (!strerror_r(errno, buf, sizeof(buf)))
                    LOG(WARNING) << "Could not change ownership for "
                                 << notifSocketPath << ": "
                                 << buf;
            }
        }
    }
    accept();
}

void NotifServer::do_stop() {
    std::set<session_ptr> sess_cpy = sessions;
    for (const session_ptr& sp : sess_cpy) {
        sp->close();
    }
}

void NotifServer::stop() {
    if (!running) return;
    running = false;
    if (acceptor)
        acceptor->close();
    io_service.dispatch(bind(&NotifServer::do_stop, this));
}

static void do_dispatch(shared_ptr<StringBuffer> buffer,
                        const std::set<NotifServer::session_ptr>& sessions,
                        const std::string& type) {
    for (const NotifServer::session_ptr& sp : sessions) {
        if (!sp->subscribed(type)) continue;
        sp->dispatch(buffer);
    }
}

void NotifServer::dispatchVirtualIp(std::unordered_set<std::string> uuids,
                                    const opflex::modb::MAC& macAddr,
                                    const std::string& ipAddr) {
    std::string mstr = macAddr.toString();
    if (!vipLimiter.event(mstr + "|" + ipAddr)) return;

    shared_ptr<StringBuffer> sb(new StringBuffer());
    // leave room to fill in message size later
    sb->Put('\0');
    sb->Put('\0');
    sb->Put('\0');
    sb->Put('\0');
    Writer<StringBuffer> writer(*sb);
    writer.StartObject();
    writer.Key("method");
    writer.String("virtual-ip");
    writer.Key("params");
    writer.StartObject();
    writer.Key("uuid");
    writer.StartArray();
    for (const std::string& uuid : uuids) {
        writer.String(uuid.c_str());
    }
    writer.EndArray();
    writer.Key("mac");
    writer.String(mstr.c_str());
    writer.Key("ip");
    writer.String(ipAddr.c_str());
    writer.EndObject();
    writer.EndObject();

    do_dispatch(sb, sessions, "virtual-ip");
}

} /* namespace opflexagent */
