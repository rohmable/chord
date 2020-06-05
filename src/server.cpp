#include "server.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <gcrypt.h>
#include <cmath>
#include <chrono>
#include <utility>
#include <iomanip>
#include <cereal/archives/binary.hpp>
#include <cereal/types/map.hpp>
#include <google/protobuf/util/time_util.h>

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using grpc::StatusCode;

namespace chord {
    /**
     * Logger function used to avoid annoying prints by the gRPC library.
     * 
     * The logging messages won't be printed on the console.
    */
    void BlackholeLogger(gpr_log_func_args *args) {}

    /**
     * Fills a mail::Message from a chord::MailboxMessage
     * 
     * @param dst mail::Message destination reference
     * @param src chord::MailboxMessage source reference
    */
    void fillMessage(mail::Message &dst, const chord::MailboxMessage &src) {
        using google::protobuf::util::TimeUtil;
        using google::protobuf::Timestamp;
        dst.to = src.to(); dst.from = src.from(); dst.subject = src.subject();
        dst.body = src.body(); dst.date = TimeUtil::TimestampToTimeT(TimeUtil::SecondsToTimestamp(src.date()));
    }

    /**
     * Fills a chord::MailboxMessage from a mail::Message.
     * 
     * @param dst chord::MailboxMessage destination reference
     * @param src mail::Message source reference
    */
    void fillMailboxMessage(chord::MailboxMessage &dst, const mail::Message &src) {
        using google::protobuf::util::TimeUtil;
        using google::protobuf::Timestamp;
        dst.set_to(src.to); dst.set_from(src.from); dst.set_subject(src.subject);
        dst.set_body(src.body);
        dst.set_date(TimeUtil::TimestampToSeconds(TimeUtil::TimeTToTimestamp(src.date)));
    }
}

chord::key_t chord::hashString(const std::string &str) {
    unsigned int id_length = gcry_md_get_algo_dlen(GCRY_MD_SHA1);
    unsigned char *x = new unsigned char[id_length];
    gcry_md_hash_buffer(GCRY_MD_SHA1, x, str.c_str(), str.size());
    long long int mod = std::pow(2, M);
    char *buffer = new char[id_length];
    std::string hash;
    for(int i = 0; i < static_cast<int>(id_length); i += sizeof(int)) {
        sprintf(buffer, "%d", x[i]);
        hash += buffer;
    }
    delete x;
    delete buffer;
    return std::stoll(hash) % mod;
}

chord::Node::Node()
    : info_({.address = "", .port = 0})
    , predecessor_({"", 0, -1})
    , disable_transfer_(false)
    , finger_table_(chord::M) {}

chord::Node::Node(const std::string &address, int port) 
    : info_({.address = address, .port = port})
    , predecessor_({"", 0, -1})
    , disable_transfer_(false)
    , finger_table_(chord::M) {
    info_.id = hashString(info_.conn_string());
    Run();
}

chord::Node::~Node() {
    Stop();
}

bool chord::Node::isRunning() const { return node_thread_ != nullptr; }

void chord::Node::Run() {
    std::stringstream filename;
    filename << info_.id << ".dat";
    std::ifstream is(filename.str());
    if(is.is_open()) {
        cereal::BinaryInputArchive archive(is);
        archive(boxes_);
    }
    ServerBuilder builder;
    builder.AddListeningPort(info_.conn_string(), grpc::InsecureServerCredentials());
    builder.RegisterService(this);
    server_ = builder.BuildAndStart();
    if (server_ != nullptr) {
        node_thread_.reset(new std::thread(&Server::Wait, server_.get()));
        run_stabilize_ = true;
        stabilize_thread_.reset(new std::thread(&Node::stabilize, this));
    } else {
        throw NodeException(std::string("Couldn't build node ") + info_.conn_string());
    }
}

void chord::Node::Stop() {
    if (node_thread_) {
        disable_transfer_ = true;
        if(!transferBoxes(finger_table_.front())) {
            std::cerr << info_.id << " couldn't transfer mail, trying to dump boxes to file...";
            std::flush(std::cerr);
            if(dumpBoxes()) {
                std::cerr << " Done." << std::endl;
            } else {
                std::cerr << " FAILED: DATA WILL BE LOST" << std::endl;
            }
        }
        run_stabilize_ = false;
        stabilize_thread_->join();
        server_->Shutdown();
        server_.release();
        node_thread_->join();
        node_thread_.release();
        stabilize_thread_.release();
        disable_transfer_ = true;
    }
}

