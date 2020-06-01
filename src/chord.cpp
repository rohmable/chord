#include "chord.hpp"

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

namespace chord {
    void BlackholeLogger(gpr_log_func_args *args) {}

    void fillNodeInfoMessage(chord::NodeInfoMessage &dst, const chord::NodeInfo &src) {
        dst.set_ip(src.address); dst.set_port(src.port); dst.set_id(src.id);
    }

    void fillNodeInfo(chord::NodeInfo &dst, const chord::NodeInfoMessage &src) {
        dst.address = src.ip(); dst.port = src.port(); dst.id = src.id();
    }

    void copyNodeInfoMessage(chord::NodeInfoMessage &dst, const chord::NodeInfoMessage &src) {
        dst.set_ip(src.ip()); dst.set_port(src.port()); dst.set_id(src.id());
        dst.set_cutoff(src.cutoff());
    }

    void copyNodeInfo(NodeInfo &dst, const NodeInfo &src) {
        dst.address = src.address; dst.port = src.port; dst.id = src.id;
    }

    void fillMessage(mail::Message &dst, const chord::MailboxMessage &src) {
        using google::protobuf::util::TimeUtil;
        using google::protobuf::Timestamp;
        dst.to = src.to(); dst.from = src.from(); dst.subject = src.subject();
        dst.body = src.body(); dst.date = TimeUtil::TimestampToTimeT(TimeUtil::SecondsToTimestamp(src.date()));
    }

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
    for(int i = 0; i < id_length; i+= sizeof(int)) {
        sprintf(buffer, "%d", x[i]);
        hash += buffer;
    }
    delete x;
    delete buffer;
    return std::stoll(hash) % mod;
}

std::unique_ptr<chord::NodeService::Stub> chord::createStub(const chord::NodeInfo &node) {
    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(node.conn_string(), grpc::InsecureChannelCredentials());
    return chord::NodeService::NewStub(channel);
}

std::string chord::NodeInfo::conn_string() const {
    return address + ":" + std::to_string(port);
}

bool chord::operator==(const NodeInfo &lhs, const NodeInfo &rhs) {
    return (lhs.address.compare(rhs.address) == 0) && (lhs.id == rhs.id) && (lhs.port == rhs.port);
}

inline bool chord::between(key_t key, const NodeInfo &lhs, const NodeInfo &rhs) {
    //Inserimento normale, inserimento in giunzione con chiave maggiore dell'ultimo nodo
    //Inserimento in giunzione con chiave minore del primo nodo
    //I nodi in questi casi cadranno sul rhs
    return (key > lhs.id && (key <= rhs.id || lhs.id > rhs.id)) ||
            (key <= rhs.id && key < lhs.id && rhs.id < lhs.id);
}

chord::NodeException::NodeException(const std::string &conn_string) 
    : msg_(std::string("NodeException: Couldn't build server " + conn_string + std::string(". The connection is probably already taken, try to change the listening port"))) {}

const char * chord::NodeException::what() const throw() {
    return msg_.c_str();
}

chord::Node::Node()
    : info_({.address = "", .port = 0})
    , finger_table_(chord::M)
    , values_() {}

chord::Node::Node(const std::string &address, int port) 
    : info_({.address = address, .port = port}) 
    , finger_table_(chord::M)
    , values_()
    , predecessor_({"", 0, -1}) {
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
        throw NodeException(info_.conn_string());
    }
}

void chord::Node::Stop() {
    if (node_thread_) {
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
    } else if(request->sender_id() == info_.id) {
        // Finger request made the entire loop
    } else {
        // Forward the call to the successor
        // TODO: optimize to use the finger table
        auto[result, rep] = sendMessage<FingerQuestion, NodeInfoMessage>(request, finger_table_.front(), &chord::NodeService::Stub::SearchFinger);
        copyNodeInfoMessage(*reply, rep);
    }
    return Status::OK;
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
        copyNodeInfoMessage(*reply, rep);
    } else {
        // Forward the call to the predecessor
        // This method should not be exploited for the normal lookup for performance
        // reasons, however this is not a frequent operation so we can slow down
        // the protocol in order to make it easier
        auto[result, rep] = sendMessage<JoinRequest, NodeInfoMessage>(request, predecessor_, &chord::NodeService::Stub::NodeJoin);
        copyNodeInfoMessage(*reply, rep);
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
    if(isSuccessor(request->key())) {
        fillNodeInfoMessage(*reply, info_);
        mail::MailBox box(request->owner(), request->password());
        // If the box is already present the insert function will return false, checks are not necessary
        boxes_.insert({request->key(), {request->owner(), request->password()}});
    } else {
        if(request->ttl() > 0) {
            InsertMailboxMessage req;
            req.set_key(request->key());
            req.set_owner(request->owner());
            req.set_password(request->password());
            req.set_ttl(request->ttl() - 1);
            auto[result, rep] = sendMessage<InsertMailboxMessage, NodeInfoMessage>(&req, getFingerForKey(request->key()), &chord::NodeService::Stub::InsertMailbox);
            copyNodeInfoMessage(*reply, rep);
        } else {
            fillNodeInfoMessage(*reply, info_);
            reply->set_cutoff(true);
        }
    }
    return Status::OK;
}

