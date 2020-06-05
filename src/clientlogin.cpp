#include <gui/clientlogin.hpp>
#include "ui_clientlogin.h"
#include <sstream>

ClientLogin::ClientLogin(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ClientLogin)
    , client_(nullptr)
    , logged_(false) {
    
    ui->setupUi(this);
    ui->node_offline->setVisible(false);
    ui->login_invalid->setVisible(false);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ClientLogin::okClicked);
}

ClientLogin::~ClientLogin() {
    delete ui;
}

bool ClientLogin::isLogged() { return logged_; }

std::shared_ptr<chord::Client> ClientLogin::getClient() { return client_; }

void ClientLogin::okClicked() {
    ui->node_offline->setVisible(false);
    ui->login_invalid->setVisible(false);
    QString ip = ui->ip_address->text(),
            address = ui->address->text(),
            password = ui->password->text();
    int port = ui->port->value();
    std::stringstream conn_string;
    conn_string << ip.toStdString() << ":" << port;
    try {
        client_.reset(new chord::Client(conn_string.str()));
    } catch (chord::NodeException &e) {
        ui->node_offline->setVisible(true);
        return;
    }
    try {
        if(ui->new_account->isChecked()) {
            client_->accountRegister(address.toStdString(), password.toStdString());    
        } else {
            client_->accountLogin(address.toStdString(), password.toStdString());
        }
        logged_ = true;
    } catch (chord::NodeException &e) {
        ui->login_invalid->setVisible(true);
        return;
    }
}