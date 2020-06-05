#include <chord/server.hpp>
#include <cstdlib>
#include <chrono>
#include <ncurses.h>
#include <menu.h>

void print_nodes(WINDOW *window, chord::Ring &ring) {
    auto nodes = ring.getNodes();
    int idx = 2;
    for(auto node : ring.getNodes()) {
        if(node->isRunning()) {
            auto info = node->getInfo();
            mvwprintw(window, idx, 0, "%-20lld @ %-15s managing %d mailboxes", info.id, info.conn_string().c_str(), node->numMailbox());
            idx++;
        }
    }
    idx += 2;
    attron(COLOR_PAIR(1));
    for(auto error : ring.getErrors()) {
        mvwprintw(window, idx, 0, "%s", error.c_str());
        idx++;
    }
    attroff(COLOR_PAIR(1));
}

int main(int argc, char *argv[]) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, true);
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);
    timeout(1000);
    chord::Ring ring("cfg.json");
    ring.dot("ring.gv");
    auto nodes = ring.getNodes();
    while(true) {
        clear();
        auto timenow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        mvprintw(0, 0, "Chord server - Press q to exit");
        mvprintw(1, 0, "%s", ctime(&timenow));
        print_nodes(stdscr, ring);
        refresh();
        int ch = getch();
        if(ch != ERR && ch == 'q') break;
    }
    for(auto node : nodes) {
        clear();
        auto timenow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        mvprintw(0, 0, "Chord server - Press q to exit");
        mvprintw(1, 0, "%s", ctime(&timenow));
        print_nodes(stdscr, ring);
        mvprintw(LINES - 1, 0, "Stopping node %lld...", node->getInfo().id);
        node->Stop();
    }
    endwin();
    return EXIT_SUCCESS;
}