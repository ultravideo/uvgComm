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

  void sort();

public slots:

  void deleteListItem();

  void showContextMenu(const QPoint& pos);

  void addContact(ParticipantInterface* interface,
                  QString name, QString username, QString address);
signals:
  void callTo(QString name, QString username, QString address);
  void openChatWith(QString name, QString username, QString address);

private:

  void removeContact(int index);

  // called whenever a contact is added
  void writeListToSettings();

  // list must be initialized before this
  void addContactToList(ParticipantInterface* interface,
                        QString name, QString username, QString address);

  void addToWidgetList(ContactListItem* cItem);

  // return -1 if it does not exist, otherwise returns the index
  int doesAddressExist(QString address);

  QAction *deleteAction_;
  // = new QAction("Reset",this);
  //ui->taskList->addAction(m_clientReset);
  //QObject::connect(m_clientReset, SIGNAL(triggered()), this, SLOT(resetButton()));


  QListWidget* list_;

  QList<ContactListItem*> items_;
};
