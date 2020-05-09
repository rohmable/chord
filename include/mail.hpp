#ifndef MAIL_HPP
#define MAIL_HPP

#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <nlohmann/json.hpp>

namespace mail {
    struct Message {
        Message();
        Message(const std::string &to_, const std::string &from_, const std::string &subject_, const std::string &body_, std::time_t date_ = std::time(nullptr));

        std::string to,
                    from,
                    subject,
                    body;
        std::time_t date;
        bool read;
        bool compare(const Message &msg) const;
    };

    void to_json(nlohmann::json &j, const Message &msg);
    void from_json(const nlohmann::json &j, Message &msg);

    std::ostream& operator<<(std::ostream& os, const Message &msg);

    class MailBox {
    public:
        MailBox(const std::string &owner = "");
        void setOwner(const std::string &owner);
        const std::string& getOwner() const;
        int getSize() const;
        const std::vector<Message>& getMessages() const;
        const Message& getMessage(int i) const;
        void insertMessage(const Message &msg);
        void insertMessages(const std::vector<Message> &msgs);
        bool saveBox(const std::string &filename) const;
        bool loadBox(const std::string &filename);

    private:
        std::string owner_;
        std::vector<Message> box_;
    };

    MailBox loadBox(const std::string &filename);
    void to_json(nlohmann::json &j, const MailBox &box);
    void from_json(const nlohmann::json &j, MailBox &box);
}

#endif // MAIL_HPP