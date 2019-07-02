#include "contactlistitem.h"

#include "participantinterface.h"

#include <QDir>
#include <QDebug>

ContactListItem::ContactListItem(QString name, QString username, QString ip):
  name_(name),
  username_(username),
  ip_(ip),
  interface_(nullptr)
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

  switchButtonIcon(QDir::currentPath() + "/icons/call.svg");

  layout_->addWidget(callButton_, 0, 1);
  QObject::connect(callButton_, SIGNAL(clicked()), this, SLOT(call()));
  callButton_->setObjectName("CallButton");

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


void ContactListItem::setActive()
{
  callButton_->hide();
  setDisabled(true);
}


void ContactListItem::setPlusOne()
{
  callButton_->show();
  switchButtonIcon(QDir::currentPath() + "/icons/plus.svg");
}

void ContactListItem::setInactive()
{
  callButton_->show();
  switchButtonIcon(QDir::currentPath() + "/icons/call.svg");
  setDisabled(false);
}


void ContactListItem::call()
{
  Q_ASSERT(interface_);
  interface_->callToParticipant(name_, username_, ip_);
  //setActive();
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

void ContactListItem::mouseDoubleClickEvent(QMouseEvent *e)
{
  QWidget::mouseDoubleClickEvent(e);
  call();
}


void ContactListItem::switchButtonIcon(QString iconLocation)
{
  QPixmap pixmap(iconLocation);
  QIcon ButtonIcon(pixmap);
  callButton_->setIcon(ButtonIcon);
}
