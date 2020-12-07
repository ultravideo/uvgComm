#include "guimessage.h"
#include "ui_guimessage.h"

#include "common.h"

GUIMessage::GUIMessage(QWidget *parent) :
    QDialog(parent),
    ui_(new Ui::GUIMessage)
{
  ui_->setupUi(this);
}

GUIMessage::~GUIMessage()
{
  delete ui_;
}


void GUIMessage::showError(QString heading, QString message)
{
  std::shared_ptr<Message> mesg = std::shared_ptr<Message>(new Message{"Error", heading, message});

  if (!isVisible())
  {
    // copies the data, pointer gets deleted
    setMessage(*mesg.get());
  }
  else
  {
    // copies the smart pointer
    waiting_.push_front(mesg);
  }
}


void GUIMessage::setMessage(Message message)
{
  setWindowTitle(message.title);
  ui_->heading->setText(message.heading);
  ui_->text->setText(message.text);

  show();
}

void GUIMessage::accept()
{
  printNormal(this, "Closing message");
  if (waiting_.size() != 0)
  {
    // take the oldest message
    std::shared_ptr<Message> mesg = waiting_.back();
    waiting_.pop_back();
    // copy the data from pointer and show it
    setMessage(*mesg.get());
  }
  else
  {
    QDialog::accept();
  }
}
