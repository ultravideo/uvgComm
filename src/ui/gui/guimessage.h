#pragma once

#include <QDialog>

#include <deque>

namespace Ui {
class GUIMessage;
}

class GUIMessage : public QDialog
{
  Q_OBJECT

public:
  explicit GUIMessage(QWidget *parent = nullptr);
  ~GUIMessage();

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

  void setMessage(Message message);

  Ui::GUIMessage *ui_;

  std::deque<std::shared_ptr<Message>> waiting_;
};
