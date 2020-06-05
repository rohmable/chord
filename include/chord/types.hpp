#ifndef CHORD_TYPES_HPP
#define CHORD_TYPES_HPP

#include "chord.grpc.pb.h"
#include <string>
#include <exception>
#include <cmath>
#include <cereal/archives/json.hpp>

namespace chord {
    const int M = 48; /**< Control parameter for key length keys will be in range [0, 2^M)*/
    const long long int CHORD_MOD = std::ceil(std::log(std::pow(2, M))); /**< Is proven that the algorithm will reach the successor in CHORD_MOD steps */
    typedef long long int key_t; /**< Type that contains an hashed key for the algorithm */

    /**
     * Models a node's coordinates.
     * 
     * This struct contains all the data to connect and identify a node on the ring.
    */
    struct NodeInfo {
        std::string address; /**< IP address of the node */
        int port; /**< Port used to contact the node, multiple nodes may run on the same IP */
        key_t id; /**< Key associated to the node, is the result of hashing the string "address:port" */
        
        /**
         * Creates the connection string
         * 
         * @returns a string in the format address:port
        */
        std::string conn_string() const {
            return address + ":" + std::to_string(port);
        }

        /**
         * Method used to serialize the data structure.
         * 
         * Only address and port will be saved.
        */
        template<class Archive>
        void serialize(Archive &archive) {
            archive(CEREAL_NVP(address), CEREAL_NVP(port));
        }
    };

    /**
     * Equality operator for NodeInfo 
     * 
     * @param lhs left hand side NodeInfo
     * @param rhs right hand side NodeInfo
     * @returns true if every field of lhs and rhs are equal, false otherwise
    */
    inline bool operator==(const NodeInfo &lhs, const NodeInfo &rhs) {
        return (lhs.address.compare(rhs.address) == 0) && 
               (lhs.id == rhs.id) &&
               (lhs.port == rhs.port);
    }

    /**
     * Checks if the given key is contained inside the interval (lhs.id, rhs.id].
     * 
     * Another way that a key can fall "between" two nodes is if the key is bigger than the
     * last node inside the ring, this node's successor has a smaller id compared to the former,
     * in this case the key will fall on the smaller node.
     * The same applies if the key is smaller than the first node inside the ring.
     * 
     * @returns true if lhs.id < key <= rhs.id or key falls between the biggest id and the lowest, false otherwise
    */
    inline bool between(key_t key, const NodeInfo &lhs, const NodeInfo &rhs) {
        return (key > lhs.id && (key <= rhs.id || lhs.id > rhs.id)) ||
                (key <= rhs.id && key < lhs.id && rhs.id < lhs.id);
    }

    /**
     *  Fills a NodeInfoMessage used by gRPC using a NodeInfo reference 
    */
    inline void fillNodeInfoMessage(chord::NodeInfoMessage &dst, const chord::NodeInfo &src) {
        dst.set_ip(src.address); dst.set_port(src.port); dst.set_id(src.id);
    }

    /**
     * Fills a NodeInfo reference with a NodeInfoMessage used by gRPC
    */
    inline void fillNodeInfo(chord::NodeInfo &dst, const chord::NodeInfoMessage &src) {
        dst.address = src.ip(); dst.port = src.port(); dst.id = src.id();
    }


    /**
     * Exception type used to signal errors to anyone that uses the service
    */
    struct NodeException : public std::exception {
    public:
        /**
         * Builds an exception with the given message, the message will be appended to a "NodeException: " string
         * 
         * @param msg Message to append
        */
        NodeException(const std::string &msg)
            : msg_(std::string("NodeException: ") + msg) {}

        /**
         *  Gives details on the exception.
         * 
         * @returns a string containing the error message 
        */
        const char * what() const throw() {
            return msg_.c_str();
        }
    private:
        std::string msg_; /**< Message string */
    };
}

#endif // CHORD_TYPES_HPP