#include "contactlist.h"

#include <QSettings>
#include <QDebug>

ContactList::ContactList()
{

}

void ContactList::initializeList(QListWidget* list)
{
  QSettings settings;

  int size = settings.beginReadArray("contacts");
  qDebug() << "Reading contact list with" << size << "contacts";
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    QString name = settings.value("name").toString();
    QString username = settings.value("userName").toString();
    QString ip = settings.value("ip").toString();

    contacts_[name] = Contact{username, ip};
    list->addItem(name);
  }
  settings.endArray();
}

void ContactList::writeListToSettings()
{
  QSettings settings;

  settings.beginWriteArray("contacts");
  int index = 0;
  for(auto contact : contacts_)
  {
    settings.setArrayIndex(index);
    ++index;
    settings.setValue("Name", contact.first);
    Contact c = contact.second;
    settings.setValue("username", c.username);
    settings.setValue("ip", c.ip);
  }
  settings.endArray();
}
