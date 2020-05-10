#include <gtest/gtest.h>
#include <grpcpp/grpcpp.h>
#include <cmath>
#include <memory>
#include <algorithm>
#include <string>
#include <utility>
#include <fstream>
#include <vector>
#include <chrono>
#include <random>
#include <nlohmann/json.hpp>

#include <chord.hpp>
#include <chord.grpc.pb.h>
#include "test_client.hpp"

void ringDot(std::vector<chord::Node*> &ring) {
    std::ofstream dot("ring.gv");
    if(dot.is_open()) {
        dot << "digraph Ring {" << std::endl;
        for(int i = 0; i < ring.size() - 1; i++) {
            dot << '\t' << ring[i]->getInfo().id << " -> " << ring[i+1]->getInfo().id << ';' << std::endl;
        }
        dot << '\t' << ring.back()->getInfo().id << " -> " << ring.front()->getInfo().id << ';' << std::endl;
        dot << '}';
        dot.close();
    }
}

const std::string& getRandomUser(std::vector<std::string> &users) {
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, users.size());
    return users.at(distribution(generator));
}

class DISABLED_NodeTest : public ::testing::Test {
protected:
    static void SetUpTestCase() {
        using nlohmann::json;

        std::ifstream cfg_file("cfg.test.json");
        json cfg;
        cfg_file >> cfg;
        cfg_file.close();
        json nodes = cfg.at("entities");
        for(auto &node : nodes) {
            std::string address = node.at("address").get<std::string>();
            int port = node.at("port").get<int>();
            ring_.push_back(new chord::Node(address, port));
        }
        std::sort(ring_.begin(), ring_.end(), [](chord::Node *lhs, chord::Node *rhs) {
            return lhs->getInfo().id < rhs->getInfo().id;
        });
        for(int i = 0; i < ring_.size() - 1; i++) {
            ring_[i]->setSuccessor(ring_[i+1]->getInfo());
        }
        ring_.back()->setSuccessor(ring_.front()->getInfo());
        node0_ = ring_.front();
        for(auto node : ring_) {
            node->buildFingerTable();
        }

        ringDot(ring_);

        std::ifstream mock_file("mock_data.json");
        json mock;
        mock_file >> mock;
        mock_file.close();
        json users = mock.at("users");
        for(auto &node : nodes) {
            users_.push_back(node.at("address").get<std::string>());
        }
    }

    static void TearDownTestSuite() {
        for(auto node : ring_) {
            node->Stop();
            delete node;
        }
    }

    static std::vector<chord::Node *> ring_;
    static chord::Node *node0_;
    static std::vector<std::string> users_;
};

chord::Node *DISABLED_NodeTest::node0_ = nullptr;
std::vector<chord::Node*> DISABLED_NodeTest::ring_;
std::vector<std::string> DISABLED_NodeTest::users_;

TEST_F(DISABLED_NodeTest, EmptyNode) {
    chord::Node n;
}

TEST_F(DISABLED_NodeTest, WithAddress) {
    chord::Node n("127.0.0.1", 50005);
}

TEST_F(DISABLED_NodeTest, SendPing) {
    chord::NodeInfo info = node0_->getInfo();
    chord::PingRequest request;
    request.set_ping_n(0);
    Client client(info);
    auto[result, msg] = client.sendMessage<chord::PingRequest, chord::PingReply>(&request, &chord::NodeService::Stub::Ping);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(msg.ping_id(), info.id);
    ASSERT_EQ(msg.ping_ip(), info.address);
    ASSERT_EQ(msg.ping_port(), info.port);
    ASSERT_EQ(msg.ping_n(), request.ping_n()) << "Ping reply is different from what has been sent: " << msg.ping_n() << " != " << request.ping_n();
}

