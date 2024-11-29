#pragma once

#include "initiation/negotiation/sdptypes.h"


class MediaID
{
public:
  MediaID(uint32_t ssrc);
  ~MediaID();

  void setReceive(bool status);
  void setSend(bool status);

  bool getReceive() const;
  bool getSend() const;

  QString toString() const;

  uint32_t getID() const;

  bool operator==(uint32_t ssrc) const;
  bool operator!=(uint32_t ssrc) const;

  friend bool operator==(const MediaID& l, const MediaID& r);
  friend bool operator!=(const MediaID& l, const MediaID& r);
  friend bool operator<(const MediaID& l, const MediaID& r);

private:

  static uint64_t nextID_;

  bool send_;
  bool receive_;

  uint32_t ssrc_;
};
