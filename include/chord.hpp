#ifndef CHORD_HPP
#define CHORD_HPP

#include <grpcpp/grpcpp.h>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <map>
#include "chord.grpc.pb.h"
#include "mail.hpp"

namespace chord {
    const int M = 48;
    typedef long long int key_t;

    struct NodeInfo {
        std::string address;
        int port;
        key_t id;

        std::string conn_string() const;
    };

    bool operator==(const NodeInfo &lhs, const NodeInfo &rhs);
    inline bool between(key_t key, const NodeInfo &lhs, const NodeInfo &rhs);

    std::unique_ptr<chord::NodeService::Stub> createStub(const chord::NodeInfo &node);

    class Node final : public NodeService::Service {
    public:
        Node();
        Node(std::string address, int port);
        ~Node();

        void Run();
        void Stop();
        void join(const NodeInfo &entry_point);

        grpc::Status Ping(grpc::ServerContext *context, const PingRequest *request, PingReply *reply) override;
        grpc::Status SearchFinger(grpc::ServerContext *context, const FingerQuestion *request, NodeInfoMessage *reply) override;
        grpc::Status NodeJoin(grpc::ServerContext *context, const JoinRequest *request, NodeInfoMessage *reply) override;
        grpc::Status Stabilize(grpc::ServerContext *context, const NodeInfoMessage *request, NodeInfoMessage *reply) override;
        grpc::Status Insert(grpc::ServerContext *context, const InsertMessage *request, NodeInfoMessage *reply) override;
        grpc::Status InsertMailbox(grpc::ServerContext *context, const InsertMailboxMessage *request, NodeInfoMessage *reply) override;
        grpc::Status Lookup(grpc::ServerContext *context, const Query *request, QueryResult *reply) override;
        grpc::Status Authenticate(grpc::ServerContext *context, const Authentication *request, StatusMessage *reply);
        grpc::Status LookupMailbox(grpc::ServerContext *context, const QueryMailbox *request, QueryResult *reply);
        grpc::Status Send(grpc::ServerContext *context, const MailboxMessage *request, StatusMessage *reply);
        grpc::Status Receive(grpc::ServerContext *context, const MailboxRequest *request, Mailbox *reply);

        void buildFingerTable();
        const NodeInfo& getInfo() const;
        void setSuccessor(const NodeInfo &successor);
        const NodeInfo& getSuccessor() const;
        const NodeInfo& getPredecessor() const;
        const NodeInfo& getFinger(int idx) const;
        std::pair<key_t, NodeInfo> insert(const std::string &value);
        std::pair<std::string, const NodeInfo> lookup(const std::string &value);

        std::pair<key_t, NodeInfo> insertMailbox(const mail::MailBox &box);
        std::pair<std::string, const NodeInfo> lookupMailbox(const std::string &owner);

    private:
        template<class T, class R>
        std::pair<grpc::Status, R> sendMessage(const T *request, const NodeInfo &to, grpc::Status (chord::NodeService::Stub::*rpc)(grpc::ClientContext *, const T &, R *)) {
            T req(*request);
            R rep;
            grpc::ClientContext context;
            return std::pair<grpc::Status, R>((chord::createStub(to).get()->*rpc)(&context, req, &rep), rep);
        }

        const NodeInfo& getFingerForKey(key_t key);
        bool isSuccessor(key_t key);
        bool checkAuthentication(const chord::Authentication &auth);

        bool run_stabilize_;
        void stabilize();


        NodeInfo info_, predecessor_;
        std::vector<NodeInfo> finger_table_;
        std::unique_ptr<grpc::Server> server_;
        std::unique_ptr<std::thread> node_thread_, stabilize_thread_;
        std::map<key_t, std::string> values_;
        std::map<key_t, mail::MailBox> boxes_;
    };
}

#endif // CHORD_HPP