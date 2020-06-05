#ifndef MAILWIDGET_HPP
#define MAILWIDGET_HPP

#include <QWidget>
#include <mail/mail.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MailWidget; }
QT_END_NAMESPACE

class MailWidget : public QWidget {
    Q_OBJECT

public:
    MailWidget(QWidget *parent = nullptr);
    MailWidget(const mail::Message &msg, QWidget *parent = nullptr);
    ~MailWidget();

    void setEditable(bool editable);
    void lockDate();
    void setFrom(const std::string &from);
    void showMessage(const mail::Message &msg);
    mail::Message getMessage();
    void clearContent();

private:
    Ui::MailWidget *ui;
};

#endif // MAILWIDGET_HPP