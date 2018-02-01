#pragma once
#include <QList>
#include <QString>
#include <stdint.h>

const QString APPLICATIONNAME = "Kvazzup";

// TODO use _sleep?
void qSleep(int ms);

// TODO fix AEC bugs and faulty operating.
const bool AEC_ENABLED = false;
const bool DSHOW_ENABLED = true;
