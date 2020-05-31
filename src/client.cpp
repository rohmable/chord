#include "chord.cpp"
#include "mail.hpp"
#include "chord.grpc.pb.h"
#include <ncurses.h>
#include <form.h>
#include <menu.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <utility>
#include <cstdlib>
#include <iterator>
#include <sstream>

class Client {
public:
    Client(const std::string &conn_string)
        : stub_(chord::NodeService::NewStub(grpc::CreateChannel(conn_string, grpc::InsecureChannelCredentials()))) 
        , box_(nullptr) {
            chord::PingRequest request;
            request.set_ping_n(1);
            auto[result, msg] = sendMessage<chord::PingRequest, chord::PingReply>(&request, &chord::NodeService::Stub::Ping);
            if(!result.ok() || msg.ping_n() != 1) {
                throw std::invalid_argument("The node is not online");
            }
        }

    ~Client() {
        if(box_) delete box_;
    }

    mail::MailBox& getBox() { return *box_; }

    bool auth(const std::string &address, const std::string &password, bool login = true) {
        if(box_) delete box_;
        box_ = new mail::MailBox(address, password);
        key_t key = chord::hashString(box_->getOwner());
        std::stringstream conn_string;
        if(login) {
            chord::QueryMailbox request;
            request.set_key(key);
            request.set_ttl(10);
            auto[result, reply] = sendMessage<chord::QueryMailbox, chord::QueryResult>(&request, &chord::NodeService::Stub::LookupMailbox);
            if(!reply.manager().cutoff()) {
                conn_string << reply.manager().ip() << ":" << reply.manager().port();
            } else {
                return false;
            }
        } else {
            chord::InsertMailboxMessage request;
            request.set_key(key);
            request.set_owner(box_->getOwner());
            request.set_password(box_->getPassword());
            request.set_ttl(10);
            auto[result, reply] = sendMessage<chord::InsertMailboxMessage, chord::NodeInfoMessage>(&request, &chord::NodeService::Stub::InsertMailbox);
            if(!reply.cutoff()) {
                conn_string << reply.ip() << ":" << reply.port();
            } else {
                return false;
            }
        }
        stub_.release();
        stub_ = chord::NodeService::NewStub(grpc::CreateChannel(conn_string.str(), grpc::InsecureChannelCredentials()));
        return true;
    }

    bool getMessages() {
        if(!box_) return false;
        chord::Authentication req;
        req.set_user(box_->getOwner());
        req.set_psw(box_->getPassword());
        auto[status, mailbox] = sendMessage<chord::Authentication, chord::Mailbox>(&req, &chord::NodeService::Stub::Receive);
        if(!status.ok() || !mailbox.valid()) return false;
        for(int i = 0; i < mailbox.messages_size(); i++) {
            const chord::MailboxMessage &msg = mailbox.messages().at(i);
            box_->insertMessage(msg.to(), msg.from(), msg.subject(), msg.body(), secondsToTimeT(msg.date()));
        }
        return true;
    }

private:
    std::unique_ptr<chord::NodeService::Stub> stub_;
    mail::MailBox *box_;

    time_t secondsToTimeT(google::protobuf::int64 secs) {
        using google::protobuf::util::TimeUtil;
        return TimeUtil::TimestampToTimeT(TimeUtil::SecondsToTimestamp(secs));
    }

    template<class T, class R>
    std::pair<grpc::Status, R> sendMessage(const T *request, grpc::Status (chord::NodeService::Stub::*rpc)(grpc_impl::ClientContext *, const T &, R *)) {
        T req(*request);
        R rep;
        grpc::ClientContext context;
        grpc::Status status = (stub_.get()->*rpc)(&context, req, &rep);
        return std::pair<grpc::Status, R>(status, rep);
    }
};

Client *cl = nullptr;

enum client_result {
    CLIENT_LOGIN,
    CLIENT_REGISTER,
    CLIENT_NOOP,
    CLIENT_EXIT
};

struct client_option {
    std::string title,
                subtitle;
    client_result val;
};

void print_in_middle(WINDOW *win, const std::string &string, int starty, int startx, int width);

