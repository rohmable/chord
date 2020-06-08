#include <chord/server.hpp>
#include <cstdlib>
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>

bool run = true;

void sigint_handler(int signal) {
    run = false;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler);
    chord::NodeInfo n, entry_point;
    std::cout << "Insert the node address: ";
    std::cin >> n.address;
    std::cout << "Insert the node port: ";
    std::cin >> n.port;
    chord::Node node(n.address, n.port);
    std::cout << "Insert the entry point address: ";
    std::cin >> entry_point.address;
    std::cout << "Insert the entry point port: ";
    std::cin >> entry_point.port;
    entry_point.id = chord::hashString(entry_point.conn_string());
    node.join(entry_point);
    while(run) {
        auto timenow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::cout << "\033[2J\033[1;1H";
        std::cout << ctime(&timenow) << std::endl;
        std::cout << node.getInfo().id << " @ " << node.getInfo().conn_string() << " managing " << node.numMailbox() << " mailboxes" << std::endl;
        std::this_thread::sleep_for(std::chrono::duration(std::chrono::milliseconds(1000)));
    }
    return EXIT_SUCCESS;
}