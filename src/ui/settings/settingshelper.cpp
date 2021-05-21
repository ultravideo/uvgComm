#include "settingshelper.h"

#include "common.h"
#include "settingskeys.h"

#include <QMenu>
#include <QCheckBox>
#include <QComboBox>


// used internally in this module
bool checkMissingValues(QSettings& settings);


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
    printDebug(DEBUG_ERROR, "Settings Helper", "Corrupted value for checkbox in settings file",
               {"Key"}, {settingValue});
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
      printDebug(DEBUG_ERROR, "Settings Helper", "Missing setting found", {"Key"}, {key});
      foundEverything = false;
    }
  }

  return foundEverything;
}


bool checkSettingsList(QSettings& settings, const QStringList& keys)
{
  bool everythingPresent = checkMissingValues(settings);

  for (auto& need : keys)
  {
    if(!settings.contains(need))
    {
      printDebug(DEBUG_WARNING, "Settings Helper",
                 "Found missing setting. Resetting video settings", {"Missing key"}, {need});
      everythingPresent = false;
    }
  }

  return everythingPresent;
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
  QSettings settings(filename, settingsFileFormat);

  int size = settings.beginReadArray(listName);

  printDebug(DEBUG_NORMAL, "Settings Helper", "Reading list from settings", {"File", "List name", "Items"},
             {filename, listName, QString::number(size)});


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
  printDebug(DEBUG_NORMAL, "Settings Helper", "Writing list from GUI to settings", {"File", "List name", "Table items"},
             {filename, listName, QString::number(table->rowCount())});

  QSettings settings(filename, settingsFileFormat);

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
  printDebug(DEBUG_NORMAL, "Settings Helper", "Showing context menu.");

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

void restoreComboBoxValue(QString key, QComboBox* box,
                          QString defaultValue, QSettings& settings)
{
  int index = box->findText(settings.value(key).toString());
  if(index != -1)
  {
    box->setCurrentIndex(index);
  }
  else
  {
    box->setCurrentText(defaultValue);
  }
}


int roundToNumber(int value, int roundingNumber)
{
  int roundedValue = static_cast<int>((value + roundingNumber/2)/roundingNumber);
  roundedValue *= roundingNumber;
  return roundedValue;
}


QString getBitrateString(int bits)
{
  QString finalString = QString::number(bits);

  // not possible at the moment
  if (bits < 1000)
  {
    finalString = QString::number(bits) + " bit/s";
  }
  else if (1000 <= bits && bits < 1000000)
  {
    finalString = QString::number(bits/1000) + " kbit/s";
  }
  else
  {
    finalString = QString::number(bits/1000000) + "." +
                  QString::number((bits%1000000)/1000) + " Mbit/s";
  }

  return finalString;
}
