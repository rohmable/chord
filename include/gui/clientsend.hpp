#ifndef CLIENTSEND_HPP
#define CLIENTSEND_HPP

#include <QDialog>
#include <memory>
#include <chord/client.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class ClientSend; }
QT_END_NAMESPACE

/**
 * Simple dialog used to send a new mail::Message.
 * 
 * Uses MailWidget to create the message.
*/
class ClientSend : public QDialog {
    Q_OBJECT

public:

    /**
     * Default constructor, builds a window with a given parent, if specified.
     * 
     * @param client the client to use to send the mail::Message
     * @param parent window's parent, if specified the window will follow the parent's position and the 
     *               parent object will handle the closing operations.
    */
    ClientSend(const std::shared_ptr<chord::Client> &client, QWidget *parent = nullptr);

    /**
     * Destructor, handles the correct deallocation of memory.
    */
    ~ClientSend();

public slots:

    /**
     * Sends the message when the "Send" button is clicked
    */
    void send();

private:
    Ui::ClientSend *ui; /**< Ui interface */
    std::shared_ptr<chord::Client> client_; /**< Pointer to the chord::Client */
};

#endif // CLIENTSEND_HPP