#pragma once

#include <QListWidget>
#include <QObject>

class ContactListItem;
class ParticipantInterface;

class ContactList : public QObject
{
  Q_OBJECT
public:
  ContactList();

  void initializeList(QListWidget *list, ParticipantInterface* interface);

  void removeContact(QString address);
  void sort();

public slots:
  void addContact(ParticipantInterface* interface,
                  QString name, QString username, QString address);

signals:
  void callTo(QString name, QString username, QString address);
  void openChatWith(QString name, QString username, QString address);

private:

  // called whenever a contact is added
  void writeListToSettings();

  // list must be initialized before this
  void addContactToList(ParticipantInterface* interface,
                        QString name, QString username, QString address);

  void addToWidgetList(ContactListItem* cItem);

  // return -1 if it does not exist, otherwise returns the index
  int doesAddressExist(QString address);

  QListWidget* list_;

  QList<ContactListItem*> items_;
};