client_result print_welcome_screen(bool error = false, const std::string &msg = "", bool auth_blocked = false) {
    std::vector<client_option> choices {
        {"Login", "Login to an existing account", CLIENT_LOGIN}, 
        {"Register", "Register a new account", CLIENT_REGISTER}, 
        {"Quit", "Quit the client", CLIENT_EXIT}
    };
    int width = 40, height = 10,
        startx = (COLS - width) / 2,
        starty = (LINES - height) / 2;

    ITEM **items = (ITEM **)calloc(choices.size() + 1, sizeof(ITEM *));
    for(int i = 0; i < choices.size(); i++) {
        items[i] = new_item(choices[i].title.c_str(), choices[i].subtitle.c_str());
        set_item_userptr(items[i], (void *) &choices[i].val);
    }
    items[choices.size()] = nullptr;

    clear();
    MENU *menu = new_menu(items);
    WINDOW *window = newwin(height, width, starty, startx);
    keypad(window, TRUE);
    set_menu_win(menu, window);
    set_menu_sub(menu, derwin(window, 6, 38, 3, 1));
    set_menu_mark(menu, " * ");
    box(window, 0, 0);
    print_in_middle(window, "Chord client", 1, 0, 40);
    mvwaddch(window, 2, 0, ACS_LTEE);
	mvwhline(window, 2, 1, ACS_HLINE, 38);
	mvwaddch(window, 2, 39, ACS_RTEE);
    if(error) {
        print_in_middle(window, msg, height - 2, 0, 40);
        if(auth_blocked) {
            set_current_item(menu, items[2]);
            item_opts_off(items[0], O_SELECTABLE);
            item_opts_off(items[1], O_SELECTABLE);
        }
    }
    refresh();
    post_menu(menu);
    wrefresh(window);

    bool run = true;

    while(run) {
        int ch = wgetch(window);
        switch(ch) {
        case KEY_DOWN:
            menu_driver(menu, REQ_NEXT_ITEM);
            break;
        case KEY_UP:
            menu_driver(menu, REQ_PREV_ITEM);
            break;
        case 10:    
            run = !((item_opts(current_item(menu)) & O_SELECTABLE) != 0);            
            break;
        }
        wrefresh(window);
    }

    ITEM *cur = current_item(menu);
    client_result res = *((client_result *) item_userptr(cur));
    for(int i = 0; i < choices.size(); i++) {
        free_item(items[i]);
        free_menu(menu);
    }
    delwin(window);
    return res;
}

bool auth(bool login) {
    clear();
    std::vector<client_option> choices {
        {login ? "Login" : "Register", "", login ? CLIENT_LOGIN : CLIENT_REGISTER},
        {"Back", "", CLIENT_NOOP}
    };
    FIELD *fields[] = {
        new_field(1, 40, 1, 9, 0, 0),
        new_field(1, 40, 3, 9, 0, 0),
        nullptr
    };
    ITEM **items = (ITEM **)calloc(choices.size() + 1, sizeof(ITEM *));
    for(int i = 0; i < choices.size(); i++) {
        items[i] = new_item(choices[i].title.c_str(), choices[i].subtitle.c_str());
        set_item_userptr(items[i], (void *) &choices[i].val);
    }
    items[choices.size()] = nullptr;
    MENU *menu = new_menu(items);
    menu_opts_off(menu, O_SHOWDESC);

    int ch;

    for(int i = 0; i < 2; i++) {
        set_field_back(fields[i], A_UNDERLINE);
        field_opts_off(fields[i], O_AUTOSKIP);
    }

    FORM *form = new_form(fields);
    int rows, cols;
    scale_form(form, &rows, &cols);
    int starty = (LINES - (rows + 6)) / 2,
        startx = (COLS - (cols + 6)) / 2;
    WINDOW *window = newwin(rows + 6, cols + 6, starty, startx);
    keypad(window, true);
    set_form_win(form, window);
    set_form_sub(form, derwin(window, rows, cols, 3, 3));
    box(window, 0, 0);
    if(login) {
        print_in_middle(window, "Login", 1, 0, cols + 4);
    } else {
        print_in_middle(window, "Register", 1, 0, cols + 4);
    }
    mvwaddch(window, 2, 0, ACS_LTEE);
	mvwhline(window, 2, 1, ACS_HLINE, cols + 4);
	mvwaddch(window, 2, cols + 5, ACS_RTEE);
    post_form(form);
    mvwprintw(window, 4, 1, "Address:");
    mvwprintw(window, 6, 1, "Password:");
    set_menu_win(menu, window);
    set_menu_sub(menu, derwin(window, 1, 15, rows + 4, cols - 10));
    set_menu_format(menu, 1, 2);
    post_menu(menu);
    set_current_field(form, fields[0]);
    
    wrefresh(window);
    refresh();

    bool run = true;

    while(run) {
        ch = wgetch(window);
        switch(ch) {
        case '\t':
            form_driver(form, REQ_NEXT_FIELD);
            form_driver(form, REQ_END_LINE);
            break;
        case KEY_RIGHT:
            menu_driver(menu, REQ_RIGHT_ITEM);
            break;
        case KEY_LEFT:
            menu_driver(menu, REQ_LEFT_ITEM);
            break;
        case 10:
            run = false;
            break;
        case KEY_BACKSPACE:
        case 127:
            form_driver(form, REQ_DEL_PREV);
            break;
        default:
            form_driver(form, ch);
            break;
        }
    }

    ITEM *cur = current_item(menu);
    client_result res = *((client_result *) item_userptr(cur));
    form_driver(form, REQ_NEXT_FIELD);
    form_driver(form, REQ_PREV_FIELD);
    move(LINES - 3, 2);
    std::string address(field_buffer(fields[0], 0)),
                password(field_buffer(fields[1], 0));
    bool authenticated = false;
    switch(res) {
    case CLIENT_LOGIN:
        print_in_middle(window, "Logging in...", 1, 0, cols + 4);
        wrefresh(window);
        authenticated = cl->auth(address, password, true);
        break;
    case CLIENT_REGISTER:
        print_in_middle(window, "Signing up...", 1, 0, cols + 4);
        wrefresh(window);
        authenticated = cl->auth(address, password, false);
        break;
    default:
        break;
    }

    unpost_menu(menu);
    free_menu(menu);
    free_item(items[0]);
    free_item(items[1]);
    unpost_form(form);
    free_form(form);
    free_field(fields[0]);
    free_field(fields[1]);
    delwin(window);
    return authenticated;
}

