#pragma once

#include <QString>
#include <QSettings>
#include <QTableWidget>

class QCheckBox;
class QComboBox;

// simpler functions for checkbox management.
void restoreCheckBox(const QString settingValue, QCheckBox* box, QSettings& settings);
void saveCheckBox(const QString settingValue, QCheckBox* box, QSettings& settings);

void saveTextValue(const QString settingValue, const QString &text, QSettings& settings);

bool checkSettingsList(QSettings& settings, const QStringList& keys);

void addFieldsToTable(QStringList& fields, QTableWidget* list);

void listSettingsToGUI(QString filename, QString listName, QStringList values, QTableWidget* table);

void listGUIToSettings(QString filename, QString listName, QStringList values, QTableWidget* table);

void showContextMenu(const QPoint& pos, QTableWidget* table, QObject* processor,
                     QStringList actions, QStringList processSlots);


void restoreComboBoxValue(QString key, QComboBox* box,
                          QString defaultValue, QSettings& settings);

// rounding number is usually something like 10, 100 or 1000
int roundToNumber(int value, int roundingNumber);

QString getBitrateString(int bits);

// returns -1 if there are no devices
int getMostMatchingDeviceID(QStringList devices, QString deviceName, int deviceID);

void convertFramerate(QString framerate, int32_t& out_numerator, int32_t& out_denominator);
