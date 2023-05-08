#pragma once

#include <QDialog>
#include <QString>

#include <deque>
#include <memory>

#include <memory>

namespace Ui {
class GUIMessage;
}

class GUIMessage : public QDialog
{
  Q_OBJECT

public:
  explicit GUIMessage(QWidget *parent = nullptr);
  ~GUIMessage();

  void clearMessages();

  // shows the error message or if there is already a message shown
  // it is shown after current message is closed
  void showWarning(QString heading, QString message);
  void showError(QString heading, QString message);

protected:
  virtual void accept();

private:

  struct Message
  {
    QString title;
    QString heading;
    QString text;
  };

  // either shows the message or ads it to queue
  void addMessage(std::shared_ptr<Message> message);

  void showMessage(Message message);

  Ui::GUIMessage *ui_;

  std::deque<std::shared_ptr<Message>> waiting_;
};
