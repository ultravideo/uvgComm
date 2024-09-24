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
  // save a copy of the media

  singleSDPTemplates_[sessionID] = sdp.media;

  QString cname = "";

  if (findCname(sdp.media.first(), cname))
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                    "Recording CNAME for participant",
                                    {"CNAME"}, {cname});

    cnames_[sessionID] = cname;
  }

  int nMedias = 0;

  // we go through all the generated SSRCs and store the corresponding SSRCs from the received message
  for (auto& ssrcList : generatedSSRCs_)
  {
    if (ssrcList.second.find(sessionID) != ssrcList.second.end())
    {
      for (int i = 0; i < singleSDPTemplates_[sessionID].size(); ++i)
      {
        int mid = 0;
        uint32_t correspondingSSRC = 0;
        QString correspondingCname = "";

        if (findSSRC( singleSDPTemplates_[sessionID][i], correspondingSSRC) &&
            findCname(singleSDPTemplates_[sessionID][i], correspondingCname) &&
            findMID(  singleSDPTemplates_[sessionID][i], mid))
        {
          if (ssrcList.second[sessionID].size() > nMedias &&
              ssrcList.second[sessionID].at(nMedias).correspondingMID == mid)
          {
            Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                            "Adding corresponding SSRC to generated SSRC",
                                            {"Generated SSRC", "CNAME", "Corresponding SSRC", "Corresponding CNAME", "MID"},
                                              {QString::number(ssrcList.second[sessionID].at(nMedias).generatedSSRC),
                                               ssrcList.second[sessionID].at(nMedias).cname,
                                               QString::number(correspondingSSRC),
                                               correspondingCname, QString::number(mid)});

            ssrcList.second[sessionID].at(nMedias).correspondingSSRC =  correspondingSSRC;
            ssrcList.second[sessionID].at(nMedias).correspondingCname = correspondingCname;

            ++nMedias;
          }
        }
        else
        {
          Logger::getLogger()->printError("SDPMeshConference", "Could not find MID, SSRC or CNAME for media");
        }
      }
    }
  }

  for (int i = 0; i < singleSDPTemplates_[sessionID].size(); ++i)
  {
    // remove SSRC and CNAME from the stored message
    singleSDPTemplates_[sessionID][i].multiAttributes.clear();
  }

  // we only want to record two instances
  while (singleSDPTemplates_[sessionID].size() > MEDIA_COUNT)
  {
    singleSDPTemplates_[sessionID].pop_front();
  }
}


void SDPMeshConference::removeSession(uint32_t sessionID)
{
  singleSDPTemplates_.erase(sessionID);
}


std::shared_ptr<SDPMessageInfo> SDPMeshConference::getMeshSDP(uint32_t sessionID,
                                                              std::shared_ptr<SDPMessageInfo> localSDP)
{
  // localSDP already contains host's (our) media
  std::shared_ptr<SDPMessageInfo> meshSDP = localSDP;

  switch(type_)
  {
    case MESH_NO_CONFERENCE:
    {
      break;
    }
    case MESH_WITH_RTP_MULTIPLEXING:
    {
      meshSDP = std::shared_ptr<SDPMessageInfo> (new SDPMessageInfo);
      *meshSDP = *localSDP;

      int mid = 3;

      for (auto& storedSDP : singleSDPTemplates_)
      {
        // naturally, we exclude their own media from the SDP
        if (storedSDP.first != sessionID)
        {
          QList<MediaInfo> medias = storedSDP.second;

          for (unsigned int i = 0; i < medias.size(); ++i)
          {
            // check if we already know the corresponding ssrc
            if (!setCorrespondingSSRC(sessionID, storedSDP.first, i, medias))
            {
              // if we did not find the corresponding ssrc, we generate a new one
              generateSSRC(sessionID, storedSDP.first, i, medias, mid);
            }
            else
            {
              // if we have previously generated the ssrc, we add it to the message
              setGeneratedSSRC(sessionID, storedSDP.first, i, medias);
            }

            setMid(medias[i], mid++);
          }

          meshSDP->media += medias;
        }
      }
      break;
    }
    case MESH_WITHOUT_RTP_MULTIPLEXING:
    {
      if (meshSDP->media.size() > 2)
      {
        for (unsigned int i = meshSDP->media.size(); i > 2; --i)
        {
          meshSDP->media.pop_back();
        }
      }

      for (auto& session : singleSDPTemplates_)
      {
        int components = 4;

        // do not add their own media to the message
        if (session.first != sessionID)
        {
          for (auto& media : session.second)
          {
            // if this is the outgoing extrapolated message, we increase candidate ports
            if (singleSDPTemplates_.find(sessionID) == singleSDPTemplates_.end())
            {
              for (int i = 0; i < media.candidates.size(); ++i)
              {
                media.candidates[i] = updateICECandidate(media.candidates[i],
                                                         components);
              }

              media.receivePort += components;
            }

            localSDP->media.push_back(copyMedia(media));
          }
        }
      }
      break;
    }
    default:
    {
      Logger::getLogger()->printUnimplemented("SDPMeshConference", "Unimplemented mesh conferencing mode");
      break;
    }
  }

  return meshSDP;
}


