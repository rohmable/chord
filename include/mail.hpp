#ifndef MAIL_HPP
#define MAIL_HPP

#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

namespace mail {
    long long int hashPsw(const std::string &str);

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

        template<class Archive>
        void serialize(Archive &archive) {
            archive(to, from, subject, body, date);
        }
    };

    std::ostream& operator<<(std::ostream& os, const Message &msg);

    class MailBox {
    public:
        MailBox();
        MailBox(const std::string &owner, const std::string &psw);
        MailBox(const std::string &owner, long long int psw);
        void setOwner(const std::string &owner);
        const std::string& getOwner() const;
        void setPassword(const std::string &psw);
        void setPassword(long long int psw);
        long long int getPassword() const;
        int getSize() const;
        bool empty() const;
        const std::vector<Message>& getMessages() const;
        const Message& getMessage(int i) const;
        void insertMessage(const std::string &to, const std::string &from, const std::string &subject, const std::string &body, std::time_t date = std::time(nullptr));
        void insertMessage(const Message &msg);
        void insertMessages(const std::vector<Message> &msgs);
        bool saveBox(const std::string &filename) const;
        bool loadBox(const std::string &filename);

        template<class Archive>
        void serialize(Archive &archive) {
            archive(owner_, psw_, box_);
        }

    private:
        std::string owner_;
        long long int psw_;
        std::vector<Message> box_;
    };

    MailBox loadBox(const std::string &filename);
}

#endif // MAIL_HPP