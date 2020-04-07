#pragma once

// A place for program defines and constants. See common.h for common functions.

#include <QString>

const QString APPLICATIONNAME = "Kvazzup";

const bool AEC_ENABLED = true;

// how often registrations are sent in seconds
const int REGISTER_INTERVAL = 600;

// this macro checks the condition and quits in debug mode and exits the current function in

#define CHECKERROR(condition, errorString, errorReturnValue) \
  Q_ASSERT(condition); \
  if(!condition) \
  { \
    qCritical().noSpace() << __FILE__ << ":" __LINE__ << "ERROR:" << errorstring;\
    return errorReturnValue; \
  }

#ifndef __cplusplus
#error A C++ compiler is required!
#endif

