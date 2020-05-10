#ifndef TEST_CLIENT_HPP
#define TEST_CLIENT_HPP

#include <gtest/gtest.h>
#include <utility>

#include <chord.hpp>
#include <chord.grpc.pb.h>

class Client {
public:
    explicit Client(std::shared_ptr<grpc::Channel> channel);
    explicit Client(const std::string &conn_string);
    explicit Client(const chord::NodeInfo &node);

    std::pair<grpc::Status, chord::PingReply> SendPing(int ping_n);
    template<class T, class R>
    std::pair<grpc::Status, R> sendMessage(const T *request, grpc::Status (chord::NodeService::Stub::*rpc)(grpc_impl::ClientContext *, const T &, R *)) {
        T req(*request);
        R rep;
        grpc::ClientContext context;
        grpc::Status status = (stub_.get()->*rpc)(&context, req, &rep);
        return std::pair<grpc::Status, R>(status, rep);
    }

private:
    std::unique_ptr<chord::NodeService::Stub> stub_;
};

#endif // TEST_CLIENT_HPP