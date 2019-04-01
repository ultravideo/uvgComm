#include "contactlist.h"

#include "contactlistitem.h"
#include "participantinterface.h"

#include <QSettings>
#include <QDebug>
#include <QMenu>

ContactList::ContactList()
{}

void ContactList::initializeList(QListWidget* list, ParticipantInterface* interface)
{
  Q_ASSERT(list);
  list_ = list;

  list_->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(list, SIGNAL(customContextMenuRequested(QPoint)),
          this, SLOT(showContextMenu(QPoint)));

  QSettings settings("contacts.local", QSettings::IniFormat);

  int size = settings.beginReadArray("contacts");
  qDebug() << "Reading contact list with" << size << "contacts";
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    QString name = settings.value("name").toString();
    QString username = settings.value("username").toString();
    QString address = settings.value("ip").toString();

    addContactToList(interface, name, username, address);
  }
  settings.endArray();
}

void ContactList::showContextMenu(const QPoint& pos)
{
  // Handle global position
  QPoint globalPos = list_->mapToGlobal(pos);

  // Create menu and insert some actions
  QMenu myMenu;
  myMenu.addAction("Delete", this, SLOT(deleteListItem()));

  // Show context menu at handling position
  myMenu.exec(globalPos);
}

void ContactList::deleteListItem()
{
  // If multiple selection is on, we need to erase all selected items
  for (int i = 0; i < list_->selectedItems().size(); ++i) {
    // Get curent item on selected row
    int row = list_->currentRow();
    QListWidgetItem *item = list_->takeItem(row);

    removeContact(row);
    delete item;
  }
}

void ContactList::addContact(ParticipantInterface* interface,
                             QString name, QString username, QString address)
{
  Q_ASSERT(!address.isEmpty());

  qDebug() << "Adding contact. Name:" << name << "username:" << username
         << "address:" << address << "index:" << items_.size();

  if(name == "")
    name = "Anonymous";
  if(username == "")
    username = "anonymous";
  if(address == "")
    return;

  int index = doesAddressExist(address);

  if(index != -1)
  {
    qDebug() << "User already exists at index:" << index;
    return;
  }

  addContactToList(interface, name, username, address);

  // not the fastest, just the simplest way to do this
  writeListToSettings();
}

void ContactList::writeListToSettings()
{
  qDebug() << "Writing contactList with" << items_.size() << "items to settings.";
  QSettings settings("contacts.local", QSettings::IniFormat);

  settings.beginWriteArray("contacts");
  int index = 0;
  for(auto contact : items_)
  {
    settings.setArrayIndex(index);
    ++index;
    settings.setValue("name", contact->getName());
    settings.setValue("username", contact->getUserName());
    settings.setValue("ip", contact->getAddress());
  }
  settings.endArray();
}

void ContactList::addContactToList(ParticipantInterface *interface,
                                   QString name, QString username, QString address)
{
  Q_ASSERT(list_);

  items_.push_back(new ContactListItem(name, username, address));
  items_.back()->init(interface);

  addToWidgetList(items_.back());
}

int ContactList::doesAddressExist(QString address)
{
  for(int i = 0; i < items_.size(); ++i)
  {
    if(items_.at(i)->getAddress() == address)
      return i;
  }
  return -1;
}

void ContactList::removeContact(int index)
{
  Q_ASSERT(index != -1 && index < items_.size());

  qDebug() << "Removing contact from index:" << index;

  if(index == -1  || index >= items_.size())
  {
    qWarning() << "WARNING: Tried to remove a nonexisting contact";
    return;
  }

  items_.erase(items_.begin() + index);

  {
    QSettings settings("contacts.local", QSettings::IniFormat);
    settings.remove("contacts");
  }
  writeListToSettings();
}

void ContactList::addToWidgetList(ContactListItem* cItem)
{
  // set list_ as a parent
  QListWidgetItem* item = new QListWidgetItem(list_);
  item->setSizeHint(QSize(150, 50));
  list_->addItem(item);
  list_->setItemWidget(item, cItem);
}
