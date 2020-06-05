#ifndef CHORD_CLIENT_HPP
#define CHORD_CLIENT_HPP

#include "types.hpp"
#include "chord.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <google/protobuf/util/time_util.h>
#include <mail/mail.hpp>
#include <string>
#include <memory>
#include <ctime>

namespace chord {
    /**
     * This is the main interface for a client with the chord hash-table.
     * 
     * All of the actions required by a mail client are given here.
    */
    class Client {
    public:
        /**
         * Builds a new client from a connection string
         * 
         * @param conn_string in the format "address:port"
        */
        Client(const std::string &conn_string);

        /**
         * Builds a new client from a node's coordinates.
         * 
         * Is the equivalent of:
         * \code{.cpp}
         * Client(node.conn_string());
         * \endcode
        */
        Client(const NodeInfo &node) : Client(node.conn_string()) {}

        /**
         * Connects to a node, the current connection will be dropped.
         * 
         * Is the equivalent of:
         * \code{.cpp}
         * client.connectTo(node.conn_string());
         * \endcode
         * 
         * @param node the new node to connect to
        */
        bool connectTo(const NodeInfo &node);

        /**
         * Connects to a node, the current connection will be dropped.
         * 
         * @param conn_string in the format "address:port"
        */
        bool connectTo(const std::string &conn_string);

        /**
         * @returns the mailbox handled by the client
         * @throw chord::NodeException if the client is not logged in to any account (via Client::accountLogin or Client::accountRegister);
        */
        mail::MailBox& getBox();

        /**
         * Sends a Node::Ping message to the connected node.
        */
        bool ping(int p = 1);

        /**
         * Logs in to an account with the given credentials.
         * 
         * The node will automatically connect to the successor node for the mailbox.
         * 
         * After calling this method it will be possible to interact with the nodes.
         * 
         * @throw NodeException if the account isn't registered or the password is not valid
         * @param address mailbox address to login to.
         * @param password mailbox password to login to
         * @returns the successor node for the given mailbox
         * 
        */
        NodeInfo accountLogin(const std::string &address, const std::string &password) {
            mail::MailBox box(address, password);
            return auth(box, true);
        }

        /**
         * Logs in to an account with the given credentials.
         * 
         * The node will automatically connect to the successor node for the mailbox.
         * 
         * After calling this method it will be possible to interact with the nodes.
         * 
         * Is the equivalent of:
         * \code{.cpp}
         * client.accountLogin(box.getOwner(), box.getPassword());
         * \endcode
         * 
         * @throw NodeException if the account isn't registered or the password is not valid
         * @param box to log into
         * @returns the successor node for the given mailbox
        */
        NodeInfo accountLogin(const mail::MailBox &box) {
            return auth(box, true);
        }

        /**
         * Registers a new account with the given credentials.
         * 
         * The node will automatically connect to the successor node for the mailbox.
         * 
         * After calling this method it will be possible to interact with the nodes.
         * 
         * @throw NodeException if the account isn't registered or the password is not valid.
         *        This normally shouldn't happen but Node::InsertMailbox and Node::Authenticate
         *        can throw errors if something unexpected happens so this eventuality is handled
         * @param address mailbox address to login to.
         * @param password mailbox password to login to
         * @returns the successor node for the given mailbox
        */
        NodeInfo accountRegister(const std::string &address, const std::string &password) {
            mail::MailBox box(address, password);
            return auth(box, false);
        }

        /**
         * Registers a new account with the given credentials.
         * 
         * The node will automatically connect to the successor node for the mailbox.
         * 
         * After calling this method it will be possible to interact with the nodes.
         * 
         * Is the equivalent of:
         * \code{.cpp}
         * client.accountRegister(box.getOwner(), box.getPassword());
         * \endcode
         * 
         * @throw NodeException if the account isn't registered or the password is not valid.
         *        This normally shouldn't happen but Node::InsertMailbox and Node::Authenticate
         *        can throw errors if something unexpected happens so this eventuality is handled
         * @param box to log into
         * @returns the successor node for the given mailbox
        */
        NodeInfo accountRegister(const mail::MailBox &box) {
            return auth(box, false);
        }

