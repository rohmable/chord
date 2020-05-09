#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <random>

#include <mail.hpp>
#include <chord.hpp>

class ChordMailTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
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

    static mail::Message getRandomMessage() {
        static std::random_device dev;
        static std::mt19937 rng(dev());
        static std::uniform_int_distribution<std::mt19937::result_type> dist_users(0, users_.size() - 1);
        static std::uniform_int_distribution<std::mt19937::result_type> dist_subjects(0, subjects_.size() - 1);
        static std::uniform_int_distribution<std::mt19937::result_type> dist_bodies(0, bodies_.size() - 1);

        int to_idx = dist_users(rng),
            from_idx = dist_users(rng),
            subject_idx = dist_subjects(rng),
            body_idx = dist_bodies(rng);

        while(to_idx == from_idx) {
            from_idx = dist_users(rng);
        }

        return mail::Message(
            users_.at(to_idx), 
            users_.at(from_idx), 
            subjects_.at(subject_idx),
            bodies_.at(body_idx));
    }

    static std::vector<mail::Message> getRandomMessages() {
        static std::random_device dev;
        static std::mt19937 rng(dev());
        static std::uniform_int_distribution<std::mt19937::result_type> dist(0, 50);

        std::vector<mail::Message> ret;
        int num_msg = dist(rng);
        for(int i = 0; i < num_msg; i++) {
            ret.push_back(getRandomMessage());
        }
        return ret;
    }

    static mail::MailBox getRandomMailbox() {
        static std::random_device dev;
        static std::mt19937 rng(dev());
        static std::uniform_int_distribution<std::mt19937::result_type> dist_users(0, users_.size() - 1);

        return mail::MailBox(users_.at(dist_users(rng)));
    }

    static std::vector<chord::Node *> ring_;
    static chord::Node *node0_;
    static std::vector<std::string> users_,
                                    subjects_,
                                    bodies_;
};

std::vector<chord::Node *> ChordMailTest::ring_;
chord::Node *ChordMailTest::node0_;
std::vector<std::string> ChordMailTest::users_;
std::vector<std::string> ChordMailTest::subjects_;
std::vector<std::string> ChordMailTest::bodies_;

TEST_F(ChordMailTest, InsertLookupMailbox) {
    for(int i = 0; i < users_.size(); i++) {
        mail::MailBox box(users_[i]);
        box.insertMessages(getRandomMessages());
        std::flush(std::cout);
        auto[key, node] = node0_->insertMailbox(box);
        std::flush(std::cout);
        auto[result, manager] = node0_->lookupMailbox(box.getOwner());
        ASSERT_TRUE(result.compare(box.getOwner()) == 0);
        ASSERT_TRUE(node.id == manager.id);
    }
}