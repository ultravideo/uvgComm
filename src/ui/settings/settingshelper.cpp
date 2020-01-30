#include "settingshelper.h"

#include "common.h"

#include <QMenu>
#include <QCheckBox>
#include <QDebug>


void saveCheckBox(const QString settingValue, QCheckBox* box, QSettings& settings)
{
  if(box->isChecked())
  {
    settings.setValue(settingValue,          "1");
  }
  else
  {
    settings.setValue(settingValue,          "0");
  }
}


void restoreCheckBox(const QString settingValue, QCheckBox* box, QSettings& settings)
{
  if(settings.value(settingValue).toString() == "1")
  {
    box->setChecked(true);
  }
  else if(settings.value(settingValue).toString() == "0")
  {
    box->setChecked(false);
  }
  else
  {
    qDebug() << "Settings, SettingsHelper : Corrupted value for checkbox in settings file for:"
             << settingValue << "!!!";
  }
}


void saveTextValue(const QString settingValue, const QString &text, QSettings& settings)
{
  if(text != "")
  {
    settings.setValue(settingValue,  text);
  }
}


bool checkMissingValues(QSettings& settings)
{
  QStringList list = settings.allKeys();

  bool foundEverything = true;
  for(auto& key : list)
  {
    if(settings.value(key).isNull() || settings.value(key) == "")
    {
      qDebug() << "Settings, SettingsHelper: MISSING SETTING FOR:" << key;
      foundEverything = false;
    }
  }
  return foundEverything;
}


void addFieldsToTable(QStringList& fields, QTableWidget* list)
{
  list->insertRow(list->rowCount());

  for (int i = 0; i < fields.size(); ++i)
  {
    QString field = fields.at(i);
    QTableWidgetItem* item = new QTableWidgetItem(field);

    list->setItem(list->rowCount() - 1, i, item);
  }
}


void listSettingsToGUI(QString filename, QString listName, QStringList values, QTableWidget* table)
{
  QSettings settings(filename, QSettings::IniFormat);

  int size = settings.beginReadArray(listName);

  qDebug() << "Settings, SettingsHelper : Reading" << listName << "from" << filename << "with"
           << size << "items to GUI.";

  for(int i = 0; i < size; ++i)
  {
    settings.setArrayIndex(i);

    QStringList list;
    for(int j = 0; j < values.size(); ++j)
    {
      list = list << settings.value(values.at(j)).toString();
    }

    addFieldsToTable(list, table);
  }
  settings.endArray();
}


void listGUIToSettings(QString filename, QString listName, QStringList values, QTableWidget* table)
{
  qDebug() << "Settings," << "SettingsHelper" << "Writing" << listName << "with"
           << table->rowCount() << "items to settings.";

  QSettings settings(filename, QSettings::IniFormat);

  settings.beginWriteArray(listName);
  for(int i = 0; i < table->rowCount(); ++i)
  {
    settings.setArrayIndex(i);

    for (int j = 0; j < values.size(); ++j)
    {
      settings.setValue(values.at(j), table->item(i,j)->text());
    }
  }
  settings.endArray();
}


void showContextMenu(const QPoint& pos, QTableWidget* table, QObject* processor,
                     QStringList actions, QStringList processSlots)
{
  qDebug() << "Settings," << "SettingsHelper" << ": Showing context menu for blocked users.";

  if(actions.size() != processSlots.size())
  {
    printDebug(DEBUG_PROGRAM_ERROR, "SettingsHelper",  "Different amounts of actions and slots",
                    {"Actions", "Slots"}, {QString::number(actions.size()), QString::number(processSlots.size())});
    return;
  }

  // Handle global position
  QPoint globalPos = table->mapToGlobal(pos);

  // Create menu and insert some actions
  QMenu myMenu;

  for (int i = 0; i < actions.size(); ++i) {
    myMenu.addAction(actions.at(i), processor, processSlots.at(i).toStdString().c_str());
  }

  // Show context menu at handling position
  myMenu.exec(globalPos);
}
