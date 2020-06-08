#include <chord/client.hpp>
#include <mail/mail.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <iostream>
#include <fstream>
#include <random>
#include <cstdlib>

std::vector<std::string> users,
                         passwords,
                         subjects,
                         bodies;


mail::Message getRandomMessage(const mail::MailBox &from, const mail::MailBox &to) {
    static std::random_device dev;
    static std::mt19937 rng(dev());
    static std::uniform_int_distribution<std::mt19937::result_type> dist_subjects(0, subjects.size() - 1);
    static std::uniform_int_distribution<std::mt19937::result_type> dist_bodies(0, bodies.size() - 1);

    return mail::Message(
        to.getOwner(),
        from.getOwner(),
        subjects[dist_subjects(rng)],
        bodies[dist_bodies(rng)]
    );
}

int main() {
    std::string path,
                conn_string;
    std::cout << "Connection string: ";
    std::cin >> conn_string;
    chord::Client cl(conn_string);
    std::cout << "Mock file: ";
    std::cin >> path;
    {
        std::ifstream is(path);
        cereal::JSONInputArchive archive(is);
        archive(cereal::make_nvp("users", users));
        archive(cereal::make_nvp("passwords", passwords));
        archive(cereal::make_nvp("subjects", subjects));
        archive(cereal::make_nvp("bodies", bodies));
    }
    
    for(int i = 0; i < MIN(users.size(), passwords.size()) ; i++) {
        cl.accountRegister(users[i], passwords[i]);
    }

    return EXIT_SUCCESS;
}