void chord::Node::join(const NodeInfo &entry_point) {
    JoinRequest request;
    request.set_node_id(info_.id);
    auto[result, reply] = sendMessage<JoinRequest, NodeInfoMessage>(&request, entry_point, &chord::NodeService::Stub::NodeJoin);
    if(result.ok()) {
        NodeInfo successor_info = {.address = reply.ip(), .port = reply.port(), .id = reply.id()};
        setSuccessor(successor_info);
    }
} 

grpc::Status chord::Node::Ping(grpc::ServerContext *context, const PingRequest *request, PingReply *reply) {
    reply->set_ping_ip(info_.address);
    reply->set_ping_port(info_.port);
    reply->set_ping_id(info_.id);
    reply->set_ping_n(request->ping_n());
    return Status::OK;
}

grpc::Status chord::Node::SearchFinger(grpc::ServerContext *context, const FingerQuestion *request, NodeInfoMessage *reply) {
    if(info_.id >= request->finger_value() || (info_.id < request->sender_id() && info_.id < request->finger_value())) {
        // This node is the right finger
        fillNodeInfoMessage(*reply, info_);
        return Status::OK;
    } else if(request->sender_id() == info_.id) {
        // Finger request made the entire loop
        return Status(StatusCode::NOT_FOUND, "The request made the entire loop");
    } else {
        // Forward the call to the successor
        auto[result, rep] = sendMessage<FingerQuestion, NodeInfoMessage>(request, finger_table_.front(), &chord::NodeService::Stub::SearchFinger);
        reply->CopyFrom(rep);
        return result;
    }
}

grpc::Status chord::Node::NodeJoin(grpc::ServerContext *context, const JoinRequest *request, NodeInfoMessage *reply) {
    if(info_.id > request->node_id() && (predecessor_.id < request->node_id() || predecessor_.id > info_.id) ) {
        // The joining node is smaller than me and either my predecessor is smaller than the joining node
        // or I have the smallest id of the ring
        fillNodeInfoMessage(*reply, info_);
    } else if (info_.id < request->node_id()) {
        // The joining node has a bigger id than mine so I forward the call
        // to the appropriate finger
        auto[result, rep] = sendMessage<JoinRequest, NodeInfoMessage>(request, getFingerForKey(request->node_id()), &chord::NodeService::Stub::NodeJoin);
        reply->CopyFrom(rep);
    } else {
        // Forward the call to the predecessor
        // This method should not be exploited for the normal lookup for performance
        // reasons, however this is not a frequent operation so we can slow down
        // the protocol in order to make it easier
        auto[result, rep] = sendMessage<JoinRequest, NodeInfoMessage>(request, predecessor_, &chord::NodeService::Stub::NodeJoin);
        reply->CopyFrom(rep);
    }
    return Status::OK;
}

grpc::Status chord::Node::Stabilize(grpc::ServerContext *context, const NodeInfoMessage *request, NodeInfoMessage *reply) {
    if(request->id() > predecessor_.id) {
        fillNodeInfo(predecessor_, *request);
    }
    fillNodeInfoMessage(*reply, predecessor_);
    return Status::OK;
}

grpc::Status chord::Node::InsertMailbox(grpc::ServerContext *context, const InsertMailboxMessage * request, NodeInfoMessage *reply) {
    key_t key = hashString(request->owner());
    if(isSuccessor(key)) {
        fillNodeInfoMessage(*reply, info_);
        mail::MailBox box(request->owner(), request->password());
        // If the box is already present the insert function will return false, checks are not necessary
        auto[it, success] = boxes_.insert({key, {request->owner(), request->password()}});
        if(success) {
            return Status::OK;
        } else {
            return Status(StatusCode::ALREADY_EXISTS, "User already registered");
        }
    } else {
        if(request->ttl() > 0) {
            InsertMailboxMessage req;
            req.set_owner(request->owner());
            req.set_password(request->password());
            req.set_ttl(request->ttl() - 1);
            auto[result, rep] = sendMessage<InsertMailboxMessage, NodeInfoMessage>(&req, getFingerForKey(key), &chord::NodeService::Stub::InsertMailbox);
            reply->CopyFrom(rep);
            return result;
        } else {
            fillNodeInfoMessage(*reply, info_);
            return Status(StatusCode::NOT_FOUND, "Couldn't find the correct node");
        }
    }
}

