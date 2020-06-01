#include <gtest/gtest.h>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
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
#include <google/protobuf/util/time_util.h>

#include <chord.hpp>
#include <mail.hpp>
#include <chord.grpc.pb.h>
#include "test_client.hpp"

void ringDot(const std::vector<chord::Node*> &ring) {
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

class NodeTest : public ::testing::Test {
protected:
    static void SetUpTestCase() {
        ring_ = new chord::Ring("cfg.test.json");
        node0_ = ring_->getEntryNode();

        {
            std::ifstream is("mock_data.json");
            cereal::JSONInputArchive archive(is);
            archive(cereal::make_nvp("users", users_));
            archive(cereal::make_nvp("passwords", passwords_));
            archive(cereal::make_nvp("subjects", subjects_));
            archive(cereal::make_nvp("bodies", bodies_));
        }
    }

    static void TearDownTestCase() {
        delete ring_;
    }

    static mail::Message getRandomMessage(const std::string &to) {
        static std::random_device dev;
        static std::mt19937 rng(dev());
        static std::uniform_int_distribution<std::mt19937::result_type> dist_users(0, users_.size() - 1);
        static std::uniform_int_distribution<std::mt19937::result_type> dist_subjects(0, subjects_.size() - 1);
        static std::uniform_int_distribution<std::mt19937::result_type> dist_bodies(0, bodies_.size() - 1);

        int from_idx = dist_users(rng),
            subject_idx = dist_subjects(rng),
            body_idx = dist_bodies(rng);

        return mail::Message(
            to, 
            users_.at(from_idx), 
            subjects_.at(subject_idx),
            bodies_.at(body_idx));
    }

    static std::vector<mail::Message> getRandomMessages(const std::string &to) {
        static std::random_device dev;
        static std::mt19937 rng(dev());
        static std::uniform_int_distribution<std::mt19937::result_type> dist(0, 50);

        std::vector<mail::Message> ret;
        int num_msg = dist(rng);
        for(int i = 0; i < num_msg; i++) {
            ret.push_back(getRandomMessage(to));
        }
        return ret;
    }

    static mail::MailBox getRandomMailbox() {
        static std::random_device dev;
        static std::mt19937 rng(dev());
        static std::uniform_int_distribution<std::mt19937::result_type> dist_users(0, users_.size() - 1);
        static std::uniform_int_distribution<std::mt19937::result_type> dist_passwords(0, passwords_.size() - 1);

        return mail::MailBox(users_[dist_users(rng)], passwords_[dist_passwords(rng)]);
    }

    static chord::Ring *ring_;
    static chord::Node *node0_;
    static std::vector<std::string> users_,
                                    passwords_,
                                    subjects_,
                                    bodies_;
};


chord::Ring *NodeTest::ring_;
chord::Node *NodeTest::node0_ = nullptr;
std::vector<std::string> NodeTest::users_;
std::vector<std::string> NodeTest::passwords_;
std::vector<std::string> NodeTest::subjects_;
std::vector<std::string> NodeTest::bodies_;

time_t secondsToTimeT(google::protobuf::int64 sec) {
    using google::protobuf::util::TimeUtil;
    return TimeUtil::TimestampToTimeT(TimeUtil::SecondsToTimestamp(sec));
}

google::protobuf::int64 timeTToSeconds(time_t time) {
    using google::protobuf::util::TimeUtil;
    return TimeUtil::TimestampToSeconds(TimeUtil::TimeTToTimestamp(time));
}

void fillMailboxMessage(chord::MailboxMessage &dst, const mail::Message &src, const mail::MailBox &box) {
    chord::Authentication *auth = new chord::Authentication;
    auth->set_user(box.getOwner());
    auth->set_psw(box.getPassword());
    dst.set_allocated_auth(auth);
    dst.set_to(src.to); dst.set_from(src.from); dst.set_subject(src.subject);
    dst.set_body(src.body);
    dst.set_date(timeTToSeconds(src.date));
}

TEST_F(NodeTest, EmptyNode) {
    chord::Node n;
}

TEST_F(NodeTest, WithAddress) {
    chord::Node n("0.0.0.0", 60005);
}

TEST_F(NodeTest, SendPing) {
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

TEST_F(NodeTest, SetSuccessor) {
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

TEST_F(NodeTest, FingerTable) {
    chord::key_t mod = std::pow(2, chord::M);
    for(auto node : ring_->getNodes()) {
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

TEST_F(NodeTest, CorrectPredecessor) {
    auto &nodes = ring_->getNodes();
    ASSERT_TRUE(nodes.front()->getPredecessor().id == nodes.back()->getInfo().id);
    for(int i = 1; i < nodes.size(); i++) {
        ASSERT_TRUE(nodes[i]->getPredecessor().id == nodes[i-1]->getInfo().id);
    }
}

TEST_F(NodeTest, TestNodeJoinCorrectId) {
    Client client(node0_->getInfo());
    chord::JoinRequest request;
    request.set_node_id(1234321);
    auto[result, msg] = client.sendMessage<chord::JoinRequest, chord::NodeInfoMessage>(&request, &chord::NodeService::Stub::NodeJoin);
    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(msg.id() >= request.node_id());
}

TEST_F(NodeTest, TestNodeJoin) {
    chord::Node *new_node = new chord::Node("127.0.0.1", 60000);
    ring_->push_back(new_node);
    new_node->join(node0_->getInfo());
    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto &nodes = ring_->getNodes();
    for(int i = 0; i < nodes.size() - 1; i++) {
        ASSERT_EQ(nodes[i]->getSuccessor().id, nodes[i+1]->getInfo().id);
    }
    ringDot(nodes);
}

TEST_F(NodeTest, InsertLookupMailbox) {
    for(int i = 0; i < users_.size(); i++) {
        mail::MailBox box(users_[i], passwords_[i]);
        auto[key, node] = node0_->insertMailbox(box);
        auto[result, manager] = node0_->lookupMailbox(box.getOwner());
        ASSERT_EQ(result.compare(box.getOwner()), 0);
        ASSERT_EQ(node.id, manager.id);
    }
}

TEST_F(NodeTest, LookupNonExisting) {
    mail::MailBox box("non_existing@test.com", "non_existing");
    EXPECT_THROW(
        node0_->lookupMailbox(box.getOwner()),
        std::out_of_range
    );
}

TEST_F(NodeTest, SendMessage) {
    mail::MailBox box("send_message@test.com", "test_psw");
    mail::MailBox box_wrong_psw("send_message@test.com", "wrong_psw");
    mail::Message msg = getRandomMessage(box.getOwner());
    auto[result, manager] = node0_->insertMailbox(box);
    Client client(manager);
    chord::MailboxMessage message, message_wrong_psw;
    fillMailboxMessage(message, msg, box);
    fillMailboxMessage(message_wrong_psw, msg, box_wrong_psw);
    auto[status_wrong_psw, res_wrong_psw] = client.sendMessage<chord::MailboxMessage, chord::StatusMessage>(&message_wrong_psw, &chord::NodeService::Stub::Send);
    ASSERT_TRUE(status_wrong_psw.ok());
    ASSERT_FALSE(res_wrong_psw.result());
    auto[status, res] = client.sendMessage<chord::MailboxMessage, chord::StatusMessage>(&message, &chord::NodeService::Stub::Send);
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(res.result());
}

TEST_F(NodeTest, GetMessages) {
    mail::MailBox box("get_messages@test.com", "test_psw");
    auto[result, manager] = node0_->insertMailbox(box);
    Client client(manager);
    std::vector<mail::Message> messages;
    for(int i = 0; i < 10; i++) {
        messages.push_back(getRandomMessage(box.getOwner()));
        chord::MailboxMessage message;
        fillMailboxMessage(message, messages[i], box);
        auto[status, res] = client.sendMessage<chord::MailboxMessage, chord::StatusMessage>(&message, &chord::NodeService::Stub::Send);
        ASSERT_TRUE(status.ok());
        ASSERT_TRUE(res.result());
    }
    chord::Authentication req;
    req.set_user(box.getOwner());
    req.set_psw(mail::hashPsw("wrong_password"));
    auto[wrong_pwd_status, wrong_pwd_mailbox] = client.sendMessage<chord::Authentication, chord::Mailbox>(&req, &chord::NodeService::Stub::Receive);
    ASSERT_FALSE(wrong_pwd_mailbox.valid());
    req.set_psw(box.getPassword());
    auto[status, mailbox] = client.sendMessage<chord::Authentication, chord::Mailbox>(&req, &chord::NodeService::Stub::Receive);
    ASSERT_TRUE(mailbox.valid());
    ASSERT_EQ(mailbox.messages().size(), 10);
    for(int i = 0; i < mailbox.messages().size(); i++) {
        const chord::MailboxMessage &msg = mailbox.messages().at(i);
        ASSERT_EQ(msg.to().compare(messages[i].to), 0);
        ASSERT_EQ(msg.from().compare(messages[i].from), 0);
        ASSERT_EQ(msg.subject().compare(messages[i].subject), 0);
        ASSERT_EQ(msg.body().compare(messages[i].body), 0);
        ASSERT_EQ(msg.date(), timeTToSeconds(messages[i].date));
    }
}