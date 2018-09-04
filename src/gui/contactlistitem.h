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

  // construct the widget
  void init(ParticipantInterface* interface);

public slots:
  void call();
  void chat();

private:

  QString name_;
  QString username_;
  QString ip_;

  QGridLayout* layout_;

  QLabel* nameLabel_;
  QPushButton* callButton_;
  QPushButton* chatButton_;

  ParticipantInterface* interface_;

};
