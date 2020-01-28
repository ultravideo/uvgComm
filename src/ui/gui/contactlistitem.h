#pragma once
#include <QPushButton>
#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>

//user to store data associated with a contact list entity

class ParticipantInterface;

class ContactListItem : public QWidget
{
  Q_OBJECT
public:
  ContactListItem(QString name, QString username, QString ip);

  // get data values
  QString getName();
  QString getUserName();
  QString getAddress();

  uint32_t getActiveSessionID()
  {
    return  activeSessionID_;
  }

  // construct the widget
  void init(ParticipantInterface* interface);

  void SetInaccessible(uint32_t sessionID);
  void setPlusOne();
  void setAccesssible();

public slots:
  void call();
  void chat();

protected:

  void mouseDoubleClickEvent(QMouseEvent *e);

private:

  void switchButtonIcon(QString iconLocation);

  QString name_;
  QString username_;
  QString address;

  QGridLayout* layout_;

  QLabel* nameLabel_;
  QPushButton* callButton_;
  QPushButton* chatButton_;

  ParticipantInterface* interface_;

  uint32_t activeSessionID_;
};
