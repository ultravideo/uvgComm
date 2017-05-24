#pragma once

#include <QListWidget>
#include <QObject>

class ContactListItem;

class ContactList : public QObject
{
  Q_OBJECT
public:
  ContactList();

  void initializeList(QListWidget *list);

public slots:
  void addContact(QString name, QString username, QString address);

signals:
  void callTo(QString name, QString username, QString address);
  void openChatWith(QString name, QString username, QString address);

private:

  // called whenever a contact is added
  void writeListToSettings();

  // list must be initialized before this
  void addContactToList(QString name, QString username, QString address);

  // return -1 if it does not exist, otherwise returns the index
  int doesAddressExist(QString address);

  QListWidget* list_;

  QList<ContactListItem*> items_;
};
