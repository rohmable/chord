#include <gtest/gtest.h>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <mail.hpp>

class MailTest : public ::testing::Test {
protected:
    static void SetUpTestCase() {
        {
            std::ifstream is("mock_data.json");
            cereal::JSONInputArchive archive(is);
            archive(cereal::make_nvp("users", users_));
            archive(cereal::make_nvp("passwords", passwords_));
            archive(cereal::make_nvp("subjects", subjects_));
            archive(cereal::make_nvp("bodies", bodies_));
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
        static std::uniform_int_distribution<std::mt19937::result_type> dist_passwords(0, passwords_.size() - 1);

        return mail::MailBox(users_.at(dist_users(rng)), passwords_.at(dist_passwords(rng)));
    }

    static std::vector<std::string> users_,
                                    passwords_,
                                    subjects_,
                                    bodies_;
};

std::vector<std::string> MailTest::users_;
std::vector<std::string> MailTest::passwords_;
std::vector<std::string> MailTest::subjects_;
std::vector<std::string> MailTest::bodies_;

TEST_F(MailTest, CreateMailbox) {
    mail::MailBox box = getRandomMailbox();
    ASSERT_EQ(box.getSize(), 0);
}

TEST_F(MailTest, InsertInMailbox) {
    mail::MailBox box = getRandomMailbox();
    mail::Message msg = getRandomMessage();
    box.insertMessage(msg);
    ASSERT_EQ(box.getSize(), 1);
    ASSERT_TRUE(box.getMessage(0).compare(msg));
    ASSERT_NE(&msg, &box.getMessage(0));
}

TEST_F(MailTest, SaveAndLoadBox) {
    mail::MailBox box = getRandomMailbox();
    box.insertMessages(getRandomMessages());
    ASSERT_TRUE(box.saveBox("save_load_test.dat"));
    mail::MailBox box_loaded = mail::loadBox("save_load_test.dat");
    ASSERT_EQ(box.getSize(), box_loaded.getSize());
    ASSERT_TRUE(box.getOwner().compare(box_loaded.getOwner()) == 0);
    for(int i = 0; i < box.getSize(); i++) {
        ASSERT_TRUE(box.getMessage(i).compare(box_loaded.getMessage(i)));
    }
}