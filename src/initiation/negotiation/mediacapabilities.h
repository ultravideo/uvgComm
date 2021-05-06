#pragma once

#include "sdptypes.h"

#include <QList>

// Predefined payload types. Just defining number means that these are
// supported. Currently only pcm for audio is supported.
// For reference on rtp payload type numbers:
// https://en.wikipedia.org/wiki/RTP_audio_video_profile
const QList<uint8_t> PREDEFINED_AUDIO_CODECS = {0};
const QList<uint8_t> PREDEFINED_VIDEO_CODECS = {};

// dynamic payload types.
// TODO: put number of channels in parameters for audio.
const QList<RTPMap> DYNAMIC_AUDIO_CODECS = {RTPMap{96, 48000, "opus", ""}};
const QList<RTPMap> DYNAMIC_VIDEO_CODECS = {RTPMap{97, 90000, "h265", ""}};

const QString SESSION_NAME = "HEVC Video Call";
const QString CONFERENCE_SESSION_DESCRIPTION = "HEVC Video Conference";
const QString SESSION_DESCRIPTION = "A Kvazzup initiated video communication";
