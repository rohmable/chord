#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <random>
#include <google/protobuf/util/time_util.h>

#include <mail.hpp>
#include <chord.hpp>
#include "test_client.hpp"

class ChordMailTest : public ::testing::Test {
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
        json mock_data;
        mock_file >> mock_data;
        mock_file.close();
        for(auto &user : mock_data.at("users")) {
            users_.push_back(user.at("address").get<std::string>());
        }
        for(auto &password : mock_data.at("passwords")) {
            passwords_.push_back(password.at("password").get<std::string>());
        }
        for(auto &subject : mock_data.at("subjects")) {
            subjects_.push_back(subject.at("subject").get<std::string>());
        }
        for(auto &body : mock_data.at("bodies")) {
            bodies_.push_back(body.at("body").get<std::string>());
        }
    }

    static void ringDot(std::vector<chord::Node*> &ring) {
        std::ofstream dot("ringmail.gv");
        if(dot.is_open()) {
            dot << "digraph Ring {" << std::endl;
            for(int i = 0; i < ring.size() - 1; i++) {
                dot << '\t' << ring[i]->getInfo().id << " -> " << ring[i]->getSuccessor().id << ';' << std::endl;
            }
            dot << '\t' << ring.back()->getInfo().id << " -> " << ring.back()->getSuccessor().id << ';' << std::endl;
            dot << '}';
            dot.close();
        }
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

    static std::vector<chord::Node *> ring_;
    static chord::Node *node0_;
    static std::vector<std::string> users_,
                                    passwords_,
                                    subjects_,
                                    bodies_;
};

std::vector<chord::Node *> ChordMailTest::ring_;
chord::Node *ChordMailTest::node0_;
std::vector<std::string> ChordMailTest::users_;
std::vector<std::string> ChordMailTest::passwords_;
std::vector<std::string> ChordMailTest::subjects_;
std::vector<std::string> ChordMailTest::bodies_;

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

TEST_F(ChordMailTest, InsertLookupMailbox) {
    for(int i = 0; i < users_.size(); i++) {
        mail::MailBox box(users_[i], passwords_[i]);
        auto[key, node] = node0_->insertMailbox(box);
        auto[result, manager] = node0_->lookupMailbox(box.getOwner());
        ASSERT_EQ(result.compare(box.getOwner()), 0);
        ASSERT_EQ(node.id, manager.id);
    }
}

TEST_F(ChordMailTest, SendMessage) {
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

TEST_F(ChordMailTest, GetMessages) {
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
    chord::MailboxRequest req;
    req.set_owner(box.getOwner());
    req.set_password(mail::hashPsw("wrong_password"));
    auto[wrong_pwd_status, wrong_pwd_mailbox] = client.sendMessage<chord::MailboxRequest, chord::Mailbox>(&req, &chord::NodeService::Stub::Receive);
    ASSERT_FALSE(wrong_pwd_mailbox.valid());
    req.set_password(box.getPassword());
    auto[status, mailbox] = client.sendMessage<chord::MailboxRequest, chord::Mailbox>(&req, &chord::NodeService::Stub::Receive);
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

