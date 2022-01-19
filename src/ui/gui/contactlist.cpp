#include "contactlist.h"

#include "contactlistitem.h"
#include "participantinterface.h"

#include "common.h"
#include "settingskeys.h"
#include "logger.h"

#include <QSettings>
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

  QSettings settings(contactsFile, settingsFileFormat);

  int size = settings.beginReadArray(SettingsKey::contactList);

  Logger::getLogger()->printNormal(this, "Reading contact list", {"# of contacts"}, 
                                   {QString::number(size)});

  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    QString name = settings.value("name").toString();
    QString username = settings.value("username").toString();
    QString address = settings.value("ip").toString();

    addContactToList(interface, name, username, address);
  }
  settings.endArray();
}


void ContactList::turnAllItemsToPlus()
{
  for (auto& item : items_)
  {
    item->setPlusOne();
  }
}


void ContactList::setAccessibleAll()
{
  for (auto& item : items_)
  {
    item->setAccesssible();
  }
}


void ContactList::setAccessible(uint32_t sessionID)
{
  for (auto& item : items_)
  {
    if (item->getActiveSessionID() == sessionID)
    {
      item->setAccesssible();
      return;
    }
  }
}


void ContactList::setInaccessibleAll()
{
  for (auto& item : items_)
  {
    // 0 means that this cannot be restored by a call cancellation
    item->SetInaccessible(0);
  }
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

  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Adding contact",
                                  {"Name", "username", "address", "index"},
                                  {name, username, address, QString::number(items_.size())});

  if(name == "")
    name = "Anonymous";
  if(username == "")
    username = "anonymous";
  if(address == "")
  {
    Logger::getLogger()->printNormal(this, "Please input address!");
    return;
  }

  int index = doesAddressExist(username, address);

  if(index != -1)
  {
    Logger::getLogger()->printWarning(this, "Found previous contact with same name",
                                      "Index", QString::number(index));
    return;
  }

  addContactToList(interface, name, username, address);

  // not the fastest, just the simplest way to do this
  writeListToSettings();
}


void ContactList::writeListToSettings()
{
  Logger::getLogger()->printNormal(this, "Add/remove contact", "Items in list",
                                   QString::number(items_.size()));

  QSettings settings(contactsFile, settingsFileFormat);

  settings.beginWriteArray(SettingsKey::contactList);
  int index = 0;
  for(auto& contact : items_)
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


int ContactList::doesAddressExist(QString username, QString address)
{
  for(int i = 0; i < items_.size(); ++i)
  {
    if(items_.at(i)->getAddress() == address
       && items_.at(i)->getUserName() == username)
      return i;
  }
  return -1;
}

void ContactList::removeContact(int index)
{
  Q_ASSERT(index != -1 && index < items_.size());

  Logger::getLogger()->printNormal(this, "Removing contact",
                                   "Index", QString::number(index));

  if(index == -1  || index >= items_.size())
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, this,
                                    "Tried to remove a nonexisting contact");
    return;
  }

  items_.erase(items_.begin() + index);

  // delete the whole list and write it again
  QSettings settings(contactsFile, settingsFileFormat);
  settings.remove(SettingsKey::contactList);

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
