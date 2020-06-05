#include <gui/clientmainwindow.hpp>
#include <gui/clientlogin.hpp>
#include <gui/clientsend.hpp>
#include "ui_clientmainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <ctime>

ClientMainWindow::ClientMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ClientMainWindow) {
    
    ui->setupUi(this);
    ui->mailwidget->setEditable(false);

    connect(ui->mailbox, &QTableWidget::currentCellChanged, this, &ClientMainWindow::mailChanged);
    connect(ui->newMessageButton, &QPushButton::clicked, this, &ClientMainWindow::sendMessageClicked);
    connect(ui->logOutButton, &QPushButton::clicked, this, &ClientMainWindow::logOutClicked);
    connect(ui->refreshButton, &QPushButton::clicked, this, &ClientMainWindow::updateMailbox);
    connect(ui->deleteButton, &QPushButton::clicked, this, &ClientMainWindow::deleteMessage);
}

ClientMainWindow::~ClientMainWindow() {
    delete ui;
}

void ClientMainWindow::login() {
    ClientLogin log(this);
    while(!log.isLogged()) {
        if(log.exec() == QDialog::Accepted) {
            client_ = log.getClient();
        } else {
            exit(EXIT_SUCCESS);
        }
    }
    updateMailbox();
}

void ClientMainWindow::mailChanged(int currentRow) {
    auto &msg = client_->getBox().getMessage(currentRow);
    ui->mailwidget->showMessage(msg);
}

void ClientMainWindow::sendMessageClicked() {
    ClientSend send(client_, this);
    send.exec();
}

void ClientMainWindow::logOutClicked() {
    client_.reset();
    ui->mailwidget->clearContent();
    ui->mailbox->blockSignals(true);
    ui->mailbox->setRowCount(0);
    ui->mailbox->blockSignals(false);
    login();
}

void ClientMainWindow::updateMailbox() {
    ui->mailbox->blockSignals(true);
    ui->mailbox->setRowCount(0);
    ui->mailbox->blockSignals(false);
    ui->mailwidget->clearContent();
    client_->getMessages();
    auto &box = client_->getBox();
    for(auto &msg : box.getMessages()) {
        int row = ui->mailbox->rowCount();
        ui->mailbox->insertRow(row);
        QTableWidgetItem *from = new QTableWidgetItem(QString::fromStdString(msg.from)),
                         *subject = new QTableWidgetItem(QString::fromStdString(msg.subject)),
                         *date = new QTableWidgetItem(ctime(&msg.date));
        ui->mailbox->setItem(row, 0, from);
        ui->mailbox->setItem(row, 1, subject);
        ui->mailbox->setItem(row, 2, date);
    }
    // Workaround to make the subject column stretch out
    int row = ui->mailbox->rowCount();
    ui->mailbox->insertRow(row);
    QTableWidgetItem *long_subject = new QTableWidgetItem("00000000000000000000000000000000000000000000000000000000");
    ui->mailbox->setItem(row, 1, long_subject);
    ui->mailbox->resizeColumnsToContents();
    ui->mailbox->horizontalHeader()->setStretchLastSection(true);
    ui->mailbox->removeRow(row);
}

void ClientMainWindow::deleteMessage() {
    int idx = ui->mailbox->currentRow();
    try {
        client_->remove(idx);
        updateMailbox();
    } catch (chord::NodeException &e) {
        QMessageBox::critical(this, "Error", "Couldn't delete this message");
    }

}

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    ClientMainWindow w;
    w.show();
    w.login();
    return a.exec();
}