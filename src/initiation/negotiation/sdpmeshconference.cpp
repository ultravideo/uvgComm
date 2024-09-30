#include "sdpmeshconference.h"

#include "logger.h"
#include "common.h"
#include "ssrcgenerator.h"


const int MEDIA_COUNT = 2;

SDPMeshConference::SDPMeshConference():
  type_(MESH_NO_CONFERENCE)
{}


void SDPMeshConference::setConferenceMode(MeshType type)
{
  type_ = type;
}


void SDPMeshConference::uninit()
{
  singleSDPTemplates_.clear();
}

void SDPMeshConference::addRemoteSDP(uint32_t sessionID, SDPMessageInfo &sdp)
{
  if (sdp.media.size() < MEDIA_COUNT)
  {
    Logger::getLogger()->printError("SDPMeshConference", "Received SDP message with too few media lines");
    return;
  }

  updateTemplateMedia(sessionID, sdp.media);

  // store the cname
  QString cname = "";

  if (findCname(sdp.media[0], cname))
  {
    cnames_[sessionID] = cname;
  }
  else
  {
    Logger::getLogger()->printError("SDPMeshConference", "Received SDP message without CNAME");
  }

  // if sent them a generated SSRC, it means the received medias should be distributed to existing sessions
  if (generatedSSRCs_.find(sessionID) != generatedSSRCs_.end())
  {
    // go through received medias for updating
    for (auto& media : sdp.media)
    {
      int mid = 0;
      uint32_t ssrc = 0;

      // we need to find the mid and ssrc to know which session this media belongs to
      if (findMID(media, mid) && findSSRC(media, ssrc))
      {
        // if we have generated an SSRC for this media, we need to generate the corresponding media for the existing participants
        if (generatedSSRCs_[sessionID].find(mid) != generatedSSRCs_[sessionID].end())
        {
          GeneratedSSRC details = generatedSSRCs_[sessionID][mid];

          if (preparedMessages_.find(details.sessionID) != preparedMessages_.end())
          {
            Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                            "Distributing media to existing session from new participant",
                                            {"SessionID"}, {QString::number(details.sessionID)});

            preparedMessages_[details.sessionID].push_back(media);
            preparedMessages_[details.sessionID].last().multiAttributes.push_back({{A_SSRC, QString::number(details.ssrc)},
                                                                                   {A_CNAME, details.cname}});
          }
          else
          {
            Logger::getLogger()->printError("SDPMeshConference", "We have generated an SSRC for a session that does not exist");
          }
        }
      }
      else
      {
        Logger::getLogger()->printError("SDPMeshConference", "Received SDP message without MID or SSRC");
      }
    }

    generatedSSRCs_.erase(sessionID);
  }
  else // just update the existing sessions with details. Most likely matters if they have enabled/disabled media
  {
    // go through received medias for updating
    for (auto& recvMedia : sdp.media)
    {
      uint32_t recvSSRC = 0;

      if (findSSRC(recvMedia, recvSSRC))
      {
        for (auto& message : preparedMessages_)
        {
          for (auto& preparedMedia : message.second)
          {
            for (auto& attributes : preparedMedia.multiAttributes)
            {
              if (recvSSRC == attributes.first().value.toUInt())
              {
                preparedMedia = copyMedia(recvMedia);
                Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                                "Updated media for session",
                                                {"SessionID"}, {QString::number(message.first)});
              }
            }
          }
        }
      }
    }
  }
}


void SDPMeshConference::removeSession(uint32_t sessionID)
{
  singleSDPTemplates_.erase(sessionID);
  preparedMessages_.erase(sessionID);
}


std::shared_ptr<SDPMessageInfo> SDPMeshConference::getMeshSDP(uint32_t sessionID,
                                                              std::shared_ptr<SDPMessageInfo> localSDP)
{
  if (type_ == MESH_NO_CONFERENCE)
  {
    return localSDP;
  }

  // prepare the message for this session if we have not yet done so
  if (preparedMessages_.find(sessionID) == preparedMessages_.end())
  {
    // go through templates to form the sdp message and record it to preparedMessages_
    for (auto& mediaTemplate : singleSDPTemplates_)
    {
      if (mediaTemplate.first != sessionID)
      {
        for (auto& media : mediaTemplate.second)
        {
          MediaInfo newMedia = copyMedia(media);

          // we need to generate an SSRC for this media
          generateSSRC(sessionID, mediaTemplate.first, newMedia);

          preparedMessages_[sessionID].push_back(newMedia);
        }
      }
    }
  }

  // localSDP already contains host's (our) media
  std::shared_ptr<SDPMessageInfo> meshSDP = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
  *meshSDP = *localSDP;

  meshSDP->media.append(preparedMessages_[sessionID]);
  return meshSDP;
}


void SDPMeshConference::updateTemplateMedia(uint32_t sessionID, QList<MediaInfo>& medias)
{
  if (singleSDPTemplates_.find(sessionID) != singleSDPTemplates_.end())
  {
    singleSDPTemplates_[sessionID].clear();
  }

  for (int i = medias.size() - MEDIA_COUNT; i < medias.size(); ++i)
  {
    singleSDPTemplates_[sessionID].append(medias[i]);
    singleSDPTemplates_[sessionID].last().multiAttributes.clear(); // we dont what to save ssrc

    // remove mid
    for (int i = 0;  i < singleSDPTemplates_[sessionID].last().valueAttributes.size(); ++i)
    {
      if (singleSDPTemplates_[sessionID].last().valueAttributes[i].type == A_MID)
      {
        singleSDPTemplates_[sessionID].last().valueAttributes.erase(singleSDPTemplates_[sessionID].last().valueAttributes.begin() + i);
      }
    }
  }
}


MediaInfo SDPMeshConference::copyMedia(MediaInfo& media)
{
  return media;
}


void SDPMeshConference::generateSSRC(uint32_t sessionID, uint32_t mediaSessionID, MediaInfo& media)
{
  if (nextMID_.find(sessionID) == nextMID_.end())
  {
    nextMID_[sessionID] = 3;
  }

  int mid = nextMID_[sessionID];
  media.valueAttributes.push_back({A_MID, QString::number(mid)});
  nextMID_[sessionID]++;

  QString cname = cnames_[mediaSessionID];
  uint32_t ssrc = SSRCGenerator::generateSSRC();

  // we store the generated SSRC so that it can later be communicated to this participant
  generatedSSRCs_[sessionID][mid] = {ssrc, cname, mediaSessionID};

  // this ssrc/cname combination indicates to the participant that we have generated them an ssrc
  media.multiAttributes.push_back({{A_SSRC, QString::number(ssrc)}, {A_CNAME, cname}});
}


bool SDPMeshConference::findCname(MediaInfo& media, QString& cname) const
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


bool SDPMeshConference::findSSRC(MediaInfo& media, uint32_t& ssrc) const
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


bool SDPMeshConference::findMID(MediaInfo& media, int& mid) const
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
