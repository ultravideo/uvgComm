
#include "mediaid.h"

uint64_t MediaID::nextID_ = 1;


MediaID::MediaID(const MediaInfo &media):
    id_(++nextID_),
    media_(media)
{}


MediaID::~MediaID()
{}


QString MediaID::toString() const
{
  return QString::number(id_);
}


uint32_t MediaID::getID() const
{
  return (uint32_t)id_;
}


bool MediaID::operator==(const MediaInfo& media) const
{
  return areMediasEqual(media, media_);
}


bool MediaID::operator!=(const MediaInfo& media) const
{
  return !areMediasEqual(media, media_);
}


bool operator==(const MediaID& l, const MediaID& r)
{
  return l.id_ == r.id_;
}


bool operator!=(const MediaID& l, const MediaID& r)
{
  return l.id_ != r.id_;
}


bool operator<(const MediaID& l, const MediaID& r)
{
  return l.id_ < r.id_;
}


bool MediaID::areMediasEqual(const MediaInfo& first, const MediaInfo& second) const
{
  return first.type == second.type &&
         first.receivePort == second.receivePort &&
         first.proto == second.proto &&
         first.connection_nettype == second.connection_nettype &&
         first.connection_addrtype == second.connection_addrtype &&
         first.connection_address == second.connection_address;
}
