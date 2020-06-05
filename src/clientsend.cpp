#include <gui/clientsend.hpp>
#include "ui_clientsend.h"

ClientSend::ClientSend(const std::shared_ptr<chord::Client> &client, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ClientSend)
    , client_(client) {
    
    ui->setupUi(this);
    ui->mailwidget->lockDate();
    ui->mailwidget->setFrom(client_->getBox().getOwner());
    ui->buttonBox->addButton("Send", QDialogButtonBox::ButtonRole::AcceptRole);

    connect(this, &QDialog::accepted, this, &ClientSend::send);
}

ClientSend::~ClientSend() {
    delete ui;
}

void ClientSend::send() {
    mail::Message msg = ui->mailwidget->getMessage();
    client_->send(msg);
}