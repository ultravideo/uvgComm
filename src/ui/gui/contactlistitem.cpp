#include "contactlistitem.h"

#include "participantinterface.h"

#include <QDir>

ContactListItem::ContactListItem(QString name, QString username, QString ip):
  name_(name),
  username_(username),
  address(ip),
  interface_(nullptr),
  activeSessionID_(0)
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

  setToolTip(username_ + "@" + address);

  callButton_ = new QPushButton();
  callButton_->setMaximumWidth(30);

  switchButtonIcon(QDir::currentPath() + "/icons/start_call.svg");

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


void ContactListItem::SetInaccessible(uint32_t sessionID)
{
  callButton_->hide();
  setDisabled(true);
  activeSessionID_ = sessionID;
}


void ContactListItem::setPlusOne()
{
  //callButton_->show();
  switchButtonIcon(QDir::currentPath() + "/icons/add_to_call.svg");
}

void ContactListItem::setAccesssible()
{
  callButton_->show();
  switchButtonIcon(QDir::currentPath() + "/icons/start_call.svg");
  setDisabled(false);
  activeSessionID_ = 0;
}


void ContactListItem::call()
{
  Q_ASSERT(interface_);
  SetInaccessible(interface_->startINVITETransaction(name_, username_, address));
}

void ContactListItem::chat()
{
  Q_ASSERT(interface_);
  interface_->chatWithParticipant(name_, username_, address);
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
  return address;
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