grpc::Status chord::Node::Authenticate(grpc::ServerContext *context, const Authentication *request, Empty *reply) {
    key_t key = hashString(request->user());
    try {
        auto &box = boxes_.at(key);
        return box.getPassword() == request->psw() ?
            Status::OK : 
            Status(StatusCode::UNAUTHENTICATED, "Authentication failed");
    } catch (std::out_of_range &e) {
        return Status(StatusCode::UNAUTHENTICATED, "Couldn't find the mailbox");
    }
}

grpc::Status chord::Node::LookupMailbox(grpc::ServerContext *context, const QueryMailbox *request, NodeInfoMessage *reply) {
    key_t key = hashString(request->owner());
    try {
        boxes_.at(key);
        fillNodeInfoMessage(*reply, info_);
        return Status::OK;
    } catch (std::out_of_range &e) {
        if(request->ttl() > 0) {
            QueryMailbox req;
            req.set_owner(request->owner());
            req.set_ttl(request->ttl() - 1);
            auto[result, rep] = sendMessage<QueryMailbox, NodeInfoMessage>(&req, getFingerForKey(key), &chord::NodeService::Stub::LookupMailbox);
            reply->CopyFrom(rep);
            return result;
        } else {
            return Status(StatusCode::NOT_FOUND, "Couldn't find the mailbox");
        }
    }
}

grpc::Status chord::Node::Send(grpc::ServerContext *context, const MailboxMessage *request, Empty *reply) {
    if(request->from().compare(request->auth().user()) != 0) {
        return Status(StatusCode::UNAUTHENTICATED, "Authentication doesn't match sender");
    }
    key_t key = hashString(request->to());
    try {
        mail::MailBox &box = boxes_.at(key);
        if(checkAuthentication(request->auth())) {
            mail::Message msg;
            fillMessage(msg, *request);
            box.insertMessage(msg);
            return Status::OK;
        } else {
            return Status(StatusCode::UNAUTHENTICATED, "Authentication failed");
        }
    } catch (std::out_of_range &e) {
        if(request->ttl() > 0) {
            MailboxMessage req(*request);
            req.set_ttl(req.ttl() - 1);
            auto[rep, _] = sendMessage<MailboxMessage, Empty>(&req, getFingerForKey(key), &NodeService::Stub::Send);
            return rep;
        } else {
            return Status(StatusCode::NOT_FOUND, "Couldn't find the mailbox");
        }
    }
}

grpc::Status chord::Node::Delete(grpc::ServerContext *context, const DeleteMessage *request, Empty *reply) {
    key_t key = hashString(request->auth().user());
    try {
        mail::MailBox &box = boxes_.at(key);
        if(checkAuthentication(request->auth())) {
            return box.removeMessage(request->idx()) ? Status::OK : Status(StatusCode::OUT_OF_RANGE, "Index out of range");
        } else {
            return Status(StatusCode::UNAUTHENTICATED, "Authentication failed");
        }
    } catch (std::out_of_range &e) {
        if(request->ttl() > 0) {
            DeleteMessage req(*request);
            req.set_ttl(req.ttl() - 1);
            auto[rep, _] = sendMessage<DeleteMessage, Empty>(&req, getFingerForKey(key), &NodeService::Stub::Delete);
            return rep;
        } else {
            return Status(StatusCode::NOT_FOUND, "Couldn't find the mailbox");
        }
    }
}

grpc::Status chord::Node::Receive(grpc::ServerContext *context, const Authentication *request, Mailbox *reply) {
    try {
        key_t key = hashString(request->user());
        mail::MailBox &box = boxes_.at(key);
        if(box.getPassword() == request->psw()) {
            Authentication *auth = new Authentication();
            auth->set_user(box.getOwner());
            auth->set_psw(box.getPassword());
            reply->set_allocated_auth(auth);
            for(auto msg : box.getMessages()) {
                MailboxMessage *message = reply->add_messages();
                fillMailboxMessage(*message, msg);
            }
            return Status::OK;
        } else {
            return Status(StatusCode::UNAUTHENTICATED, "Authentication failed");
        }
    } catch (std::out_of_range &e) {
        return Status(StatusCode::NOT_FOUND, "Couldn't find the mailbox");
    }
}

