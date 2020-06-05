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
#include <filesystem>
#include <google/protobuf/util/time_util.h>

#include <chord/server.hpp>
#include <chord/client.hpp>
#include <mail.hpp>
#include <chord.grpc.pb.h>

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
        std::filesystem::path bck = "181147151130138.dat";
        std::filesystem::remove(bck);
    }

    static mail::Message getRandomMessage(const std::string &from) {
        static std::random_device dev;
        static std::mt19937 rng(dev());
        static std::uniform_int_distribution<std::mt19937::result_type> dist_users(0, users_.size() - 1);
        static std::uniform_int_distribution<std::mt19937::result_type> dist_subjects(0, subjects_.size() - 1);
        static std::uniform_int_distribution<std::mt19937::result_type> dist_bodies(0, bodies_.size() - 1);

        int to_idx = dist_users(rng),
            subject_idx = dist_subjects(rng),
            body_idx = dist_bodies(rng);

        return mail::Message(
            users_.at(to_idx), 
            from,
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
    chord::Client client(info.conn_string());
    for(int i = 0; i < 500; i++) {
        ASSERT_TRUE(client.ping(i));
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
        try {
            chord::Client client(node0_->getInfo());
            chord::NodeInfo reg = client.accountRegister({users_[i], passwords_[i]});
            chord::NodeInfo log = client.accountLogin({users_[i], passwords_[i]});
            ASSERT_EQ(reg.id, log.id);
        } catch (chord::NodeException &e) {
            FAIL() << e.what();
        }
    }
}

TEST_F(NodeTest, LookupNonExisting) {
    chord::Client client(node0_->getInfo());
    EXPECT_THROW(
        client.accountLogin({"non_existing@test.com", "non_existing"}),
        chord::NodeException
    );
}

TEST_F(NodeTest, SendGetMessages) {
    chord::Client client_receiver(node0_->getInfo()),
                  client_sender(node0_->getInfo());
    client_receiver.accountRegister({"get_messages@test.com", "test_psw"});
    client_sender.accountRegister({"send_messages@test.com", "test_psw"});
    std::vector<mail::Message> messages;
    for(int i = 0; i < 10; i++) {
        mail::Message msg = getRandomMessage("send_messages@test.com");
        msg.to = "get_messages@test.com";
        messages.push_back(msg);
        client_sender.send(msg);
    }

    ASSERT_TRUE(client_receiver.getMessages());
    auto messages_rec = client_receiver.getBox().getMessages();
    ASSERT_EQ(messages_rec.size(), 10);
    for(int i = 0; i < messages_rec.size(); i++) {    
        ASSERT_TRUE(messages_rec[i].compare(messages[i]));
    }
}