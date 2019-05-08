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
void printDebugObject(DebugType type, QObject* object,
                      QString context = "", QString description = "",
                      QStringList valueNames = {}, QStringList values = {});

// print debug information
void printDebug(DebugType type, QString className,
                QString context = "", QString description = "",
                QStringList valueNames = {}, QStringList values = {});