        /**
         * Retrieves the messages from the node.
         * 
         * The messages will be cached inside the box and can be accessed by calling the mail::MailBox::getMessages method
         * on the box returned by Client::getBox.
         * 
         * This method should only be called after Client::accountLogin or Client::accountRegister.
         * 
         * See Node::Receive for more details on the server side.
         * 
         * @returns true if the mailbox is updated and ready to be read, false otherwise.
         *          Normally if the login was successful and the client is still connected to the mailbox's successor
         *          this method should always return true.
        */
        bool getMessages();

        /**
         * Sends a new mail.
         * 
         * See Node::Send for more details on the server side.
         * 
         * This method should only be called after Client::accountLogin or Client::accountRegister.
         * 
         * @throw chord::NodeException if the message was not sent succesfully
         * @param message to send
        */
        void send(const mail::Message &message);

        /**
         * Deletes a mail from the mailbox.
         * 
         * See Node::Delete for more details on the server side.
         * 
         * This method should only be called after Client::accountLogin or Client::accountRegister.
         * 
         * @throw chord::NodeException if the message was not sent succesfully.
         * @param idx index of the message to delete
        */
        void remove(int idx);

    private:
        /**
         * Convenience method to cast a google::protobuf::int64 to a time_t.
         * 
         * This is used to get data from a chord::MailboxMessage to a mail::Message.
         * 
         * @param secs seconds from epoch
         * @returns a time_t equivalent
        */
        static time_t secondsToTimeT(google::protobuf::int64 secs);

        /**
         * Convenience method to cast a time_t to a google::protobuf::int64.
         * 
         * This is used to get data from a mail::Message to a chord::MailboxMessage.
         * 
         * @param time 
         * @returns seconds from epoch
        */
        static google::protobuf::int64 timeTToSeconds(time_t time);

        /**
         * Fill a chord::MailboxMessage from a mail::Message.
         * 
         * @param dst chord::MailboxMessage destination reference
         * @param src mail::Message source reference
        */
        void fillMailboxMessage(MailboxMessage &dst, const mail::Message &src);

        /**
         * Fills a chord::DeleteMessage with index passed by parameter.
         * 
         * Authentication data will also be filled by this method.
         * 
         * @param dst message to fill
         * @param idx index of the message to delete
        */
        void fillDeleteMessage(DeleteMessage &dst, int idx);

        /**
         * Authentication method.
         * 
         * Client::accountRegister and Client::accountLogin will use this method due to their similarity.
         * 
         * @throw NodeException if the lookup or the authentication procedure goes wrong.
         * @param box box to login/register into
         * @param login true if the client should attempt a login or false if the box is new and should be
         *              added to the system
        */
        NodeInfo auth(const mail::MailBox &box, bool login = true);

        /**
         * Shortcut used to send messages to a node.
         * 
         * @param request request to send to the node
         * @param rpc function pointer to the method to call on the remote node
         * @returns a pair composed by the grpc::Status of the call and the reply message sent by the remote node.
        */
        template<class T, class R>
        std::pair<grpc::Status, R> sendMessage(const T *request, grpc::Status (chord::NodeService::Stub::*rpc)(grpc_impl::ClientContext *, const T &, R *)) {
            T req(*request);
            R rep;
            grpc::ClientContext context;
            grpc::Status status = (stub_.get()->*rpc)(&context, req, &rep);
            return std::pair<grpc::Status, R>(status, rep);
        }

        std::unique_ptr<chord::NodeService::Stub> stub_; /**< Stub used to send remote calls */
        std::shared_ptr<mail::MailBox> box_; /**< Mailbox handled by the client */
    };
}

#endif // CHORD_CLIENT_HPP