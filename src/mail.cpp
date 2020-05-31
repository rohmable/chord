#include "mail.hpp"
#include <iostream>
#include <fstream>
#include <gcrypt.h>
#include <cereal/archives/binary.hpp>

long long int mail::hashPsw(const std::string &str) {
    unsigned int id_length = gcry_md_get_algo_dlen(GCRY_MD_SHA1);
    unsigned char *x = new unsigned char[id_length];
    gcry_md_hash_buffer(GCRY_MD_SHA1, x, str.c_str(), str.size());
    char *buffer = new char[id_length];
    std::string hash;
    for(int i = 0; i < id_length; i += sizeof(int)) {
        sprintf(buffer, "%d", x[i]);
        hash += buffer;
    }
    delete x;
    return std::stoll(hash);
}

mail::Message::Message()
    : to("")
    , from("")
    , subject("")
    , body("")
    , date(std::time(nullptr))
    , read(false) {}

mail::Message::Message(const std::string &to_, const std::string &from_, const std::string &subject_, const std::string &body_, std::time_t date_)
    : to(to_)
    , from(from_)
    , subject(subject_)
    , body(body_)
    , date(date_)
    , read(false) {}

bool mail::Message::compare(const mail::Message &msg) const {
    return (to.compare(msg.to) == 0) &&
           (from.compare(msg.from) == 0) &&
           (subject.compare(msg.subject) == 0) &&
           (body.compare(msg.body) == 0) &&
           (date == msg.date);
}

std::ostream& mail::operator<<(std::ostream& os, const Message &msg) {
    os << "From: " << msg.from << std::endl
       << "To: " << msg.to << std::endl
       << "Sent: " << std::asctime(std::localtime(&msg.date))
       << "Subject: " << msg.subject << std::endl;
    return os;
}

mail::MailBox::MailBox()
    : owner_("")
    , psw_(0)
    , box_() {}

mail::MailBox::MailBox(const std::string &owner, const std::string &psw)
    : owner_(owner)
    , psw_(hashPsw(psw))
    , box_() {}

mail::MailBox::MailBox(const std::string &owner, long long int psw)
    : owner_(owner)
    , psw_(psw)
    , box_() {}

void mail::MailBox::setOwner(const std::string &owner) {
    owner_.assign(owner.begin(), owner.end());
}

const std::string& mail::MailBox::getOwner() const { return owner_; }

void mail::MailBox::setPassword(const std::string &psw) { psw_ = hashPsw(psw); }

void mail::MailBox::setPassword(long long int psw) { psw_ = psw; }

long long int mail::MailBox::getPassword() const { return psw_; }

int mail::MailBox::getSize() const { return box_.size(); }

bool mail::MailBox::empty() const { return box_.empty(); }

const std::vector<mail::Message>& mail::MailBox::getMessages() const {
    return box_;
}

const mail::Message& mail::MailBox::getMessage(int i) const {
    return box_.at(i);
}

void mail::MailBox::insertMessage(const std::string &to, const std::string &from, const std::string &subject, const std::string &body, std::time_t date) {
    box_.emplace_back(to, from, subject, body, date);
}

void mail::MailBox::insertMessage(const mail::Message &msg) {
    box_.push_back(msg);
}

void mail::MailBox::insertMessages(const std::vector<Message> &msgs) {
    for(auto &msg : msgs) {
        box_.push_back(msg);
    }
}

bool mail::MailBox::saveBox(const std::string &filename) const {
    std::ofstream os(filename);
    if (!os.is_open()) {
        return false;
    }
    cereal::BinaryOutputArchive oarchive(os);
    oarchive(*this);
    return true;
}

bool mail::MailBox::loadBox(const std::string &filename) {
    std::ifstream is(filename);
    if(!is.is_open()) {
        return false;
    }
    cereal::BinaryInputArchive iarchive(is);
    iarchive(*this);
    return true;
}

mail::MailBox mail::loadBox(const std::string &filename) {
    std::ifstream is(filename);
    MailBox ret("", 0);
    if (is.is_open()) {
        cereal::BinaryInputArchive iarchive(is);
        iarchive(ret);
    }
    return ret;
}