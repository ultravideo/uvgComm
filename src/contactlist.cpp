#include "contactlist.h"

#include "contactlistitem.h"

#include <QSettings>
#include <QDebug>

ContactList::ContactList()
{

}

void ContactList::initializeList(QListWidget* list)
{
  list_ = list;

  QSettings settings;

  int size = settings.beginReadArray("contacts");
  qDebug() << "Reading contact list with" << size << "contacts";
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    QString name = settings.value("name").toString();
    QString username = settings.value("userName").toString();
    QString address = settings.value("ip").toString();

    addContactToList(name, username, address);
  }
  settings.endArray();
}

void ContactList::addContact(QString name, QString username, QString address)
{
  Q_ASSERT(!address.isEmpty());

  if(name == "")
    name = "Anonymous";
  if(username == "")
    username = "anonymous";
  if(address == "")
    return;

  addContactToList(name, username, address);

  // not the fastest, just the simplest way to do this
  writeListToSettings();
}

void ContactList::writeListToSettings()
{
  QSettings settings;

  settings.beginWriteArray("contacts");
  int index = 0;
  for(auto contact : items_)
  {
    settings.setArrayIndex(index);
    ++index;
    settings.setValue("Name", contact->getName());
    settings.setValue("username", contact->getUserName());
    settings.setValue("ip", contact->getAddress());
  }
  settings.endArray();
}

void ContactList::addContactToList(QString name, QString username, QString address)
{
  Q_ASSERT(list_);

  items_.push_back(new ContactListItem(name, username, address));
  items_.back()->init();

  // set list_ as a parent
  QListWidgetItem* item = new QListWidgetItem(list_);

  list_->addItem(item);
  list_->setItemWidget(item, items_.back());
}
