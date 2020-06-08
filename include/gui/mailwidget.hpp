#ifndef MAILWIDGET_HPP
#define MAILWIDGET_HPP

#include <QWidget>
#include <mail/mail.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class MailWidget; }
QT_END_NAMESPACE

/**
 * This widget is used to show and create a mail::Message.
*/
class MailWidget : public QWidget {
    Q_OBJECT

public:

    /**
     * Default constructor, builds a window with a given parent, if specified.
     * 
     * By default the fields of this widget are editable.
     * 
     * @param parent window's parent, if specified the window will follow the parent's position and the 
     *               parent object will handle the closing operations.
    */
    MailWidget(QWidget *parent = nullptr);

    /**
     * Destructor, handles the correct deallocation of memory.
    */
    ~MailWidget();

    /**
     * Sets the field of this widget editable or not.
     * 
     * If false the form will be read-only.
     * 
     * @param editable true if editable, false if read-only
    */
    void setEditable(bool editable);

    /**
     * Sets the date to the moment where this method is called and sets the field
     * to read-only.
    */
    void lockDate();

    /**
     * Sets the "From" field and sets the field to read-only.
    */
    void setFrom(const std::string &from);

    /**
     * Changes the message to show.
     * 
     * @param msg the new message to show
    */
    void showMessage(const mail::Message &msg);

    /**
     * @returns the message shown or edited by this widget.
    */
    mail::Message getMessage();

    /**
     * Removes all the data contained in the form.
    */
    void clearContent();

private:
    Ui::MailWidget *ui; /**< Ui interface */
};

#endif // MAILWIDGET_HPP