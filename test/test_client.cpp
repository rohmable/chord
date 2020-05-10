#include "test_client.hpp"

Client::Client(std::shared_ptr<grpc::Channel> channel)
    : stub_(chord::NodeService::NewStub(channel)) {}

Client::Client(const std::string &conn_string)
    : stub_(chord::NodeService::NewStub(grpc::CreateChannel(conn_string, grpc::InsecureChannelCredentials()))) {}

Client::Client(const chord::NodeInfo &node)
    : stub_(chord::NodeService::NewStub(grpc::CreateChannel(node.conn_string(), grpc::InsecureChannelCredentials()))) {}

std::pair<grpc::Status, chord::PingReply> Client::SendPing(int ping_n) {
    chord::PingRequest request;
    chord::PingReply reply;
    request.set_ping_n(ping_n);

    grpc::ClientContext context;
    grpc::Status status = stub_->Ping(&context, request, &reply);
    return std::pair<grpc::Status, chord::PingReply>(status, reply);
}
