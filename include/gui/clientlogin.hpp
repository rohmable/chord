#ifndef CLIENTLOGIN_HPP
#define CLIENTLOGIN_HPP

#include <QDialog>
#include <memory>
#include <chord/client.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class ClientLogin; }
QT_END_NAMESPACE

/**
 * Dialog used to log in a user.
 * 
 * The user can select the entry node to use for the login operation.
 * If the login operation is successful a new client connected with the
 * successor node for the mail::MailBox is created.
*/
class ClientLogin : public QDialog {
    Q_OBJECT

public:
    /**
     * Default constructor, builds a window with a given parent, if specified.
     * 
     * @param parent window's parent, if specified the window will follow the parent's position and the 
     *               parent object will handle the closing operations.
    */
    ClientLogin(QWidget *parent = nullptr);

    /**
     * Destructor, handles the correct deallocation of memory.
    */
    ~ClientLogin();

    /**
     * @returns true if the login operation was successful
    */
    bool isLogged();

    /**
     * @returns the new client associated with the logged user
    */
    std::shared_ptr<chord::Client> getClient();

public slots:

    /**
     * Logs in the user when he clicks the "Ok" button in the dialog
    */
    void okClicked();

private:
    Ui::ClientLogin *ui; /**< Ui interface */
    std::shared_ptr<chord::Client> client_; /**< Pointer to the chord::Client */
    bool logged_; /**< Flagged set to true when the login operation was successful */
};

#endif // CLIENTLOGIN_HPP