TEST_F(DISABLED_NodeTest, SendMultiplePing) {
    chord::NodeInfo info = node0_->getInfo();
    chord::PingRequest request;
    Client client(info);
    for (int i = 0; i < 500; i++) {
        request.set_ping_n(i);
        auto[result, msg] = client.sendMessage<chord::PingRequest, chord::PingReply>(&request, &chord::NodeService::Stub::Ping);
        ASSERT_TRUE(result.ok());
        ASSERT_EQ(msg.ping_id(), info.id);
        ASSERT_EQ(msg.ping_ip(), info.address);
        ASSERT_EQ(msg.ping_port(), info.port);
        ASSERT_EQ(msg.ping_n(), i) << "Ping reply is different from what has been sent: " << msg.ping_n() << " != " << i;
    }
}

TEST_F(DISABLED_NodeTest, SetSuccessor) {
    chord::Node n1("127.0.0.1", 50050),
                n2("127.0.0.1", 50051);
    chord::NodeInfo n2_info = n2.getInfo();
    n1.setSuccessor(n2_info);
    chord::NodeInfo n1_successor = n1.getSuccessor();
    ASSERT_TRUE(
        n1_successor.address.compare(n2_info.address) == 0
        && n1_successor.port == n2_info.port
        && n1_successor.id == n2_info.id
    );
}

TEST_F(DISABLED_NodeTest, FingerTable) {
    chord::key_t mod = std::pow(2, chord::M);
    for(auto node : ring_) {
        chord::key_t node_id = node->getInfo().id;
        for(int i = 0; i < chord::M; i++) {
            chord::key_t finger_id = node->getFinger(i).id,
                             finger_val = static_cast<chord::key_t>(node_id + std::pow(2, i)) % mod;
            ASSERT_TRUE(finger_id >= finger_val || (finger_id < node_id && finger_id < finger_val)) 
                << "Finger " << i << " of node " << node_id << " doesn't match" << std::endl
                << finger_id << " < " << finger_val;
        }
    }
}

TEST_F(DISABLED_NodeTest, CorrectPredecessor) {
    ASSERT_TRUE(ring_.front()->getPredecessor().id == ring_.back()->getInfo().id);
    for(int i = 1; i < ring_.size(); i++) {
        ASSERT_TRUE(ring_.at(i)->getPredecessor().id == ring_.at(i-1)->getInfo().id);
    }
}

TEST_F(DISABLED_NodeTest, TestNodeJoinCorrectId) {
    Client client(node0_->getInfo());
    chord::JoinRequest request;
    request.set_node_id(1234321);
    auto[result, msg] = client.sendMessage<chord::JoinRequest, chord::NodeInfoMessage>(&request, &chord::NodeService::Stub::NodeJoin);
    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(msg.id() >= request.node_id());
}

TEST_F(DISABLED_NodeTest, TestNodeJoin) {
    chord::Node *new_node = new chord::Node("127.0.0.1", 60000);
    ring_.push_back(new_node);
    new_node->join(node0_->getInfo());
    std::sort(ring_.begin(), ring_.end(), [](const chord::Node *lhs, const chord::Node *rhs) {
        return lhs->getInfo().id < rhs->getInfo().id;
    });
    std::this_thread::sleep_for(std::chrono::seconds(2));
    for(int i = 0; i < ring_.size() - 1; i++) {
        ASSERT_EQ(ring_[i]->getSuccessor().id, ring_[i+1]->getInfo().id);
    }
    ringDot(ring_);
}

TEST_F(DISABLED_NodeTest, TestInsertAndLookup) {
    const std::string &user = getRandomUser(users_);
    auto[ins_key, ins_manager] = node0_->insert(user);
    auto[query_result, look_manager] = node0_->lookup(user);
    ASSERT_TRUE(query_result.compare(user) == 0);
    ASSERT_TRUE(ins_manager.id == look_manager.id);
}

TEST_F(DISABLED_NodeTest, BulkInsertAndLookup) {
    for(const auto &user : users_) {
        auto[ins_key, ins_manager] = node0_->insert(user);
        auto[query_result, look_manager] = node0_->lookup(user);
        ASSERT_TRUE(query_result.compare(user) == 0);
        ASSERT_TRUE(ins_manager.id == look_manager.id);
    }
}