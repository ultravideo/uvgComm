#pragma once
#include <QList>
#include <QString>
#include <QObject>
#include <stdint.h>

// A module for functions used multiple times across the whole program

// TODO use _sleep?
void qSleep(int ms);

// generates a random string of length.
// TODO: Not yet cryptographically secure, but should be
QString generateRandomString(uint32_t length);


// DEBUG_NORMAL is for one time informational debug printing.
// DEBUG_WARNING is for events that may lead to problems in future.
// DEBUG_ERROR is for events that will cause problems for the functionality of the Kvazzup.
// DEBUG_PEER_ERROR is for events that are errors in behavior of entities that are not us.
// DEBUG_PROGRAM_ERROR is for events which are impossible in Kvazzup and can only be cause by bugs.
// DEBUG_PROGRAM_WARNING is for events which are impossible, but don't affect the
//                       functionality of Kvazzup.

enum DebugType{DEBUG_NORMAL, DEBUG_ERROR, DEBUG_WARNING,
               DEBUG_PEER_ERROR, DEBUG_PROGRAM_ERROR, DEBUG_PROGRAM_WARNING};


enum DebugContext{DC_NO_CONTEXT,
                  DC_STARTUP,
                  DC_SHUTDOWN,
                  DC_SETTINGS,
                  DC_START_CALL,
                  DC_END_CALL,
                  DC_RINGING,
                  DC_ACCEPT,
                  DC_REJECT,
                  DC_NEGOTIATING,
                  DC_SIP_CONTENT,
                  DC_ADD_MEDIA,
                  DC_REMOVE_MEDIA,
                  DC_PROCESS_MEDIA,
                  DC_AUDIO,
                  DC_FULLSCREEN,
                  DC_DRAWING,
                  DC_CONTACTLIST,
                  DC_TCP,
                  DC_SEND_SIP,
                  DC_SEND_SIP_REQUEST,
                  DC_SEND_SIP_RESPONSE,
                  DC_RECEIVE_SIP,
                  DC_RECEIVE_SIP_REQUEST,
                  DC_RECEIVE_SIP_RESPONSE,
                 };


// Print debug information with custom class name. Use this and getname with filters.
// context is a general context that makes it easier to link different prints to one another.
void printDebug(DebugType type, QString className,
                DebugContext context, QString description = "",
                QStringList valueNames = {}, QStringList values = {});


// use this if printing is inside class derived from QObject which is most classes in Kvazzup
void printDebug(DebugType type, QObject* object,
                DebugContext context, QString description = "",
                QStringList valueNames = {}, QStringList values = {});
