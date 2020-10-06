#pragma once

#include <QString>
#include <QSettings>
#include <QTableWidget>

class QCheckBox;


// simpler functions for checkbox management.
void restoreCheckBox(const QString settingValue, QCheckBox* box, QSettings& settings);
void saveCheckBox(const QString settingValue, QCheckBox* box, QSettings& settings);

void saveTextValue(const QString settingValue, const QString &text, QSettings& settings);

bool checkMissingValues(QSettings& settings);

void addFieldsToTable(QStringList& fields, QTableWidget* list);

void listSettingsToGUI(QString filename, QString listName, QStringList values, QTableWidget* table);

void listGUIToSettings(QString filename, QString listName, QStringList values, QTableWidget* table);

void showContextMenu(const QPoint& pos, QTableWidget* table, QObject* processor,
                     QStringList actions, QStringList processSlots);

int roundToThousands(int value);

QString getBitrateString(int bits);
