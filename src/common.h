#pragma once
#include <QList>
#include <QString>
#include <QObject>
#include <stdint.h>

// A module for functions used multiple times across the whole program

// TODO use _sleep?
void qSleep(int ms);

// generates a random string of length. TODO: Not yet secure, but should be
QString generateRandomString(uint32_t length);


// DEBUG_NORMAL is for one time informational debug printing.
// DEBUG_WARNING is for events that should not be able to happen in Kvazzup which do not cause problems.
// DEBUG_ERROR is for events that should not be able to happen in Kvazzup and results in problems.
// DEBUG_PEER_ERROR is for events that are errors in behavior of entities that are not us.
enum DebugType{DEBUG_NORMAL, DEBUG_ERROR, DEBUG_WARNING, DEBUG_PEER_ERROR};


// Print debug information with custom class name. Use this and getname with filters.
// context is a general context that makes it easier to link different prints to one another.
void printDebug(DebugType type, QString className,
                QString context = "", QString description = "",
                QStringList valueNames = {}, QStringList values = {});


// use this if printing is inside class derived from QObject which is most Qt-classes
void printDebug(DebugType type, QObject* object,
                QString context = "", QString description = "",
                QStringList valueNames = {}, QStringList values = {});
