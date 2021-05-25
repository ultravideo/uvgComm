#pragma once

#include <QString>

#include <stdint.h>

// A module for functions used multiple times across the whole program.
// Try to keep printing of same information consequatively to a minimum and
// instead try to combine the string to more information rich statement.

// TODO use _sleep?
void qSleep(int ms);

// generates a random string of length.
// TODO: Not yet cryptographically secure, but should be
QString generateRandomString(uint32_t length);

bool settingEnabled(QString key);

int settingValue(QString key);

QString settingString(QString key);

QString getLocalUsername();
