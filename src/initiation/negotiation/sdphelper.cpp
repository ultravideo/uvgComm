#include "sdphelper.h"


bool findCname(const MediaInfo &media, QString& cname)
{
  for (auto& attributeList : media.multiAttributes)
  {
    for (auto& attribute : attributeList)
    {
      if (attribute.type == A_CNAME)
      {
        cname = attribute.value;
        return true;
      }
    }
  }

  return false;
}


bool findCname(const MediaInfo &media, QStringList& cnames)
{
  for (auto& attributeList : media.multiAttributes)
  {
    for (auto& attribute : attributeList)
    {
      if (attribute.type == A_CNAME)
      {
        cnames.append(attribute.value);
      }
    }
  }

  return !cnames.empty();
}

bool findSSRC(const MediaInfo &media, uint32_t& ssrc)
{
  for (auto& attributeList : media.multiAttributes)
  {
    for (auto& attribute : attributeList)
    {
      if (attribute.type == A_SSRC)
      {
        ssrc = attribute.value.toUInt();
        return true;
      }
    }
  }

  return false;
}


bool findSSRC(const MediaInfo &media, std::vector<uint32_t> &ssrc)
{
  for (auto& attributeList : media.multiAttributes)
  {
    for (auto& attribute : attributeList)
    {
      if (attribute.type == A_SSRC)
      {
        ssrc.push_back(attribute.value.toUInt());
      }
    }
  }

  return !ssrc.empty();
}


bool findMID(const MediaInfo &media, int& mid)
{
  for (auto& attribute : media.valueAttributes)
  {
    if (attribute.type == A_MID)
    {
      mid = attribute.value.toInt();
      return true;
    }
  }

  return false;
}
