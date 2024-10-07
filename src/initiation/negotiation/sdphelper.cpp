#include "sdphelper.h"


bool findCname(MediaInfo& media, QString& cname)
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


bool findSSRC(MediaInfo& media, uint32_t& ssrc)
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


bool findSSRC(MediaInfo& media, std::vector<uint32_t> &ssrc)
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


bool findMID(MediaInfo& media, int& mid)
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