void show_incoming_messages() {
    cl->getMessages();
    auto box = cl->getBox();
    clear();
    print_in_middle(stdscr, std::string("Chord - ") + cl->getBox().getOwner(), 1, 0, COLS);
    mvchgat(1, 0, -1, A_REVERSE, 0, nullptr);
    if(box.empty()) {
        mvprintw(2, 0, "Your mailbox is empty");
    } else {
        mvprintw(2, 0, "You've got %d messages", box.getSize());
    }
    ITEM **mails = (ITEM **)calloc(box.getSize() + 1, sizeof(ITEM *));
    for(int i = 0; i < box.getSize(); i++) {
        auto msg = box.getMessage(i);
        mails[i] = new_item(msg.from.c_str(), msg.subject.c_str());
    }
    mails[box.getSize()] = nullptr;

    MENU *mails_menu = new_menu(mails);
    set_menu_format(mails_menu, LINES, 1);
    set_menu_mark(mails_menu, " * ");

    getch();
    refresh();
}

int main(int argc, char *argv[]) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, true);
    client_result welcome_selection;
    try {
        cl = new Client("0.0.0.0:50000");
        welcome_selection = print_welcome_screen();
    } catch (std::invalid_argument) {
        welcome_selection = print_welcome_screen(true, "You are offline", true);
    }
    bool result = false;
    while(welcome_selection != CLIENT_EXIT) {
        switch(welcome_selection) {
        case CLIENT_LOGIN:
            result = auth(true);
            break;
        case CLIENT_REGISTER:
            result = auth(false);
            break;
        case CLIENT_EXIT:
            break;
        }
        if(result) {
            show_incoming_messages();
        } else {
            welcome_selection = print_welcome_screen(true, "Failed to authenticate");
        }
    }
    endwin();
    delete cl;

    return EXIT_SUCCESS;
}

void print_in_middle(WINDOW *win, const std::string &string, int starty, int startx, int width) {
    int length, x, y;
    float temp;

    if(win == nullptr) {
        win = stdscr;
    }
    getyx(win, y, x);
    if(startx != 0) {
        x = startx;
    }
    if(starty != 0) {
        y = starty;
    }
    if(width == 0) {
        width = 80;
    }

    temp = (width - string.length()) / 2;
    x = startx + static_cast<int>(temp);
    mvwprintw(win, y, x, "%s", string.c_str());
    refresh();
}