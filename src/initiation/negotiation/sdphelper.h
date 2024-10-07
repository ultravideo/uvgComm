#pragma once

#include "sdptypes.h"

#include <QString>
#include <vector>

bool findCname(MediaInfo& media, QString& cname);
bool findSSRC(MediaInfo& media, uint32_t& ssrc);
bool findSSRC(MediaInfo& media, std::vector<uint32_t>& ssrc);
bool findMID(MediaInfo& media, int& mid);