grpc::Status chord::Node::Transfer(grpc::ServerContext *context, const TransferMailbox *request, Empty *reply) {
    if(disable_transfer_) {
        return Status(StatusCode::UNAVAILABLE, "Transfer is disabled");
    }
    std::map<chord::key_t, mail::MailBox> new_boxes;
    for(auto &box : request->boxes()) {
        key_t key = hashString(box.auth().user());
        auto[b, success] = new_boxes.insert({key, {box.auth().user(), box.auth().psw()}});
        if(success) {
            for(auto &msg : box.messages()) {
                mail::Message message;
                fillMessage(message, msg);
                b->second.insertMessage(message);
            }
        } else {
            return Status(StatusCode::INTERNAL, "Something went wrong when transfering mailboxes");
        }
    }
    boxes_.merge(new_boxes);
    return Status::OK;
}

void chord::Node::buildFingerTable() {
    const NodeInfo &successor = finger_table_.front();
    key_t mod = std::pow(2, chord::M);
    for(int i = 1; i < M; i++) {
        key_t finger_val = static_cast<key_t>(info_.id + std::pow(2, i)) % mod;
        FingerQuestion request;
        request.set_sender_id(info_.id);
        request.set_finger_value(finger_val);
        auto[result, reply] = sendMessage<FingerQuestion, NodeInfoMessage>(&request, successor, &chord::NodeService::Stub::SearchFinger);
        if(result.ok()) {
            fillNodeInfo(finger_table_[i], reply);
        } else {
            std::cout << "No finger found" << std::endl;
        }
    }
}

void chord::Node::setInfo(const NodeInfo &info) {
    info_ = NodeInfo(info);
    info_.id = hashString(info_.conn_string());
}

const chord::NodeInfo& chord::Node::getInfo() const { return info_; }

int chord::Node::numMailbox() const { return boxes_.size(); }

void chord::Node::setSuccessor(const NodeInfo &successor) {
    NodeInfo &successor_ = finger_table_.front();
    successor_.address = successor.address;
    successor_.port = successor.port;
    successor_.id = successor.id;
    NodeInfoMessage notification;
    fillNodeInfoMessage(notification, info_);
    sendMessage<NodeInfoMessage, NodeInfoMessage>(&notification, successor_, &chord::NodeService::Stub::Stabilize);
}

const chord::NodeInfo& chord::Node::getSuccessor() const {
    return finger_table_.front();
}

const chord::NodeInfo& chord::Node::getPredecessor() const {
    return predecessor_;
}

const chord::NodeInfo& chord::Node::getFinger(int idx) const {
    return finger_table_.at(idx);
}

const chord::NodeInfo& chord::Node::getFingerForKey(key_t key) {
    if(between(key, info_, finger_table_.front())) {
        return finger_table_.front();
    }
    for(auto finger = finger_table_.begin(); finger != std::prev(finger_table_.end(), 1); finger++) {
        if(between(key, *finger, *std::next(finger, 1))) {
            return *finger;
        }
    }
    return finger_table_.back();
}

bool chord::Node::isSuccessor(key_t key) {
    return between(key, predecessor_, info_);
}

bool chord::Node::transferBoxes(const chord::NodeInfo &dest) {
    if(boxes_.empty()) {
        return true;
    }
    chord::PingRequest ping;
    ping.set_ping_n(1);
    auto[result, status] = sendMessage<PingRequest, PingReply>(&ping, dest, &chord::NodeService::Stub::Ping);
    if(!result.ok() || status.ping_n() != ping.ping_n()) {
        return false;
    }

    TransferMailbox transfer_message;
    std::vector<key_t> to_transfer;

    for(auto &pair : boxes_) {
        if(pair.first <= dest.id) {
            to_transfer.push_back(pair.first);
            chord::Mailbox *new_box = transfer_message.add_boxes();
            mail::MailBox &box = pair.second;
            Authentication *auth = new Authentication;
            auth->set_user(box.getOwner());
            auth->set_psw(box.getPassword());
            new_box->set_allocated_auth(auth);
            for(auto &msg : box.getMessages()) {
                chord::MailboxMessage *new_msg = new_box->add_messages();
                fillMailboxMessage(*new_msg, msg);
            }
        }
    }
    if(!to_transfer.empty()) {
        auto[result, _] = sendMessage<TransferMailbox, Empty>(&transfer_message, dest, &chord::NodeService::Stub::Transfer);
        if(result.ok()) {
            for(key_t key : to_transfer) {
                boxes_.erase(key);
            }
        }
        return result.ok();
    } else {
        return true;
    }
}

