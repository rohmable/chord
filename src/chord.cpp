#include "chord.hpp"

#include <iostream>
#include <gcrypt.h>
#include <cmath>
#include <chrono>

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;

namespace chord {
    key_t hashString(const std::string &str) {
        unsigned int id_length = gcry_md_get_algo_dlen(GCRY_MD_SHA1);
        unsigned char *x = new unsigned char[id_length];
        gcry_md_hash_buffer(GCRY_MD_SHA1, x, str.c_str(), str.size());
        long long int mod = std::pow(2, M);
        char *buffer = new char[id_length];
        std::string hash;
        for(int i = 0; i < M/8; i++) {
            sprintf(buffer, "%d", x[i]);
            hash += buffer;
        }
        delete x;
        delete buffer;
        return std::stoll(hash) % mod;
    }

    void fillNodeInfoMessage(chord::NodeInfoMessage &dst, const chord::NodeInfo &src) {
        dst.set_ip(src.address); dst.set_port(src.port); dst.set_id(src.id);
    }

    void fillNodeInfo(chord::NodeInfo &dst, const chord::NodeInfoMessage &src) {
        dst.address = src.ip(); dst.port = src.port(); dst.id = src.id();
    }

    void copyNodeInfoMessage(chord::NodeInfoMessage &dst, const chord::NodeInfoMessage &src) {
        dst.set_ip(src.ip()); dst.set_port(src.port()); dst.set_id(src.id());
    }

    void copyNodeInfo(NodeInfo &dst, const NodeInfo &src) {
        dst.address = src.address; dst.port = src.port; dst.id = src.id;
    }
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
    return (key > lhs.id && (key <= rhs.id || lhs.id > rhs.id));
}

chord::Node::Node()
    : info_({.address = "", .port = 0})
    , finger_table_(chord::M)
    , values_() {}

chord::Node::Node(std::string address, int port) 
    : info_({.address = address, .port = port}) 
    , finger_table_(chord::M)
    , values_() {
    info_.id = hashString(info_.conn_string());
    Run();
}

chord::Node::~Node() {
    Stop();
}

void chord::Node::Run() {
    ServerBuilder builder;
    builder.AddListeningPort(info_.conn_string(), grpc::InsecureServerCredentials());
    builder.RegisterService(this);
    server_ = builder.BuildAndStart();
    node_thread_.reset(new std::thread(&Server::Wait, server_.get()));
    run_stabilize_ = true;
    stabilize_thread_.reset(new std::thread(&Node::stabilize, this));
}

void chord::Node::Stop() {
    if (node_thread_) {
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

grpc::Status chord::Node::Insert(grpc::ServerContext *context, const InsertMessage *request, NodeInfoMessage *reply) {
    if(isSuccessor(request->key())) {
        fillNodeInfoMessage(*reply, info_);
        values_.insert({request->key(), request->value()});
    } else {
        auto[result, rep] = sendMessage<InsertMessage, NodeInfoMessage>(request, getFingerForKey(request->key()), &chord::NodeService::Stub::Insert);
        copyNodeInfoMessage(*reply, rep);
    }
    return Status::OK;
}

grpc::Status chord::Node::Lookup(grpc::ServerContext *context, const Query *request, QueryResult *reply) {
    NodeInfoMessage *manager = new NodeInfoMessage();
    try {
        reply->set_value(values_.at(request->key()));
        reply->set_key(request->key());
        fillNodeInfoMessage(*manager, info_);
        reply->set_allocated_manager(manager);
    } catch (std::out_of_range) {
        auto[result, rep] = sendMessage<Query, QueryResult>(request, getFingerForKey(request->key()), &chord::NodeService::Stub::Lookup);
        reply->set_value(rep.value());
        reply->set_key(rep.key());
        copyNodeInfoMessage(*manager, rep.manager());
        reply->set_allocated_manager(manager);
    }
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

const chord::NodeInfo& chord::Node::getInfo() const {
    return info_;
}

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

std::pair<chord::key_t, chord::NodeInfo> chord::Node::insert(const std::string &value) {
    key_t key = hashString(value);
    NodeInfo ret;
    if(isSuccessor(key)) {
        values_.insert({key, value});
        copyNodeInfo(ret, info_);
    } else {
        InsertMessage request;
        request.set_key(key);
        request.set_value(value);
        auto[result, reply] = sendMessage<InsertMessage, NodeInfoMessage>(&request, info_, &chord::NodeService::Stub::Insert);
        fillNodeInfo(ret, reply);
    }
    return std::pair<key_t, NodeInfo>(key, ret);
}

std::pair<std::string, const chord::NodeInfo> chord::Node::lookup(const std::string &value) {
    key_t key = hashString(value);
    std::string ret_val;
    NodeInfo ret_manager;
    try {
        ret_val = values_.at(key);
        copyNodeInfo(ret_manager, info_);
    } catch (std::out_of_range) {
        Query query;
        query.set_key(key);
        auto[result, reply] = sendMessage<Query, QueryResult>(&query, getFingerForKey(key), &chord::NodeService::Stub::Lookup);
        ret_val = reply.value();
        fillNodeInfo(ret_manager, reply.manager());
        
    }
    return std::pair<std::string, const NodeInfo>(ret_val, ret_manager);
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

void chord::Node::stabilize() {
    NodeInfoMessage request;
    fillNodeInfoMessage(request, info_);
    while(run_stabilize_) {
        auto[result, reply] = sendMessage<NodeInfoMessage, NodeInfoMessage>(&request, finger_table_.front(), &chord::NodeService::Stub::Stabilize);
        if(reply.id() > info_.id) {
            fillNodeInfo(finger_table_.front(), reply);
            buildFingerTable();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}