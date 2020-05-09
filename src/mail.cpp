#include "mail.hpp"
#include <iostream>
#include <fstream>

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

void mail::to_json(nlohmann::json &j, const Message &msg) {
    j = nlohmann::json{
        {"to", msg.to}, 
        {"from", msg.from}, 
        {"subject", msg.subject}, 
        {"body", msg.body}, 
        {"date", msg.date}, 
        {"read", msg.read}
    };
}

void mail::from_json(const nlohmann::json &j, Message &msg) {
    j.at("to").get_to(msg.to);
    j.at("from").get_to(msg.from);
    j.at("subject").get_to(msg.subject);
    j.at("body").get_to(msg.body);
    j.at("date").get_to(msg.date);
    j.at("read").get_to(msg.read);
}

std::ostream& mail::operator<<(std::ostream& os, const Message &msg) {
    os << "From: " << msg.from << std::endl
       << "To: " << msg.to << std::endl
       << "Sent: " << std::asctime(std::localtime(&msg.date))
       << "Subject: " << msg.subject << std::endl;
    return os;
}

mail::MailBox::MailBox(const std::string &owner)
    : owner_(owner)
    , box_() {}

void mail::MailBox::setOwner(const std::string &owner) {
    owner_.assign(owner.begin(), owner.end());
}

const std::string& mail::MailBox::getOwner() const { return owner_; }

int mail::MailBox::getSize() const { return box_.size(); }

const std::vector<mail::Message>& mail::MailBox::getMessages() const {
    return box_;
}

const mail::Message& mail::MailBox::getMessage(int i) const {
    return box_.at(i);
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
    std::ofstream f(filename);
    if (!f.is_open()) {
        return false;
    }
    nlohmann::json box = (*this);
    f << box;
    f.close();
    return true;
}

mail::MailBox mail::loadBox(const std::string &filename) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        return MailBox();
    }
    nlohmann::json box;
    f >> box;
    f.close();
    return box.get<mail::MailBox>();
}

void mail::to_json(nlohmann::json &j, const MailBox &box) {
    j = nlohmann::json{
        {"owner", box.getOwner()}, 
        {"box", box.getMessages()}
    };
}
    
void mail::from_json(const nlohmann::json &j, MailBox &box) {
    box.setOwner(j.at("owner").get<std::string>());
    for(auto &msg : j.at("box")) {
        box.insertMessage(msg.get<mail::Message>());
    }
}