bool chord::Node::checkAuthentication(const chord::Authentication &auth) {
    key_t key = hashString(auth.user());
    QueryMailbox query;
    query.set_owner(auth.user());
    query.set_ttl(CHORD_MOD);
    auto[res, n] = sendMessage<QueryMailbox, NodeInfoMessage>(&query, getFingerForKey(key), &chord::NodeService::Stub::LookupMailbox);
    NodeInfo node;
    fillNodeInfo(node, n);
    auto[result, _] = sendMessage<Authentication, Empty>(&auth, node, &chord::NodeService::Stub::Authenticate);
    return result.ok();
}

void chord::Node::stabilize() {
    NodeInfoMessage request;
    fillNodeInfoMessage(request, info_);
    while(run_stabilize_) {
        auto[result, reply] = sendMessage<NodeInfoMessage, NodeInfoMessage>(&request, finger_table_.front(), &chord::NodeService::Stub::Stabilize);
        if(reply.id() > info_.id) {
            fillNodeInfo(finger_table_.front(), reply);
            buildFingerTable();
        }
        if(info_.id > predecessor_.id) {
            transferBoxes(predecessor_);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

bool chord::Node::dumpBoxes() {
    std::stringstream filename;
    filename << info_.id << ".dat";
    std::ofstream os(filename.str());
    if(!os) {
        return false;
    }
    cereal::BinaryOutputArchive archive(os);
    archive(boxes_);
    return true;
}

chord::Ring::Ring() {}

chord::Ring::Ring(const std::string &json_file) {
    gpr_set_log_function(BlackholeLogger);

    std::vector<NodeInfo> nodes;
    {
        std::ifstream is(json_file);
        if(!is.is_open()) {
            throw NodeException("File does not exist");
        }
        cereal::JSONInputArchive archive(is);
        archive(cereal::make_nvp("entities", nodes));
    }

    for(auto &node : nodes) {
        try {
            chord::Node *new_node = new chord::Node(node.address, node.port);
            ring_.push_back(new_node);
        } catch (chord::NodeException &e) {
            std::cout << e.what() << std::endl;
            errors_.emplace_back(e.what());
        }
    }

    std::sort(ring_.begin(), ring_.end(), [](const chord::Node *lhs, const chord::Node *rhs) {
        return lhs->getInfo().id < rhs->getInfo().id;
    });
    for(auto node = ring_.begin(); node != std::prev(ring_.end(), 1); node++) {
        (*node)->setSuccessor((*std::next(node, 1))->getInfo());
    }
    ring_.back()->setSuccessor(ring_.front()->getInfo());
    for(auto node : ring_) {
        node->buildFingerTable();
    }
}

chord::Ring::~Ring() {
    for(auto node : ring_) {
        delete node;
    }
}

void chord::Ring::push_back(Node *node) {
    ring_.push_back(node);
    std::sort(ring_.begin(), ring_.end(), [](const chord::Node *lhs, const chord::Node *rhs) {
        return lhs->getInfo().id < rhs->getInfo().id;
    });
}

void chord::Ring::emplace_back(const std::string &address, int port) {
    ring_.push_back(new chord::Node(address, port));
    std::sort(ring_.begin(), ring_.end(), [](const chord::Node *lhs, const chord::Node *rhs) {
        return lhs->getInfo().id < rhs->getInfo().id;
    });
}

const std::vector<chord::Node *>& chord::Ring::getNodes() const { return ring_; }

const std::vector<std::string>& chord::Ring::getErrors() const { return errors_; }

chord::Node* chord::Ring::getEntryNode() const { return ring_.front(); }

void chord::Ring::dot(const std::string &filename) const {
    std::ofstream dot(filename);
    if(dot.is_open()) {
        dot << "digraph Ring {" << std::endl;
        for(auto it = ring_.begin(); it != std::prev(ring_.end()); it++) {
            dot << '\t' << (*it)->getInfo().id << " -> " << (*it)->getSuccessor().id << ';' << std::endl;
        }
        dot << '\t' << ring_.back()->getInfo().id << " -> " << ring_.back()->getSuccessor().id << ';' << std::endl;
        dot << '}';
        dot.close();
    }
}