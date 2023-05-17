#pragma once

#include "initiation/negotiation/sdptypes.h"


class MediaID
{
public:
  MediaID(const MediaInfo& media);
  ~MediaID();

  QString toString() const;

  uint32_t getID() const;

  bool operator==(const MediaInfo& media) const;
  bool operator!=(const MediaInfo& media) const;

  friend bool operator==(const MediaID& l, const MediaID& r);
  friend bool operator!=(const MediaID& l, const MediaID& r);
  friend bool operator<(const MediaID& l, const MediaID& r);

private:
  //bool operator=(const MediaID& r);

  bool areMediasEqual(const MediaInfo &first, const MediaInfo &second) const;

  static uint64_t nextID_;
  uint64_t id_;

  MediaInfo media_;
};
