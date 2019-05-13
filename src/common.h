#pragma once
#include <QList>
#include <QString>
#include <QObject>
#include <stdint.h>

// A module for functions used multiple times across the whole program

// TODO use _sleep?
void qSleep(int ms);

QString generateRandomString(uint32_t length);

enum DebugType{DEBUG_NORMAL, DEBUG_ERROR, DEBUG_WARNING, DEBUG_PEER_ERROR};


// use this if printing happens inside class derived from QObject which is most Qt-classes
void printDebug(DebugType type, QObject* object,
                QString context = "", QString description = "",
                QStringList valueNames = {}, QStringList values = {});

// print debug information
// DEBUG_NORMAL is for one time informational debug printing.
// DEBUG_ERROR is for events that should not be able to happen in Kvazzup and result in problems
// DEBUG_WARNING is for events that should not be able to happen in Kvazzup which do not result in problems
// DEBUG_PEER_ERROR is for events that are errors in behavior of entities that are not us.
void printDebug(DebugType type, QString className,
                QString context = "", QString description = "",
                QStringList valueNames = {}, QStringList values = {});

