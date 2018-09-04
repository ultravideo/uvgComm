#pragma once
#include <QList>
#include <QString>
#include <stdint.h>

// A module for functions used multiple times across the whole program

// TODO use _sleep?
void qSleep(int ms);

QString generateRandomString(uint32_t length);