grpc::Status chord::Node::Authenticate(grpc::ServerContext *context, const Authentication *request, StatusMessage *reply) {
    key_t key = hashString(request->user());
    try {
        auto box = boxes_.at(key);
        reply->set_result(box.getPassword() == request->psw());
    } catch (std::out_of_range) {
        reply->set_result(false);
    }
    return Status::OK;
}

grpc::Status chord::Node::LookupMailbox(grpc::ServerContext *context, const QueryMailbox *request, QueryResult *reply) {
    NodeInfoMessage *manager = new NodeInfoMessage();
    try {
        reply->set_value(boxes_.at(request->key()).getOwner());
        reply->set_key(request->key());
        fillNodeInfoMessage(*manager, info_);
        reply->set_allocated_manager(manager);
    } catch (std::out_of_range) {
        if(request->ttl() > 0) {
            QueryMailbox req;
            req.set_key(request->key());
            req.set_ttl(request->ttl() - 1);
            auto[result, rep] = sendMessage<QueryMailbox, QueryResult>(&req, getFingerForKey(request->key()), &chord::NodeService::Stub::LookupMailbox);
            reply->set_value(rep.value());
            reply->set_key(rep.key());
            copyNodeInfoMessage(*manager, rep.manager());
            reply->set_allocated_manager(manager);
        } else {
            reply->set_key(-1);
            reply->set_value("");
            manager->set_cutoff(true);
            reply->set_allocated_manager(manager);
        }
    }
    return Status::OK;
}

grpc::Status chord::Node::Send(grpc::ServerContext *context, const MailboxMessage *request, StatusMessage *reply) {
    try {
        key_t key = hashString(request->to());
        mail::MailBox &box = boxes_.at(key);
        if(checkAuthentication(request->auth())) {
            mail::Message msg;
            fillMessage(msg, *request);
            box.insertMessage(msg);
            reply->set_result(true);
        } else {
            reply->set_result(false);
        }
    } catch (std::out_of_range &e) {
        reply->set_result(false);
    }
    return Status::OK;
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
            reply->set_valid(true);
        } else {
            reply->set_valid(false);
        }
    } catch (std::out_of_range) {
        reply->set_valid(false);
    }
    return Status::OK;
}

grpc::Status chord::Node::Transfer(grpc::ServerContext *context, const TransferMailbox *request, StatusMessage *reply) {
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
            reply->set_result(false);
        }
    }
    boxes_.merge(new_boxes);
    reply->set_result(true);
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
            NodeInfo &finger = finger_table_.at(i);
            fillNodeInfo(finger_table_[i], reply);
        }
    }
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
    const NodeInfo finger = finger_table_.at(idx);
    return finger_table_.at(idx);
}

std::pair<chord::key_t, chord::NodeInfo> chord::Node::insertMailbox(const mail::MailBox &box) {
    key_t key = hashString(box.getOwner());
    NodeInfo ret;
    if(isSuccessor(key)) {
        // If the box is already present the insert function will return false, checks are not necessary
        boxes_.insert({key, box});
        copyNodeInfo(ret, info_);
    } else {
        InsertMailboxMessage request;
        request.set_key(key);
        request.set_owner(box.getOwner());
        request.set_password(box.getPassword());
        request.set_ttl(10);
        auto[result, reply] = sendMessage<InsertMailboxMessage, NodeInfoMessage>(&request, info_, &chord::NodeService::Stub::InsertMailbox);
        if(reply.cutoff()) {
            std::cout << "Something went wrong" << std::endl;
        }
        fillNodeInfo(ret, reply);
    }
    return std::pair<chord::key_t, chord::NodeInfo>(key, ret);
}

