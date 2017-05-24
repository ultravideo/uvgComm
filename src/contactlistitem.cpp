#include "contactlistitem.h"

ContactListItem::ContactListItem(QString name, QString username, QString ip):
  name_(name),
  username_(username),
  ip_(ip)
{}

void ContactListItem::init()
{
  layout_ = new QHBoxLayout(this);
  setLayout(layout_);

  nameLabel_ = new QLabel(name_);
  layout_->addWidget(nameLabel_);

  callButton_ = new QPushButton("Call");
  layout_->addWidget(callButton_);

  chatButton_ = new QPushButton("Chat");
  layout_->addWidget(chatButton_);

  QObject::connect(callButton_, SIGNAL(clicked()), this, SLOT(call()));
  QObject::connect(chatButton_, SIGNAL(clicked()), this, SLOT(chat()));
}

void ContactListItem::call()
{
  emit startCall(name_, username_, ip_);
}

void ContactListItem::chat()
{
  emit startChat(name_, username_, ip_);
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
