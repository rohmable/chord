#ifndef CHORD_SERVER_HPP
#define CHORD_SERVER_HPP

#include "types.hpp"
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
    /**
     * Hash function used to generate keys.
     * The function uses a SHA-1 algorithm.
     * These hashes are in the range [0, 2^M)
     * 
     * @param str string to hash
     * @return A key in the range [0, 2^M)
    */
    key_t hashString(const std::string &str);

    /**
     * Handles all node's backend operations.
    */
    class Node final : public NodeService::Service {
    public:
        /* NODE MANAGEMENT METHODS */

        /**
         * Default constructor, used mainly by serializing functions.
         * 
         * This node won't work properly until adress and port are set with setInfo
        */
        Node();
        /**
         * Builds a node with the given address and port.
         * 
         * The node will be ready to execute the services without the need to call the Run method.
         * 
         * @param address ip address of the node
         * @param port ip port that the node will answer to 
        */
        Node(const std::string &address, int port);
        
        /**
         * Destructor, refer to Stop method to check the work done.
        */
        ~Node();

        /**
         * Returns the node's status.
         * 
         * @returns true if the gRPC server is running, false otherwise
        */
        bool isRunning() const;
        /**
         * Start the server operations, from now on the server will answer his requests. 
        */
        void Run();
        /**
         * Stops the server, from now on the server won't answer his requests.
         * 
         * This method will transfer his mailboxes to his successor before closing, if the successor
         * doesn't answer the mailboxes will be dumped in a binary file named after his id.
        */
        void Stop();
        /**
         * Joins the node to the ring.
         * 
         * @param entry_point node reference, the node will contact the entry point and will start
         *                    the join operations, the entry point won't necessarily be his successor.
        */
        void join(const NodeInfo &entry_point);

        /* SERVICES */

        /**
         * Simple service, will answer to the request by filling his reply with the same number sent in the request.
         * 
         * This method shouldn't be called directly, use chord::Client to interact with this service.
         * 
         * @param context metadata used by gRPC
         * @param request ping request
         * @param reply ping reply
         * @returns Status::OK every time
        */
        grpc::Status Ping(grpc::ServerContext *context, const PingRequest *request, PingReply *reply) override;

        /**
         * This service will search for the finger for a given key inside the request.
         * 
         * A node is a finger for a key if is the left-nearest node of the ring.
         * The request will be forwarded inside the ring if this node is not the finger and the reply 
         * will be based on what the neighbour replied.
         * 
         * This method shouldn't be called directly, is used by the nodes internally.
         * 
         * @param context metadata used by gRPC
         * @param request contains the key to search the finger for
         * @param reply contains the answer to the research
         * @returns Status::OK if the finger was found, StatusCode::NOT_FOUND if the request made the entire loop.
        */
        grpc::Status SearchFinger(grpc::ServerContext *context, const FingerQuestion *request, NodeInfoMessage *reply) override;

        /**
         * This service will help a new node to join the ring.
         * 
         * This request should contain the id of the node that wants to join the ring and the answer will be
         * his successor to contact for joining in.
         * 
         * This method shouldn't be called directly, is used by nodes intenally, use Node::join to make a node join the ring
         * 
         * @param context metadata used by gRPC
         * @param request contains the id of the node that wants to join
         * @param reply the node to contact to set as successor inside the ring
         * @returns Status::OK every time
        */
        grpc::Status NodeJoin(grpc::ServerContext *context, const JoinRequest *request, NodeInfoMessage *reply) override;

        /**
         * This service is used to regularly update the successor and predecessor of a node.
         * 
         * Using this service will trigger a sequence of messages that allows the predecessor and the successor to
         * mantain his neighbours updated.
         * 
         * See <a href="https://en.wikipedia.org/wiki/Chord_(peer-to-peer)#Stabilization">stabilization</a> for more details.
         * 
         * This method shouldn't be called directly, is used by nodes intenally
         * 
         * @param context metadata used by gRPC
         * @param request contains the info about the node that considers the executing node his successor
         * @param reply contains the info about the node that the executing node considers his predecessor
         * @returns Status::OK every time 
        */
        grpc::Status Stabilize(grpc::ServerContext *context, const NodeInfoMessage *request, NodeInfoMessage *reply) override;

        /**
         * This service inserts a new mailbox inside the ring, this operations equals the account registration in a mail service.
         * 
         * If the node is not the successor node of the new mailbox the request will be forwarded inside the ring with a TTL of
         * chord::CHORD_MOD steps.
         * 
         * This method shouldn't be called directly, use chord::Client to interact with this service.
         * 
         * @param context metadata used by gRPC
         * @param request new account address and password
         * @param reply successor node for the mailbox
         * @returns Status::OK if the operation was successful, StatusCode::ALREADY_EXISTS if the address is already taken and
         *          StatusCode::NOT_FOUND if the TTL goes to 0 before finding the successor node
        */
        grpc::Status InsertMailbox(grpc::ServerContext *context, const InsertMailboxMessage *request, NodeInfoMessage *reply) override;

        /**
         * This service authenticates a combination of username and password.
         * 
         * This method shouldn't be called directly, is used by nodes intenally
         * 
         * @param context metadata used by gRPC
         * @param request username and password
         * @param reply empty reply
         * @returns Status::OK if the authentication is successful, StatusCode::UNAUTHENTICATED if the request is not valid
        */
        grpc::Status Authenticate(grpc::ServerContext *context, const Authentication *request, Empty *reply);

        /**
         * Finds the successor node of a given mailbox.
         * 
         * The request will be forwarded inside the ring with a TTL of chord::CHORD_MOD.
         * 
         * This method shouldn't be called directly, is used by nodes intenally
         * 
         * @param context metadata used by gRPC
         * @param request mailbox to search for
         * @param reply successor node's info
         * @returns Status::OK if the successor node was found, StatusCode::NOT_FOUND if the TTL reaches 0
        */
        grpc::Status LookupMailbox(grpc::ServerContext *context, const QueryMailbox *request, NodeInfoMessage *reply);

        /**
         * Sends a mail::Message to a certain mailbox.
         * 
         * The service should normally be called on the sender's successor node but the message will be 
         * delivered also if the service is called on another node in the ring, the message will be
         * forwarded with a starting TTL equal to chord::CHORD_MOD.
         * 
         * The service will check the authentication that must match the sender address.
         * 
         * This method shouldn't be called directly, use chord::Client to interact with this service.
         * 
         * @param context metadata used by gRPC
         * @param request the message to send
         * @param reply empty reply
         * @returns Status::OK if the message was delivered successfully, StatusCode::UNAUTHENTICATED if the
         *          authentication was wrong and StatusCode::NOT_FOUND if the TTL reaches 0
        */
        grpc::Status Send(grpc::ServerContext *context, const MailboxMessage *request, Empty *reply);

        /**
         * Deletes a message from a mailbox.
         * 
         * The service should normally be called on the sender's successor node but the message will be 
         * deleted also if the service is called on another node in the ring, the message will be
         * forwarded with a starting TTL equal to chord::CHORD_MOD.
         * 
         * This service will check the authentication.
         * 
         * This method shouldn't be called directly, use chord::Client to interact with this service.
         * 
         * @param context metadata used by gRPC
         * @param request the index of the message to delete
         * @param reply empty reply
         * @returns Status::OK if the message was deleted successfully, StatusCode::OUT_OF_RANGE if the requested message's index
         *          is negative or bigger than the size of the mailbox, StatusCode::NOT_FOUND if the TTL reaches 0
         *          and StatusCode::UNAUTHENTICATED if the authentication doesn't match
        */
        grpc::Status Delete(grpc::ServerContext *context, const DeleteMessage *request, Empty *reply);

        /**
         * Returns the content of a given mailbox.
         * 
         * This service must be called on the successor's node to avoid the transmission of big messages.
         * 
         * This service will check for authentication.
         * 
         * This method shouldn't be called directly, use chord::Client to interact with this service.
         * 
         * @param context metadata used by gRPC
         * @param request authentication data
         * @param reply containing all the necessary data of the mailbox
         * @returns Status::OK if the mailbox was found, StatusCode::UNAUTHENTICATED if the authentication doesn't match
         *          and StatusCode::NOT_FOUND if the mailbox wasn't found.
        */
        grpc::Status Receive(grpc::ServerContext *context, const Authentication *request, Mailbox *reply);

        /**
         * Receives mailboxes from another node.
         * 
         * A node won't be available to accept a transfer request if is stopping his services too.
         * 
         * This service is used by nodes that are shutting down to transfer all their content to another
         * node and keep the service running or at startup phase when a node that has a backup of
         * the mailboxes propagate the mailboxes through the ring.
         * 
         * This method shouldn't be called directly, is used by nodes intenally.
         * 
         * @param context metadata used by gRPC
         * @param request contains the mailboxes to transfer
         * @param reply empty reply
         * @returns Status::OK if the boxes are transferred successfully, StatusCode::UNAVAILABLE if transfers are disabled
         *          StatusCode::INTERNAL if something goes wrong while adding a mailbox.
        */
        grpc::Status Transfer(grpc::ServerContext *context, const TransferMailbox *request, Empty *reply);

        /* PUBLIC INTERFACE */

        /**
         * Starts the build of a new finger table.
        */
        void buildFingerTable();
        
        /**
         * Sets the node's IP address and port.
         * 
         * The new id will be calculated by this method so is not necessary to pre-calculate it.
         * 
         * @param info reference to chord::NodeInfo used to get the new coordinates
        */
        void setInfo(const NodeInfo &info);

        /**
         * @returns this node's coordinates.
        */
        const NodeInfo& getInfo() const;

        /**
         * @returns the number of mailboxes managed by this node.
        */
        int numMailbox() const;

        /**
         * Sets a new successor.
         * 
         * This will notify the successor and, if necessary, start the building of the new finger table.
         * 
         * @param successor new successor
        */
        void setSuccessor(const NodeInfo &successor);

        /**
         * @returns this node's successor
        */
        const NodeInfo& getSuccessor() const;

        /**
         * @returns this node's predecessor
        */
        const NodeInfo& getPredecessor() const;

        /**
         * @throw std::out_of_range if idx is negative or bigger than the size of the finger table 
         * @param idx finger index
         * @returns this node's finger at index idx
        */
        const NodeInfo& getFinger(int idx) const;
        
        /**
         * Method used to serialize the data structure.
         * 
         * Only the chord::NodeInfo of this node will be serialized.
        */
        template <class Archive>
        void serialize(Archive &archive) {
            archive(info_);
        }

    private:

        /* INTERNAL MANAGEMENT METHODS */

        /**
         * Shortcut used to send messages between nodes.
         * 
         * @param request request to send to the node
         * @param to node to send the message to
         * @param rpc function pointer to the method to call on the remote node
         * @returns a pair composed by the grpc::Status of the call and the reply message sent by the remote node.
        */
        template<class T, class R>
        std::pair<grpc::Status, R> sendMessage(const T *request, const NodeInfo &to, grpc::Status (chord::NodeService::Stub::*rpc)(grpc::ClientContext *, const T &, R *)) {
            T req(*request);
            R rep;
            grpc::ClientContext context;
            std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(to.conn_string(), grpc::InsecureChannelCredentials());
            auto stub = chord::NodeService::NewStub(channel);
            return std::pair<grpc::Status, R>((stub.get()->*rpc)(&context, req, &rep), rep);
        }

        /**
         * @param key the key to find the finger for.
         * @returns the correct finger to contact for the given key
        */
        const NodeInfo& getFingerForKey(key_t key);

        /**
         * @param key
         * @returns true if this node is the successor for the given key, false otherwise
        */
        bool isSuccessor(key_t key);

        /**
         * Starts the mailbox transfer procedure described in Node::Transfer.
         * 
         * @param dest the node to send the mailboxes to.
         * @returns true if the procedure was succesful, false otherwise
        */
        bool transferBoxes(const chord::NodeInfo &dest);

        /**
         * Check the authentication of a given user.
         * 
         * This method will search for the successor node of the given address and then checks the 
         * authentication data passed as a parameter.
         * 
         * @param auth authentication data
         * @returns true if the authentication data is good, false otherwise
        */
        bool checkAuthentication(const chord::Authentication &auth);

        /**
         * Method used to periodically run the stabilize procedure described at Node::Stabilize
         * 
         * This is a blocking method so it should be ran by a separate thread.
        */
        void stabilize();

        /**
         * Method used to dump on a file the managed mailboxes.
         * 
         * This will create a .dat file named after the node's id that can be later loaded to 
         * recover all the managed mailboxes.s
        */
        bool dumpBoxes();

        /* MANAGEMENT DATA */

        bool run_stabilize_; /**< Flag used to run and stop the Node::stabilize procedure */
        NodeInfo info_, /**< Coordinates of this node */
                 predecessor_; /**< Coordinates of this node's predecessor */
        bool disable_transfer_; /**< Flag used to enable/disable the Node::Transfer procedure */
        std::vector<NodeInfo> finger_table_; /**< Finger table, this node's successor resides at index 0 */
        std::unique_ptr<grpc::Server> server_; /**< gRPC server used to handle services */
        std::unique_ptr<std::thread> node_thread_, /**< Used to run the Node::server_ */
                                     stabilize_thread_; /**< Used to run the Node::stabilize procedure */
        std::map<key_t, mail::MailBox> boxes_; /**< mail::Mailbox managed by the node */
    };

    /**
     * Convenience class used to handle multiple nodes running on the same machine.
    */
    class Ring {
    public:
        Ring(); /**< Builds an empty ring */

        /**
         * Builds a ring from an accordingly formatted json file.
         * 
         * This file must contain an array named "entities" and each element of the array
         * must be an object with an "address" (string) field and a "port" field (number).
         *  
        */
        Ring(const std::string &json_file);
        ~Ring(); /**< Destructor, this will delete all the nodes pushed to the ring */

        /**
         * Adds a new node to the ring, it will be inserted in the correct position inside the ring
         * 
         * @param node the new node to insert in the ring
        */
        void push_back(Node *node);

        /**
         * Creates a new node with the given parameters and adds it to the ring
         * 
         * @param address node's address
         * @param port node's port
        */
        void emplace_back(const std::string &address, int port);

        /**
         * @returns a reference to the vector managed by the ring
        */
        const std::vector<Node *>& getNodes() const;

        /**
         * @returns a list of the errors catched by the ring
        */
        const std::vector<std::string>& getErrors() const;

        /**
         * @returns a node that can be used as an entry point for join/login operations.
        */
        Node* getEntryNode() const;

        /**
         * Creates a dot file that can be used to get an easier representation of the ring
         * 
         * The dotfile will be composed by the nodes' ids and arrows will point from each node to their successor.
         * 
         * @param filename dot file name
        */
        void dot(const std::string &filename) const;
        
    private:
        std::vector<Node *> ring_; /**< Vector containing all the nodes inside the ring */
        std::vector<std::string> errors_; /**< Vector containing all the errors catched by the ring */
    };
}

#endif // CHORD_SERVER_HPP