std::pair<std::string, const chord::NodeInfo> chord::Node::lookupMailbox(const std::string &owner) {
    key_t key = hashString(owner);
    std::string ret_val;
    NodeInfo ret_manager;
    try {
        ret_val = boxes_.at(key).getOwner();
        copyNodeInfo(ret_manager, info_);
    } catch (std::out_of_range) {
        QueryMailbox request;
        request.set_key(key);
        request.set_ttl(10);
        auto[result, reply] = sendMessage<QueryMailbox, QueryResult>(&request, getFingerForKey(key), &chord::NodeService::Stub::LookupMailbox);
        ret_val = reply.value();
        if(reply.manager().cutoff()) {
            throw std::out_of_range("Mailbox not found");
        }
        fillNodeInfo(ret_manager, reply.manager());
    }
    return std::pair<std::string, const chord::NodeInfo>(ret_val, ret_manager);
}

const chord::NodeInfo& chord::Node::getFingerForKey(key_t key) {
    if(between(key, info_, finger_table_.front())) {
        return finger_table_.front();
    }
    for(int i = 0; i < finger_table_.size() - 1; i++) {
        if(between(key, finger_table_.at(i), finger_table_.at(i+1))) {
            return finger_table_.at(i);
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
    TransferMailbox transfer_message;
    std::vector<key_t> to_transfer;
    if(info_.id == 957361667) {
        to_transfer.max_size();
    }

    for(auto &pair : boxes_) {
        if(pair.first <= dest.id) {
            to_transfer.push_back(pair.first);
            chord::Mailbox *new_box = transfer_message.add_boxes();
            mail::MailBox &box = pair.second;
            Authentication *auth = new Authentication;
            auth->set_user(box.getOwner());
            auth->set_psw(box.getPassword());
            new_box->set_allocated_auth(auth);
            new_box->set_valid(true);
            for(auto &msg : box.getMessages()) {
                chord::MailboxMessage *new_msg = new_box->add_messages();
                fillMailboxMessage(*new_msg, msg);
            }
        }
    }
    auto[result, status] = sendMessage<TransferMailbox, StatusMessage>(&transfer_message, dest, &chord::NodeService::Stub::Transfer);
    if(status.result()) {
        for(key_t key : to_transfer) {
            boxes_.erase(key);
        }
    }
    return status.result();
}

bool chord::Node::checkAuthentication(const chord::Authentication &auth) {
    key_t key = hashString(auth.user());
    QueryMailbox query;
    query.set_key(key);
    query.set_ttl(10);
    auto[res, n] = sendMessage<QueryMailbox, QueryResult>(&query, getFingerForKey(key), &chord::NodeService::Stub::LookupMailbox);
    NodeInfo node;
    fillNodeInfo(node, n.manager());
    auto[result, authenticated] = sendMessage<Authentication, StatusMessage>(&auth, node, &chord::NodeService::Stub::Authenticate);
    return authenticated.result();
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
            throw std::invalid_argument("File does not exist");
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
    for(int i = 0; i < ring_.size() - 1; i++) {
        ring_[i]->setSuccessor(ring_[i+1]->getInfo());
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

chord::Node* chord::Ring::getEntryNode() const { return ring_.front(); }

void chord::Ring::dot(const std::string &filename) const {
    std::ofstream dot(filename);
    if(dot.is_open()) {
        dot << "digraph Ring {" << std::endl;
        for(int i = 0; i < ring_.size() - 1; i++) {
            dot << '\t' << ring_[i]->getInfo().id << " -> " << ring_[i]->getSuccessor().id << ';' << std::endl;
        }
        dot << '\t' << ring_.back()->getInfo().id << " -> " << ring_.back()->getSuccessor().id << ';' << std::endl;
        dot << '}';
        dot.close();
    }
}

void chord::Ring::print(std::ostream &os) const {
    for(auto &error : errors_) {
        os << error << std::endl;
    }
    for(auto node : ring_) {
        if(node->isRunning()) {
            os << node->getInfo().conn_string();
            os << " id: " << std::setw(20) << std::left << node->getInfo().id;
            os << "managing " << node->numMailbox() << " mailboxes" << std::endl;
        }
    }
}

std::ostream & chord::operator<<(std::ostream &os, const Ring &ring) {
    ring.print(os);
    return os;
}