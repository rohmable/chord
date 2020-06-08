#include <gui/mailwidget.hpp>
#include "ui_mailwidget.h"
#include <ctime>

MailWidget::MailWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MailWidget) {
    
    ui->setupUi(this);
}

MailWidget::~MailWidget() {
    delete ui;
}

void MailWidget::setEditable(bool editable) {
    bool val = !editable;
    ui->from->setReadOnly(val);
    ui->to->setReadOnly(val);
    ui->subject->setReadOnly(val);
    ui->date->setReadOnly(val);
    ui->body->setReadOnly(val);
}

void MailWidget::lockDate() {
    ui->date->setReadOnly(true);
    time_t date = time(nullptr);
    ui->date->setText(QString(ctime(&date)));
}

void MailWidget::setFrom(const std::string &from) {
    ui->from->setReadOnly(true);
    ui->from->setText(QString::fromStdString(from));
}

void MailWidget::showMessage(const mail::Message &msg) {
    ui->from->setText(QString::fromStdString(msg.from));
    ui->to->setText(QString::fromStdString(msg.to));
    ui->subject->setText(QString::fromStdString(msg.subject));
    ui->date->setText(QString(ctime(&msg.date)));
    ui->body->setText(QString::fromStdString(msg.body));
}

mail::Message MailWidget::getMessage() {
    std::string from(ui->from->text().toStdString()),
                to(ui->to->text().toStdString()),
                subject(ui->subject->text().toStdString()),
                body(ui->body->toPlainText().toStdString());
    time_t date = time(nullptr);
    return {to, from, subject, body, date};
}

void MailWidget::clearContent() {
    ui->from->setText("");
    ui->to->setText("");
    ui->subject->setText("");
    ui->date->setText("");
    ui->body->setText("");
}