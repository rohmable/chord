#ifndef CHORD_HPP
#define CHORD_HPP

#include <grpcpp/grpcpp.h>
#include <string>
#include <thread>
#include <map>
#include <exception>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
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

        template<class Archive>
        void serialize(Archive &archive) {
            archive(CEREAL_NVP(address), CEREAL_NVP(port));
        }
    };

    bool operator==(const NodeInfo &lhs, const NodeInfo &rhs);
    inline bool between(key_t key, const NodeInfo &lhs, const NodeInfo &rhs);

    key_t hashString(const std::string &str);

    std::unique_ptr<chord::NodeService::Stub> createStub(const chord::NodeInfo &node);

    struct NodeException : public std::exception {
    public:
        NodeException(const std::string &conn_string);
        const char *what() const throw();
    private:
        std::string msg_;
    };

    class Node final : public NodeService::Service {
    public:
        Node();
        Node(const std::string &address, int port);
        ~Node();

        bool isRunning() const;
        void Run();
        void Stop();
        void join(const NodeInfo &entry_point);

        grpc::Status Ping(grpc::ServerContext *context, const PingRequest *request, PingReply *reply) override;
        grpc::Status SearchFinger(grpc::ServerContext *context, const FingerQuestion *request, NodeInfoMessage *reply) override;
        grpc::Status NodeJoin(grpc::ServerContext *context, const JoinRequest *request, NodeInfoMessage *reply) override;
        grpc::Status Stabilize(grpc::ServerContext *context, const NodeInfoMessage *request, NodeInfoMessage *reply) override;
        grpc::Status InsertMailbox(grpc::ServerContext *context, const InsertMailboxMessage *request, NodeInfoMessage *reply) override;
        grpc::Status Authenticate(grpc::ServerContext *context, const Authentication *request, StatusMessage *reply);
        grpc::Status LookupMailbox(grpc::ServerContext *context, const QueryMailbox *request, QueryResult *reply);
        grpc::Status Send(grpc::ServerContext *context, const MailboxMessage *request, StatusMessage *reply);
        grpc::Status Receive(grpc::ServerContext *context, const Authentication *request, Mailbox *reply);
        grpc::Status Transfer(grpc::ServerContext *context, const TransferMailbox *request, StatusMessage *reply);

        void buildFingerTable();
        const NodeInfo& getInfo() const;
        int numMailbox() const;
        void setSuccessor(const NodeInfo &successor);
        const NodeInfo& getSuccessor() const;
        const NodeInfo& getPredecessor() const;
        const NodeInfo& getFinger(int idx) const;

        std::pair<key_t, NodeInfo> insertMailbox(const mail::MailBox &box);
        std::pair<std::string, const NodeInfo> lookupMailbox(const std::string &owner);
        

        template <class Archive>
        void serialize(Archive &archive) {
            archive(info_);
        }

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
        bool transferBoxes(const chord::NodeInfo &dest);
        bool checkAuthentication(const chord::Authentication &auth);

        bool run_stabilize_;
        void stabilize();

        bool dumpBoxes();


        NodeInfo info_, predecessor_;
        std::vector<NodeInfo> finger_table_;
        std::unique_ptr<grpc::Server> server_;
        std::unique_ptr<std::thread> node_thread_, stabilize_thread_;
        std::map<key_t, std::string> values_;
        std::map<key_t, mail::MailBox> boxes_;
    };

    class Ring {
    public:
        Ring();
        Ring(const std::string &json_file);
        ~Ring();
        void push_back(Node *node);
        void emplace_back(const std::string &address, int port);
        const std::vector<Node *>& getNodes() const;
        Node* getEntryNode() const;

        void dot(const std::string &filename) const;
        void print(std::ostream &os) const;
        
    private:
        std::vector<Node *> ring_;
        std::vector<std::string> errors_;
    };

    std::ostream& operator << (std::ostream &out, const Ring &ring);

}

#endif // CHORD_HPP