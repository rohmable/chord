#ifndef MAIL_HPP
#define MAIL_HPP

#include <string>
#include <vector>
#include <chrono>
#include <ctime>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

namespace mail {
    /**
     * Models a mail message
    */
    struct Message {
        /**
         * Default constructor that builds an empty message, 
         * strings will be empty and the date will be set to the moment of creation.
         * 
         * Used mainly by serialization functions.
        */
        Message();
        
        /**
         * Builds a message with the specified fields.
         * 
         * If not passed as a parameter the date will be set to the moment of creation.
         * 
         * @param to_ receiver
         * @param from_ sender
         * @param subject_ subject of the message
         * @param body_ body of the message
         * @param date_ date of the message
        */
        Message(const std::string &to_, const std::string &from_, const std::string &subject_, const std::string &body_, std::time_t date_ = std::time(nullptr));

        std::string to, /**< Receiver */
                    from, /**< Sender */
                    subject,
                    body;
        std::time_t date;
        bool read; /**< If the message was read or not */

        /**
         * Used mainly for testing purposes
         * 
         * @param msg message to compare
         * @returns true if all the field of the message are equal to the one passed as reference.
         * 
        */
        bool compare(const Message &msg) const;

        /**
         * Method used to serialize the data structure.         
        */
        template<class Archive>
        void serialize(Archive &archive) {
            archive(to, from, subject, body, date);
        }
    };

    /**
     * Container for mail::Message.
     * 
     * Messages are associated to an owner (address) and a password.
    */
    class MailBox {
    public:
        /**
         * Default constructor. The owner will be an empty string and the password will be 0
         * 
         * Used mainly by serialization functions.
        */
        MailBox();

        /**
         * Builds a MailBox with a certain user and a password in form of a std::string.
         * 
         * The password will be automatically hashed.
         * 
         * @param owner the owner of the mailbox
         * @param psw the password of the mailbox
        */
        MailBox(const std::string &owner, const std::string &psw);

        /**
         * Builds a MailBox with a certain user and a pre-hashed password.
         * 
         * @param owner the owner of the mailbox
         * @param psw the password of the mailbox
        */
        MailBox(const std::string &owner, long long int psw);

        /**
         * Sets the owner of the MailBox.
         * 
         * This operation will invalidate the mailbox and the following operations
         * with chord::Node will fail if the password associated to this owner is not
         * set.
         * 
         * @param owner the owner of the mailbox
        */
        void setOwner(const std::string &owner);

        /**
         * @returns the owner of the mailbox
        */
        const std::string& getOwner() const;

        /**
         * Sets the password of the MailBox.
         * 
         * The password will be automatically hashed.
         * 
         * This operation will invalidate the mailbox and the following operations
         * with chord::Node will fail if the owner associated with this password is not
         * set.
         * 
         * @param psw the password of the mailbox
        */
        void setPassword(const std::string &psw);

        /**
         * Sets the password of the MailBox.
         * 
         * This operation will invalidate the mailbox and the following operations
         * with chord::Node will fail if the owner associated with this password is not
         * set.
         * 
         * @param psw the password of the mailbox
        */
        void setPassword(long long int psw);

        /**
         * @returns the password of this mailbox
        */
        long long int getPassword() const;

        /**
         * @returns the number of mail::Message in this mailbox
        */
        int getSize() const;

        /**
         * Equivalent to
         * \code{.cpp}
         * box.getSize() == 0
         * \endcode
         * 
         * @returns true if there are no mail::Message in this mailbox, false otherwise.
        */
        bool empty() const;

        /**
         * Removes all mail::Message from this mailbox.
         * 
         * Owner and password won't be cleared.
        */
        void clear();

        /**
         * @returns a reference to the mail::Message contained in this mailbox
        */
        const std::vector<Message>& getMessages() const;

        /**
         * @param i message index
         * @returns a reference to the i-th message
        */
        const Message& getMessage(int i) const;

        /**
         * Removes the i-th message of the mailbox.
         * 
         * @param i message index
        */
        bool removeMessage(int i);

        /**
         * Inserts a new message inside the box.
         * 
         * @param msg message to insert
        */
        void insertMessage(const Message &msg);

        /**
         * Inserts multiple messages inside the box.
         * 
         * @param msgs vector containing all messages to add to the box.
        */
        void insertMessages(const std::vector<Message> &msgs);

        /**
         * Saves a mailbox in a file with the given name.
         * 
         * Owner, password and messages will be saved.
         * 
         * @param filename name of the file to save mailbox contents
         * @returns true if the operation was successful, false otherwise
        */
        bool saveBox(const std::string &filename) const;

        /**
         * Hashes a string using SHA-1 algorithm.
         * 
         * @param str string to hash
         * @returns a number that represent the hashed string
        */
        static long long int hashPsw(const std::string &str);

        /**
         * @param filename to load the mailbox from
         * @returns the loaded mailbox
        */
        static MailBox loadBox(const std::string &filename);

        /**
         * Method used to serialize the data structure.
         * 
         * Owner, password and messages will be serialized.
        */
        template<class Archive>
        void serialize(Archive &archive) {
            archive(owner_, psw_, box_);
        }

    private:
        std::string owner_; /**< Mailbox owner */
        long long int psw_; /**< Mailbox password */
        std::vector<Message> box_; /**< mail::Message container */
    };
}

#endif // MAIL_HPP