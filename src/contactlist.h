#pragma once


#include <QListWidget>

class ContactList
{
public:
  ContactList();

  void initializeList(QListWidget *list);
  void writeListToSettings();

signals:

  void callTo(QString name, QString username, QString address);
  void openChatWith(QString name, QString username, QString address);

public slots:

  void addContact(QString name, QString username, QString address);

private:

  QListWidget* list_;

  struct Contact
  {
    QString username;
    QString ip;
  };

  std::map<QString, Contact> contacts_;
};
