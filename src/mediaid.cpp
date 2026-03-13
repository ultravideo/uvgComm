#include "mediaid.h"

uint64_t MediaID::nextID_ = 1;


MediaID::MediaID(uint32_t localSSRC, uint32_t remoteSSRC):
  send_(true),
  receive_(true),
  localSSRC_(localSSRC),
  remoteSSRC_(remoteSSRC)
{
  if (localSSRC_ == 0)
  {
    localSSRC_ = nextID_;
    nextID_++;
  }
}


MediaID::~MediaID()
{}


void MediaID::setReceive(bool status)
{
  receive_ = status;
}


void MediaID::setSend(bool status)
{
  send_ = status;
}


bool MediaID::getReceive() const
{
  return receive_;
}


bool MediaID::getSend() const
{
  return send_;
}


QString MediaID::toString() const
{
  return "L: " + QString::number(localSSRC_) + " R: " + QString::number(remoteSSRC_);
}


bool MediaID::isLocal(uint32_t ssrc) const
{
  return localSSRC_ == ssrc;
}


bool MediaID::isRemote(uint32_t ssrc) const
{
  return remoteSSRC_ == ssrc;
}


bool operator==(const MediaID& l, const MediaID& r)
{
  return l.localSSRC_ == r.localSSRC_ && l.remoteSSRC_ == r.remoteSSRC_;
}


bool operator!=(const MediaID& l, const MediaID& r)
{
  return l.localSSRC_ != r.localSSRC_ || l.remoteSSRC_ != r.remoteSSRC_;
}


bool operator<(const MediaID& l, const MediaID& r)
{
  uint64_t totalSSRC = l.localSSRC_ + l.remoteSSRC_;
  uint64_t totalSSRC2 = r.localSSRC_ + r.remoteSSRC_;
  return totalSSRC < totalSSRC2;
}
