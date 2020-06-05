#ifndef CLIENTMAINWINDOW_HPP
#define CLIENTMAINWINDOW_HPP

#include <QMainWindow>
#include <chord/client.hpp>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class ClientMainWindow; }
QT_END_NAMESPACE

class ClientMainWindow : public QMainWindow {
    Q_OBJECT

public:
    ClientMainWindow(QWidget *parent = nullptr);
    ~ClientMainWindow();

    void login();

public slots:
    void mailChanged(int currentRow);
    void sendMessageClicked();
    void logOutClicked();
    void updateMailbox();
    void deleteMessage();

private:
    Ui::ClientMainWindow *ui;
    std::shared_ptr<chord::Client> client_;
};

#endif // CLIENTMAINWINDOW_HPP