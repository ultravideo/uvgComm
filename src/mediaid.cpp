#include "mediaid.h"

#include "common.h"

uint64_t MediaID::nextID_ = 1;


MediaID::MediaID(uint32_t ssrc):
    send_(true),
    receive_(true),
    ssrc_(ssrc)
{
  if (ssrc_ == 0)
  {
    ssrc_ = nextID_;
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
  return QString::number(ssrc_);
}


uint32_t MediaID::getID() const
{
  return (uint32_t)ssrc_;
}


bool MediaID::operator==(uint32_t ssrc) const
{
  return ssrc == ssrc_;
}


bool MediaID::operator!=(uint32_t ssrc) const
{
  return ssrc != ssrc_;
}


bool operator==(const MediaID& l, const MediaID& r)
{
  return l.ssrc_ == r.ssrc_;
}


bool operator!=(const MediaID& l, const MediaID& r)
{
  return l.ssrc_ != r.ssrc_;
}


bool operator<(const MediaID& l, const MediaID& r)
{
  return l.ssrc_ < r.ssrc_;
}
