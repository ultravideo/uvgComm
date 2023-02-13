#pragma once

#include <QString>

#include <stdint.h>

#include <memory>

struct MediaInfo;

// A module for functions used multiple times across the whole program.
// Try to keep printing of same information consequatively to a minimum and
// instead try to combine the string to more information rich statement.

// generates a random string of length.
QString generateRandomString(uint32_t length);

bool settingEnabled(QString key);
int settingValue(QString key);

QString settingString(QString key);

QString getLocalUsername();

bool getSendAttribute(const MediaInfo &media, bool local);
bool getReceiveAttribute(const MediaInfo &media, bool local);
