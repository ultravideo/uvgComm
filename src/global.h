#pragma once

// A place for program defines and constants. See common.h for common functions.

#include <QString>

const QString APPLICATIONNAME = "Kvazzup";

const QString LOG_FILE_NAME = "kvazzup.log";

// how often registrations are sent in seconds
const int REGISTER_INTERVAL = 600;

const unsigned int INVITE_TIMEOUT = 60000;

// this affects latency of audio. We have to wait until this much audio
// has arrived before sending the packet. If packet is too small,
// we waste bandwidth.

// With a value of 100, one audio frame constitutes 10 ms of audio samples.
// It is recommended to keep the audio frame size relatively small to avoid 
// unnecessary latency.
#ifdef __linux__
// linux uses such large audio frames, that the buffers can't keep up with
// smaller packet sizes
// TODO: Make filter buffer sizes relative to this number so the latency stays constant
const uint16_t AUDIO_FRAMES_PER_SECOND = 50;
#else
const uint16_t AUDIO_FRAMES_PER_SECOND = 100;
#endif

const uint16_t MIN_ICE_PORT   = 23000;
const uint16_t MAX_ICE_PORT   = 24000;

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


struct SDPMediaParticipant
{
  bool videoEnabled;
  bool audioEnabled;
  QString name;
};