MediaInfo SDPMeshConference::copyMedia(MediaInfo& media)
{
  return media;
}


std::shared_ptr<ICEInfo> SDPMeshConference::updateICECandidate(std::shared_ptr<ICEInfo> candidate,
                                                               int components)
{
  Logger::getLogger()->printNormal("SDPMeshConference", "Updating candidate");
  std::shared_ptr<ICEInfo> newCandidate = std::shared_ptr<ICEInfo>(new ICEInfo);
  *newCandidate = *candidate;
  if (newCandidate->port != 0)
  {
    newCandidate->port += components;
  }

  if (newCandidate->rel_port != 0)
  {
    newCandidate->rel_port += components;
  }

  return newCandidate;
}


bool SDPMeshConference::setCorrespondingSSRC(uint32_t sessionID, uint32_t mediaSessionID,
                          int index, QList<MediaInfo>& medias)
{
  if (generatedSSRCs_.find(sessionID) != generatedSSRCs_.end())
  {
    if (generatedSSRCs_[sessionID].find(mediaSessionID) != generatedSSRCs_[sessionID].end())
    {
      if (generatedSSRCs_[sessionID][mediaSessionID].size() > index)
      {
        Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                        "Setting the correct corresponding SSRC to the message",
                                        {"SSRC"}, {QString::number(generatedSSRCs_[sessionID][mediaSessionID][index].correspondingSSRC)});

        // this ssrc/cname combination is the corresponding ssrc to the generated one
        medias[index].multiAttributes.push_back({{A_SSRC, QString::number(generatedSSRCs_[sessionID][mediaSessionID][index].correspondingSSRC)},
                                             {A_CNAME,                generatedSSRCs_[sessionID][mediaSessionID][index].correspondingCname}});

        return true;
      }
    }
  }

  return false;
}


void SDPMeshConference::generateSSRC(uint32_t sessionID, uint32_t mediaSessionID,
                                     int index, QList<MediaInfo>& medias, int mid)
{
  // only generate if we have not generated one previously
  if (generatedSSRCs_[mediaSessionID][sessionID].size() == index)
  {
    QString cname = cnames_[mediaSessionID];
    uint32_t ssrc = SSRCGenerator::generateSSRC();

    // we store the generated SSRC so that it can later be communicated to this participant
    generatedSSRCs_[mediaSessionID][sessionID].push_back({ssrc, cname, mid, 0, ""});

    // this ssrc/cname combination indicates to the participant that we have generated them an ssrc
    medias[index].multiAttributes.push_back({{A_SSRC, QString::number(ssrc)}, {A_CNAME, cname}});
  }
}


void SDPMeshConference::setGeneratedSSRC(uint32_t sessionID, uint32_t mediaSessionID,
                                         int index, QList<MediaInfo>& medias)
{
  // see if there are any generated SSRCs that should be added to this message
  if (generatedSSRCs_.find(sessionID) != generatedSSRCs_.end())
  {
    if (generatedSSRCs_[sessionID].find(mediaSessionID) != generatedSSRCs_[sessionID].end())
    {
      if (generatedSSRCs_[sessionID][mediaSessionID].size() > index)
      {
        Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                        "Communicating pre-generated SSRC to participant",
                                        {"SSRC"}, {QString::number(generatedSSRCs_[sessionID][mediaSessionID][index].generatedSSRC)});

        // this ssrc/cname combination indicates to the participant that we have generated them an ssrc
        medias[index].multiAttributes.push_back({{A_SSRC, QString::number(generatedSSRCs_[sessionID][mediaSessionID][index].generatedSSRC)},
                                                 {A_CNAME,                generatedSSRCs_[sessionID][mediaSessionID][index].cname}});
      }
    }
  }
}


void SDPMeshConference::setMid(MediaInfo& media, int mid)
{
  for (auto& attribute : media.valueAttributes)
  {
    if (attribute.type == A_MID)
    {
      attribute.value = QString::number(mid);
      return;
    }
  }
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
