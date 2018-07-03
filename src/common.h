#pragma once
#include <QList>
#include <QString>
#include <stdint.h>


// TODO use _sleep?
void qSleep(int ms);

QString generateRandomString(uint32_t length);
