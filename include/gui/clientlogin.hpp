#ifndef CLIENTLOGIN_HPP
#define CLIENTLOGIN_HPP

#include <QDialog>
#include <memory>
#include <chord/client.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class ClientLogin; }
QT_END_NAMESPACE

class ClientLogin : public QDialog {
    Q_OBJECT

public:
    ClientLogin(QWidget *parent = nullptr);
    ~ClientLogin();
    bool isLogged();
    std::shared_ptr<chord::Client> getClient();

public slots:
    void okClicked();

private:
    Ui::ClientLogin *ui;
    std::shared_ptr<chord::Client> client_;
    bool logged_;
};

#endif // CLIENTLOGIN_HPP