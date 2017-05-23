#pragma once

#include <QListWidget>
#include <QObject>

class ContactList : public QObject
{
  Q_OBJECT
public:
  ContactList();

  void initializeList(QListWidget *list);

signals:
  void callTo(QString name, QString username, QString address);
  void openChatWith(QString name, QString username, QString address);

  void addContact(QString name, QString username, QString address);

private:

  // called whenever a contact is added
  void writeListToSettings();

  QListWidget* list_;

  struct Contact
  {
    QString username;
    QString ip;
  };

  std::map<QString, Contact> contacts_;
};
