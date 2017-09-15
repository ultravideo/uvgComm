#include "contactlistitem.h"

#include "participantinterface.h"

#include <QDir>

ContactListItem::ContactListItem(QString name, QString username, QString ip):
  name_(name),
  username_(username),
  ip_(ip),
  interface_(NULL)
{}

void ContactListItem::init(ParticipantInterface *interface)
{
  Q_ASSERT(interface);
  interface_ = interface;

  //layout_ = new QHBoxLayout(this);
  layout_ = new QGridLayout(this);
  QWidget::setLayout(layout_);

  nameLabel_ = new QLabel(name_);
  layout_->addWidget(nameLabel_, 0,0);

  callButton_ = new QPushButton();
  callButton_->setMaximumWidth(30);
  QPixmap pixmap(QDir::currentPath() + "/icons/call.svg");
  QIcon ButtonIcon(pixmap);
  callButton_->setIcon(ButtonIcon);
  layout_->addWidget(callButton_, 0, 1);
  QObject::connect(callButton_, SIGNAL(clicked()), this, SLOT(call()));

/*
  chatButton_ = new QPushButton();
  chatButton_->setMaximumWidth(30);
  QPixmap pixmap2(QDir::currentPath() + "/icons/chat.svg");
  QIcon ButtonIcon2(pixmap2);
  chatButton_->setIcon(ButtonIcon2);
  layout_->addWidget(chatButton_);
  QObject::connect(chatButton_, SIGNAL(clicked()), this, SLOT(chat()));
*/
}

void ContactListItem::call()
{
  Q_ASSERT(interface_);
  interface_->callToParticipant(name_, username_, ip_);
}

void ContactListItem::chat()
{
  Q_ASSERT(interface_);
  interface_->chatWithParticipant(name_, username_, ip_);
}

QString ContactListItem::getName()
{
  return name_;
}

QString ContactListItem::getUserName()
{
  return username_;
}

QString ContactListItem::getAddress()
{
  return ip_;
}
