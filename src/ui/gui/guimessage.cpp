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


void GUIMessage::showWarning(QString heading, QString message)
{
  std::shared_ptr<Message> mesg
      = std::shared_ptr<Message>(new Message{"Warning", heading, message});
  addMessage(mesg);
}


void GUIMessage::showError(QString heading, QString message)
{
  std::shared_ptr<Message> mesg
      = std::shared_ptr<Message>(new Message{"Error", heading, message});
  addMessage(mesg);
}


void GUIMessage::addMessage(std::shared_ptr<Message> message)
{
  if (!isVisible())
  {
    // copies the data, pointer gets deleted
    showMessage(*message.get());
  }
  else
  {
    // copies the smart pointer
    waiting_.push_front(message);
  }
}


void GUIMessage::showMessage(Message message)
{
  setWindowTitle(message.title);
  ui_->heading->setText(message.heading);
  ui_->text->setText(message.text);

  show();
}


void GUIMessage::accept()
{
  printNormal(this, "Closing message",
              {"Waiting messages"}, {QString::number(waiting_.size())});

  if (!waiting_.empty())
  {
    // take the oldest message
    std::shared_ptr<Message> mesg = waiting_.back();
    waiting_.pop_back();
    // copy the data from pointer and show it
    showMessage(*mesg.get());
  }
  else
  {
    QDialog::accept();
  }
}
