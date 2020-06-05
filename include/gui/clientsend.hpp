#ifndef CLIENTSEND_HPP
#define CLIENTSEND_HPP

#include <QDialog>
#include <memory>
#include <chord/client.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class ClientSend; }
QT_END_NAMESPACE

class ClientSend : public QDialog {
    Q_OBJECT

public:
    ClientSend(const std::shared_ptr<chord::Client> &client, QWidget *parent = nullptr);
    ~ClientSend();

public slots:
    void send();

private:
    Ui::ClientSend *ui;
    std::shared_ptr<chord::Client> client_;
};

#endif // CLIENTSEND_HPP