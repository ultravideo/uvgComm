#pragma once
#include <QList>
#include <QString>
#include <QObject>
#include <stdint.h>

// A module for functions used multiple times across the whole program.
// Try to keep printing of same information consequatively to a minimum and
// instead try to combine the string to more information rich statement.

// TODO: Implement partial printing of information.

// TODO use _sleep?
void qSleep(int ms);

// generates a random string of length.
// TODO: Not yet cryptographically secure, but should be
QString generateRandomString(uint32_t length);


// DEBUG_NORMAL: for one time informational debug printing.
// DEBUG_IMPORTANT: for important one time events such as receiving a message
//                  or creating a session.
// DEBUG_WARNING: for events that may lead to problems in future.
// DEBUG_ERROR: for events that will cause problems for the functionality of the Kvazzup.
// DEBUG_PEER_ERROR: for errors in behavior of entities that are not us.
//                   Most commonly a protocol error.
// DEBUG_PROGRAM_ERROR: for events which are impossible and are cause by bugs.
// DEBUG_PROGRAM_WARNING: for events which are impossible, but don't affect the
//                       functionality of Kvazzup.

enum DebugType{DEBUG_NORMAL, DEBUG_IMPORTANT, DEBUG_ERROR, DEBUG_WARNING,
               DEBUG_PEER_ERROR, DEBUG_PROGRAM_ERROR, DEBUG_PROGRAM_WARNING};


void printNormalDebug(QObject* object, QString description = "",
                      QString valueName = "", QString value = "");

void printPErrorDebug(QObject* object, QString description = "",
                      QString valueName  = "", QString value = "");

void printUnimplemented(QObject* object, QString whatIsNotImplemented);

// Print debug information with custom class name. Use this and getname with filters.
// context is a general context that makes it easier to link different prints to one another.
// TODO: The order of parameters would be more logical/easier with the enums at the beginning.
// TODO: Fix different threads printing at the same time.
// TODO: Make into a template
void printDebug(DebugType type, QString className, QString description = "",
                QStringList valueNames = {}, QStringList values = {});


// use this if printing is inside class derived from QObject which is most classes in Kvazzup
void printDebug(DebugType type, QObject* object, QString description = "",
                QStringList valueNames = {}, QStringList values = {});




bool settingEnabled(QString parameter);
