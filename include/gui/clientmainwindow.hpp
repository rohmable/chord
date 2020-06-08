#ifndef CLIENTMAINWINDOW_HPP
#define CLIENTMAINWINDOW_HPP

#include <QMainWindow>
#include <chord/client.hpp>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class ClientMainWindow; }
QT_END_NAMESPACE

/**
 * Main window of the client.
 * 
 * From this window is possible to read the mail and access send/delete/refresh functions.
 * 
 * The window mantains a pointer to a chord::Client and uses it to interface with 
 * the Chord ring
*/
class ClientMainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * Default constructor, builds a window with a given parent, if specified.
     * 
     * @param parent window's parent, if specified the window will follow the parent's position and the 
     *               parent object will handle the closing operations.
    */
    ClientMainWindow(QWidget *parent = nullptr);

    /**
     * Destructor, handles the correct deallocation of memory.
    */
    ~ClientMainWindow();

    /**
     * Login function, spawns a ClientLogin window and creates a new chord::Client
     * to use.
    */
    void login();

public slots:
    /**
     * Called when a new mail::Message is selected.
     * 
     * Updates the data in the MailWidget showing the new selected message.
     * 
     * @param currentRow the selected row of the QTableWidget.
    */
    void mailChanged(int currentRow);

    /**
     * Shows a new QDialog to send a mail message, used when the "Send" button is clicked.
    */
    void sendMessageClicked();
    
    /**
     * Logs out the client and shows again the ClientLogin dialog.
     * 
     * Used when the "Log out" button is clicked.
    */
    void logOutClicked();

    /**
     * Refresh the QTableWidget asking the chord::Client to update the messages
    */
    void updateMailbox();

    /**
     * Deletes the selected message using the chord::Client.
    */
    void deleteMessage();

private:
    Ui::ClientMainWindow *ui; /**< Ui interface */
    std::shared_ptr<chord::Client> client_; /**< Pointer to the chord::Client */
};

#endif // CLIENTMAINWINDOW_HPP