#include "client.hpp"

chord::Client::Client(const std::string &conn_string)
    : stub_(NodeService::NewStub(grpc::CreateChannel(conn_string, grpc::InsecureChannelCredentials())))
    , box_(nullptr) {
    
    if(!ping()) {
        throw chord::NodeException("The node is not online");
    }
}

bool chord::Client::connectTo(const NodeInfo &node) {
    return connectTo(node.conn_string());
}

bool chord::Client::connectTo(const std::string &conn_string) {
    stub_.release();
    stub_ = NodeService::NewStub(grpc::CreateChannel(conn_string, grpc::InsecureChannelCredentials()));
    return true;
}

mail::MailBox& chord::Client::getBox() { 
    if(box_) return *box_;
    else throw chord::NodeException("You must login first");
}

bool chord::Client::ping(int p) {
    PingRequest request;
    request.set_ping_n(p);
    auto[result, reply] = sendMessage<PingRequest, PingReply>(&request, &NodeService::Stub::Ping);
    return result.ok() && static_cast<int>(reply.ping_n()) == p;
}

chord::NodeInfo chord::Client::auth(const mail::MailBox &box, bool login) {
    NodeInfo manager;

    if(login) {
        QueryMailbox request;
        request.set_owner(box.getOwner());
        request.set_ttl(CHORD_MOD);
        auto[result, reply] = sendMessage<QueryMailbox, NodeInfoMessage>(&request, &NodeService::Stub::LookupMailbox);
        if(result.ok()) {
            fillNodeInfo(manager, reply);
        } else {
            throw NodeException("Address not found");
        }
    } else {
        InsertMailboxMessage request;
        request.set_owner(box.getOwner());
        request.set_password(box.getPassword());
        request.set_ttl(CHORD_MOD);
        auto[result, reply] = sendMessage<InsertMailboxMessage, NodeInfoMessage>(&request, &NodeService::Stub::InsertMailbox);
        if(!result.ok()) {
            throw NodeException(result.error_message());
        } else if(result.error_code() == grpc::StatusCode::NOT_FOUND) {
            throw NodeException("Address not found");
        } else {
            fillNodeInfo(manager, reply);
        }
    }
    connectTo(manager);

    Authentication authentication;
    authentication.set_user(box.getOwner());
    authentication.set_psw(box.getPassword());
    auto[result, _] = sendMessage<Authentication, Empty>(&authentication, &NodeService::Stub::Authenticate);
    if(result.ok()) {
        box_.reset(new mail::MailBox(box));
        return manager;
    } else {
        throw NodeException("Invalid password");
    }
}

bool chord::Client::getMessages() {
    if(!box_) return false;
    Authentication request;
    request.set_user(box_->getOwner());
    request.set_psw(box_->getPassword());
    auto[status, mailbox] = sendMessage<Authentication, Mailbox>(&request, &NodeService::Stub::Receive);
    if(!status.ok()) return false;
    box_->clear();
    for(int i = 0; i < mailbox.messages_size(); i++) {
        const MailboxMessage &msg = mailbox.messages().at(i);
        box_->insertMessage(msg.to(), msg.from(), msg.subject(), msg.body(), secondsToTimeT(msg.date()));
    }
    return true;
}

void chord::Client::send(const mail::Message &message) {
    if(!box_) return;
    chord::MailboxMessage msg;
    fillMailboxMessage(msg, message);
    auto[status, _] = sendMessage<MailboxMessage, Empty>(&msg, &NodeService::Stub::Send);
    if(!status.ok()) {
        throw NodeException(status.error_message());
    }
}

void chord::Client::remove(int idx) {
    if(!box_) return;
    chord::DeleteMessage msg;
    fillDeleteMessage(msg, idx);
    auto[status, _] = sendMessage<DeleteMessage, Empty>(&msg, &NodeService::Stub::Delete);
    if(!status.ok()) {
        throw NodeException(status.error_message());
    }
}

time_t chord::Client::secondsToTimeT(google::protobuf::int64 secs) {
    using google::protobuf::util::TimeUtil;
    return TimeUtil::TimestampToTimeT(TimeUtil::SecondsToTimestamp(secs));
}

google::protobuf::int64 chord::Client::timeTToSeconds(time_t time) {
    using google::protobuf::util::TimeUtil;
    return TimeUtil::TimestampToSeconds(TimeUtil::TimeTToTimestamp(time));
}

void chord::Client::fillMailboxMessage(MailboxMessage &dst, const mail::Message &src) {
    Authentication *auth = new Authentication;
    auth->set_user(box_->getOwner());
    auth->set_psw(box_->getPassword());
    dst.set_allocated_auth(auth);
    dst.set_to(src.to); dst.set_from(src.from); dst.set_subject(src.subject);
    dst.set_body(src.body); dst.set_date(timeTToSeconds(src.date));
    dst.set_ttl(CHORD_MOD);
}

void chord::Client::fillDeleteMessage(DeleteMessage &dst, int idx) {
    Authentication *auth = new Authentication;
    auth->set_user(box_->getOwner());
    auth->set_psw(box_->getPassword());
    dst.set_allocated_auth(auth);
    dst.set_idx(idx);
    dst.set_ttl(CHORD_MOD);
}
