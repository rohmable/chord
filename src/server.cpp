#include <iostream>
#include <cstdlib>
#include <csignal>
#include <thread>
#include <chrono>
#include "chord.hpp"

bool run = true;

void handle_int(int param) {
    run = false;
    std::cout << "\rClosing server..." << std::endl;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_int);
    chord::Ring ring("cfg.json");
    while(run) {
        auto timenow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::cout << "\033[2J\033[1;1H" << ctime(&timenow) << std::endl
                  << ring << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    return EXIT_SUCCESS;
}