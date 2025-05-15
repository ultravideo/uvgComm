#pragma once

#include "sdptypes.h"

#include <QString>
#include <vector>

bool findCname(const MediaInfo& media, QString& cname);
bool findCname(const MediaInfo& media, QStringList& cnames);
bool findSSRC(const MediaInfo& media, uint32_t& ssrc);
bool findSSRC(const MediaInfo& media, std::vector<uint32_t>& ssrc);
QString findLabel(const MediaInfo &media);
bool findMID(const MediaInfo& media, QString &mid);
