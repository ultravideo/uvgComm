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
  std::shared_ptr<SDPMessageInfo> referenceSDP =
      std::shared_ptr<SDPMessageInfo>(new SDPMessageInfo);
  *referenceSDP = sdp;
  singleSDPTemplates_[sessionID] = referenceSDP;

  QString cname = "";

  if (findCname(sdp.media.first(), cname))
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                    "Recording CNAME for participant",
                                    {"CNAME"}, {cname});

    cnames_[sessionID] = cname;
  }

  while (singleSDPTemplates_[sessionID]->media.size() > MEDIA_COUNT)
  {
    // remove media meant for us (the host)
    singleSDPTemplates_[sessionID]->media.pop_front();
  }

  // process SSRCs and CNAMEs
  for (int i = 0; i < singleSDPTemplates_[sessionID]->media.size(); ++i)
  {
    // check if this media has a corresponding SSRC
    for (auto& ssrcList : generatedSSRCs_)
    {
      uint32_t correspondingSSRC = 0;
      QString correspondingCname = "";

      if (ssrcList.second.find(sessionID) != ssrcList.second.end() &&
          ssrcList.second[sessionID].size() > (i) &&
          findSSRC(singleSDPTemplates_[sessionID]->media[i], correspondingSSRC) &&
          findCname(singleSDPTemplates_[sessionID]->media[i], correspondingCname))
      {
        Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                        "Adding corresponding SSRC to generated SSRC",
                                        {"Generated SSRC", "Corresponding SSRC", "CNAME", "Corresponding CNAME"},
                                          {QString::number(ssrcList.second[sessionID].at(i).generatedSSRC),
                                           QString::number(correspondingSSRC),
                                           ssrcList.second[sessionID].at(i).cname,
                                           correspondingCname});

        ssrcList.second[sessionID].at(i).correspondingSSRC =
            correspondingSSRC;
        ssrcList.second[sessionID].at(i).correspondingCname =
            correspondingCname;
      }
    }

    // remove SSRC and CNAME from the stored message
    singleSDPTemplates_[sessionID]->media[i].multiAttributes.clear();
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

      for (auto& storedSDP : singleSDPTemplates_)
      {
        // naturally, we exclude their own media from the SDP
        if (storedSDP.first != sessionID)
        {
          QList<MediaInfo> medias = storedSDP.second->media;

          for (unsigned int i = 0; i < medias.size(); ++i)
          {
            // check if we already know the corresponding ssrc
            if (!setCorrespondingSSRC(sessionID, storedSDP.first, i, medias))
            {
              // if we did not find the corresponding ssrc, we generate a new one
              generateSSRC(sessionID, storedSDP.first, i, medias);
            }
            else
            {
              // if we have previously generated the ssrc, we add it to the message
              setGeneratedSSRC(sessionID, storedSDP.first, i, medias);
            }
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
          for (auto& media : session.second->media)
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
      else
      {
        Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "SDPMeshConference",
                                        "Not enough recorded pre-generated SSRC values for this stream");
      }
    }
  }

  return false;
}


void SDPMeshConference::generateSSRC(uint32_t sessionID, uint32_t mediaSessionID,
                                     int index, QList<MediaInfo>& medias)
{
  // only generate if we have not generated one previously
  if (generatedSSRCs_[mediaSessionID][sessionID].size() == index)
  {
    QString cname = cnames_[mediaSessionID];
    uint32_t ssrc = SSRCGenerator::generateSSRC();

    // we store the generated SSRC so that it can later be communicated to this participant
    generatedSSRCs_[mediaSessionID][sessionID].push_back({ssrc, cname, 0, ""});
  }

  QString ssrc = QString::number(generatedSSRCs_[mediaSessionID][sessionID][index].generatedSSRC);
  QString cname = generatedSSRCs_[mediaSessionID][sessionID][index].cname;

  // this ssrc/cname combination indicates to the participant that we have generated them an ssrc
  medias[index].multiAttributes.push_back({{A_SSRC, ssrc}, {A_CNAME, cname}});
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
      else
      {
        Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, "SDPMeshConference",
                                        "Not enough recorded pre-generated SSRC values for this stream");
      }
    }
  }
}


bool SDPMeshConference::findCname(MediaInfo& media, QString& cname)
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


bool SDPMeshConference::findSSRC(MediaInfo& media, uint32_t& ssrc)
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
