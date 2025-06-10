#include "sdpconference.h"

#include "logger.h"
#include "common.h"
#include "ssrcgenerator.h"
#include "sdphelper.h"

#undef max
#include <algorithm>

const int MEDIA_COUNT = 2;

SDPConference::SDPConference():
  type_(SDP_CONF_NONE)
{}


void SDPConference::setConferenceMode(ConferenceType type)
{
  type_ = type;
}


void SDPConference::uninit()
{
  p2pSingleSDPTemplates_.clear();
}


void SDPConference::recordReceivedSDP(uint32_t sessionID, SDPMessageInfo &sdp)
{
  if (sdp.media.size() < MEDIA_COUNT)
  {
    Logger::getLogger()->printError("SDPMeshConference", "Received SDP message with too few media lines");
    return;
  }

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

  if (type_ == SDP_CONF_MESH | type_ == SDP_CONF_LOCAL_MESH | type_ == SDP_CONF_HYBRID)
  {
    updateP2PTemplate(sessionID, sdp.media);

    // if sent them a generated SSRC, it means the received medias should be distributed to existing sessions
    if (generatedSSRCs_.find(sessionID) != generatedSSRCs_.end())
    {
      distributeGeneratedMedia(sessionID, sdp);
    }
    else // update the existing sessions with details so new participants get correct media state
    {
      updateP2PMedias(sdp);
    }
  }

  if (type_ == SDP_CONF_LOCAL_SFU || type_ == SDP_CONF_SFU || type_ == SDP_CONF_HYBRID)
  {
    distributeSFUSSRCs(sessionID, sdp);
  }
}


void SDPConference::distributeGeneratedMedia(uint32_t sessionID,SDPMessageInfo &sdp)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                  "Received SDP message as a reply to generated SSRCs",
                                  {"Number of generated SSRCs", "Number of medias"},
                                  {QString::number(generatedSSRCs_[sessionID].size()),
                                   QString::number(sdp.media.size())});

  // go through received medias for updating
  for (auto& media : sdp.media)
  {
    QString mid = "";
    uint32_t ssrc = 0;
    QString cname = "";

    // we need to find the mid and ssrc to know which session this media belongs to
    if (findMID(media, mid) && findSSRC(media, ssrc) && findCname(media, cname))
    {
      // if we have generated an SSRC for this media, we need to generate the corresponding media for the existing participants
      if (generatedSSRCs_[sessionID].find(mid) != generatedSSRCs_[sessionID].end())
      {
        GeneratedSSRC details = generatedSSRCs_[sessionID][mid];

        if (p2pPreparedMessages_.find(details.sessionID) != p2pPreparedMessages_.end())
        {
          Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                          "Distributing media to existing session from new participant",
                                          {"Target SessionID", "MID"}, {QString::number(details.sessionID), mid});

          // copy the media and remove the mid
          MediaInfo newMedia = copyMedia(media);
          removeMID(newMedia);

          // in this case, the sessionID is the mediasession
          QString mid = generateP2PMID(sessionID, newMedia);
          newMedia.valueAttributes.push_back({A_MID, mid});

          p2pPreparedMessages_[details.sessionID].push_back(newMedia);
          // the ssrc we generated for them
          p2pPreparedMessages_[details.sessionID].last().multiAttributes.push_back({{A_SSRC, QString::number(details.ssrc)},
                                                                                    {A_CNAME, details.cname}});

          //removeMID(p2pPreparedMessages_[details.sessionID].last());
          //p2pPreparedMessages_[details.sessionID].last().valueAttributes.push_back({A_MID, QString::number(nextMID(details.sessionID))});
        }
        else
        {
          Logger::getLogger()->printError("SDPMeshConference", "We have generated an SSRC for a session that does not exist");
        }
      }
    }
    else
    {
      Logger::getLogger()->printError("SDPMeshConference", "Received SDP message without MID, CNAME or SSRC");
    }
  }

  generatedSSRCs_.erase(sessionID);
}


void SDPConference::updateP2PMedias(SDPMessageInfo &sdp)
{
  // go through received medias for updating
  for (int i = MEDIA_COUNT - 1; i < sdp.media.size(); ++i)
  {
    QString label = findLabel(sdp.media[i]);

    if (label == "P2P")
    {
      uint32_t recvSSRC = 0;

      if (findSSRC(sdp.media[i], recvSSRC))
      {
        for (auto& existingMessage : p2pPreparedMessages_)
        {
          // go through the medias of the existing session
          for (auto& preparedMedia : existingMessage.second)
          {
            std::vector<uint32_t> existingSSRCs;
            if (findSSRC(preparedMedia, existingSSRCs)
                && (existingSSRCs.size() >= 1 && existingSSRCs[0] == recvSSRC
                    || existingSSRCs.size() >= 2 && existingSSRCs[1] == recvSSRC)
                && verifyMediaInfoMatch(preparedMedia, sdp.media[i]))
            {
              Logger::getLogger()->printDebug(DEBUG_NORMAL,
                                              "SDPMeshConference",
                                              "Updated media for session from new participant",
                                              {"SessionID", "index"},
                                              {QString::number(existingMessage.first),
                                               QString::number(i)});

              updateMediaState(preparedMedia, sdp.media[i]);
            }
          }
        }
      }
    }
  }
}


void SDPConference::distributeSFUSSRCs(uint32_t sessionID, SDPMessageInfo &sdp)
{
  // reset existing SDP template in case the medias have been modified
  sfuSingleSDPTemplates_[sessionID].clear();

  std::vector<uint32_t> ssrc;

  int sfuCount = 0;
  for (unsigned int i = 0; i < sdp.media.size(); ++i)
  {
    // make sure this media is for SFU
    QString label = findLabel(sdp.media.at(i));
    if (label == "SFU")
    {
      MediaInfo media = copyMedia(sdp.media.at(i));
      removeMID(media);
      // add the media to the template (used to form messages for new participants)
      sfuSingleSDPTemplates_[sessionID].push_back(media);

      // find sscr
      ssrc.clear();
      findSSRC(media, ssrc);

      // there should be only one ssrc in received media
      if (ssrc.size() == 1)
      {
        // update the SSRC to existing messages
        for (auto& preparedMessage : sfuPreparedMessages_)
        {
          // skip this participants session as they don't need their own ssrc
          if (preparedMessage.first != sessionID)
          {
            if (preparedMessage.second.size() > sfuCount)
            {
              // make sure the SSRC is not already in the media
              bool exists = false;
              for (auto& attributeList : preparedMessage.second.at(sfuCount).multiAttributes)
              {
                for (auto& attribute : attributeList) {
                  if (attribute.type == A_SSRC && attribute.value == QString::number(ssrc[0]))
                  {
                    exists = true;
                    break;
                  }
                }
              }

              if (!exists) {
                Logger::getLogger()->printDebug(DEBUG_NORMAL,
                                                "SDPMeshConference",
                                                "Added SSRC to existing session",
                                                {"SessionID", "index"},
                                                {QString::number(preparedMessage.first),
                                                 QString::number(i)});

                preparedMessage.second[sfuCount].multiAttributes.push_back(
                    sdp.media.at(i).multiAttributes.at(0));
              }
            }
          }
        }
      } else {
        Logger::getLogger()->printError("SDPMeshConference",
                                        "Received SFU SDP with incorrect number of SSRCs");
      }

      ++sfuCount;
    }
  }
}


void SDPConference::removeSession(uint32_t sessionID)
{
  // take note of cname for media removal
  QString cname = cnames_[sessionID];

  // remove session from all data structures
  cnames_.erase(sessionID);
  p2pSingleSDPTemplates_.erase(sessionID);
  p2pPreparedMessages_.erase(sessionID);
  sfuSingleSDPTemplates_.erase(sessionID);
  sfuPreparedMessages_.erase(sessionID);

  generatedSSRCs_.erase(sessionID);

  // remove session media from prepared messages
  for (auto& message : p2pPreparedMessages_)
  {
    // go from the end so we dont invalidate the index
    for (int i = message.second.size() - 1;  i >= 0; --i)
    {
      bool toDelete = false;

      for (auto& attributeList : message.second[i].multiAttributes)
      {
        for (auto& attribute : attributeList)
        {
          if (attribute.type == A_CNAME && attribute.value == cname)
          {
            toDelete = true;

            Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                            "Removed media from session",
                                            {"SessionID", "index"},
                                            {QString::number(message.first), QString::number(i)});
            break;
          }
        }
      }

      if (toDelete)
      {
        message.second.erase(message.second.begin() + i);
      }
    }
  }

  Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                  "Removed session from conference",
                                  {"SessionID"}, {QString::number(sessionID)});
}


std::shared_ptr<SDPMessageInfo> SDPConference::generateConferenceMedia(uint32_t sessionID,
                                                                std::shared_ptr<SDPMessageInfo> localSDP)
{
  std::shared_ptr<SDPMessageInfo> conferenceSDP = std::make_shared<SDPMessageInfo>();
  *conferenceSDP = *localSDP; // copies local media descriptions
  conferenceSDP->media.clear();

  if (type_ == SDP_CONF_LOCAL_MESH)
  {
    conferenceSDP->media = localSDP->media;

    // Label local media as P2P
    for (auto& media : conferenceSDP->media)
    {
      media.valueAttributes.push_back({A_LABEL, "P2P"});
    }

    // Append media from other peers
    getMeshMedia(sessionID, conferenceSDP->media);
  }
  else if (type_ == SDP_CONF_MESH)
  {
    getMeshMedia(sessionID, conferenceSDP->media);
  }
  else if (type_ == SDP_CONF_LOCAL_SFU)
  {
    getSFUMedia(sessionID, localSDP->media, conferenceSDP->media);
  }
  else if (type_ == SDP_CONF_HYBRID)
  {
    getMeshMedia(sessionID, conferenceSDP->media);

    if (conferenceSDP->media.empty())
    {
      conferenceSDP->media += localSDP->media;
      for (auto& media : conferenceSDP->media)
      {
        media.valueAttributes.push_back({A_LABEL, "P2P"});
      }
    }

    getSFUMedia(sessionID, localSDP->media, conferenceSDP->media);
  }

  // General fallback if still no media (happens in the beginning)
  if (conferenceSDP->media.empty())
  {
    conferenceSDP->media = localSDP->media;

    for (auto& media : conferenceSDP->media)
    {
      if (type_ == SDP_CONF_LOCAL_SFU || type_ == SDP_CONF_SFU)
      {
        media.valueAttributes.push_back({A_LABEL, "SFU"});
      }
      else if (type_ == SDP_CONF_LOCAL_MESH || type_ == SDP_CONF_MESH)
      {
        media.valueAttributes.push_back({A_LABEL, "P2P"});
      }
    }
  }

  return conferenceSDP;
}


void SDPConference::getMeshMedia(uint32_t sessionID, QList<MediaInfo>& sdpMedia)
{
  // prepare the message for this session if we have not yet done so
  if (p2pPreparedMessages_.find(sessionID) == p2pPreparedMessages_.end())
  {
    p2pPreparedMessages_[sessionID] = {};

    // go through templates to form the sdp message and record it to preparedMessages_
    for (auto& mediaTemplate : p2pSingleSDPTemplates_)
    {
      if (mediaTemplate.first != sessionID)
      {
        for (auto& media : mediaTemplate.second)
        {
          MediaInfo newMedia = copyMedia(media);
          removeMID(newMedia);

          // we need to generate an SSRC for this media
          generateSSRC(sessionID, mediaTemplate.first, newMedia);

          // set p2p mesh label
          if (findLabel(newMedia) == "")
          {
            newMedia.valueAttributes.push_back({A_LABEL, "P2P"});
          }

          p2pPreparedMessages_[sessionID].push_back(newMedia);
        }
      }
    }

    Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                    "Generated media for session from templates",
                                    {"Number of medias"},
                                    {QString::number(p2pPreparedMessages_[sessionID].size())});
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                    "Using previously generated SDP mesh message for session",
                                    {"SessionID", "Number of medias"},
                                    {QString::number(sessionID),
                                     QString::number(p2pPreparedMessages_[sessionID].size())});
  }

  sdpMedia.append(p2pPreparedMessages_[sessionID]);
}


void SDPConference::getSFUMedia(uint32_t sessionID, const QList<MediaInfo>& baseMedia, QList<MediaInfo>& sdpMedia)
{
  // see if we should generate the template message for this session
  if (sfuPreparedMessages_.find(sessionID) == sfuPreparedMessages_.end())
  {
    sfuPreparedMessages_[sessionID].clear();

    for (const auto& media : baseMedia)
    {
      if (findLabel(media) == "SFU" || findLabel(media) == "")
      {
        MediaInfo copied = copyMedia(media);

        // Apply SFU label if needed
        if (findLabel(copied) == "")
        {
          copied.valueAttributes.push_back({A_LABEL, "SFU"});
        }

        // Add MID
        QString mid = generateSFUMID(copied);
        copied.valueAttributes.push_back({A_MID, mid});

        sfuPreparedMessages_[sessionID].push_back(std::move(copied));
      }
    }

    // add the recorded SSRC values to the medias
    for (auto& mediaTemplate : sfuSingleSDPTemplates_)
    {
      // omit their own SSRC from SSRC list
      if (mediaTemplate.first != sessionID)
      {
        // the amount of media should stay constant as call progresses in SFU SDP
        if (sfuPreparedMessages_[sessionID].size() <= mediaTemplate.second.size())
        {
          // go through templates and pick up SSRC's that will form the SFU media
          for (unsigned int i = 0; i < sfuPreparedMessages_[sessionID].size(); ++i)
          {
            // add SSRC for this media slot i
            for (auto& attributeList : mediaTemplate.second.at(i).multiAttributes)
            {
              for (auto& attribute : attributeList)
              {
                if (attribute.type == A_SSRC)
                {
                  // copy ssrc and other attributes such as cname
                  sfuPreparedMessages_[sessionID][i].multiAttributes.push_back(attributeList);
                  break;
                }
              }
            }

            if (findLabel(sfuPreparedMessages_[sessionID][i]) == "")
            {
              // set SFU label
              sfuPreparedMessages_[sessionID][i].valueAttributes.push_back({A_LABEL, "SFU"});
            }
          }
        }
        else
        {
          Logger::getLogger()->printError("SDPMeshConference", "SFU SDP template has too few medias");
        }
      }
    }
  }

  if (type_ == SDP_CONF_LOCAL_SFU && !sfuPreparedMessages_[sessionID].empty())
  {
    sdpMedia = sfuPreparedMessages_[sessionID];
  }
  else
  {
    sdpMedia.append(sfuPreparedMessages_[sessionID]);
  }
}


void SDPConference::updateP2PTemplate(uint32_t sessionID, QList<MediaInfo>& medias)
{
  // Clear existing template for this session
  if (p2pSingleSDPTemplates_.find(sessionID) != p2pSingleSDPTemplates_.end())
  {
    p2pSingleSDPTemplates_[sessionID].clear();
  }

  QList<MediaInfo> p2pMedias;

  // Collect all medias with label "P2P"
  for (int i = 0; i < medias.size(); ++i)
  {
    QString label = findLabel(medias[i]);
    if (label == "P2P")
    {
      p2pMedias.append(medias[i]);
    }
  }

  // Append last MEDIA_COUNT P2P medias
  int startIndex = std::max(0, static_cast<int>(p2pMedias.size()) - MEDIA_COUNT);
  for (int i = startIndex; i < p2pMedias.size(); ++i)
  {
    MediaInfo media = p2pMedias[i];
    media.multiAttributes.clear();  // Don't save SSRCs
    //removeMID(media);

    p2pSingleSDPTemplates_[sessionID].append(media);
  }
}


MediaInfo SDPConference::copyMedia(const MediaInfo& media)
{
  return media;
}


void SDPConference::generateSSRC(uint32_t sessionID, uint32_t mediaSessionID, MediaInfo& media)
{
  QString mid = findMID(media);
  if (mid == "")
  {
    mid = generateP2PMID(mediaSessionID, media);
    media.valueAttributes.push_back({A_MID, mid});
  }

  QString cname = cnames_[mediaSessionID];
  uint32_t ssrc = SSRCGenerator::generateSSRC();

  // we store the generated SSRC so that it can later be communicated to this participant
  generatedSSRCs_[sessionID][mid] = {ssrc, cname, mediaSessionID};

  // this ssrc/cname combination indicates to the participant that we have generated them an ssrc
  media.multiAttributes.push_back({{A_SSRC, QString::number(ssrc)}, {A_CNAME, cname}});
}


QString SDPConference::generateP2PMID(uint32_t mediaSessionID, const MediaInfo& media)
{
  return "p2p-" + media.type + "-" + QString::number(mediaSessionID);
}


QString SDPConference::generateSFUMID(const MediaInfo& media)
{
  return "sfu-" + media.type;
}


void SDPConference::removeMID(MediaInfo& media)
{
  for (int i = 0;  i < media.valueAttributes.size(); ++i)
  {
    if (media.valueAttributes[i].type == A_MID)
    {
      media.valueAttributes.erase(media.valueAttributes.begin() + i);
      break;
    }
  }
}


void SDPConference::updateMediaState(MediaInfo& currentState, const MediaInfo& newState)
{
  if (currentState.title != newState.title)
  {
    Logger::getLogger()->printNormal("SDPMeshConference", "Session title changed");
    currentState.title = newState.title;
  }


  if (currentState.bandwidth.size() != newState.bandwidth.size())
  {
    Logger::getLogger()->printNormal("SDPMeshConference", "New bitrates found");
    currentState.bandwidth = newState.bandwidth;
  }
  else
  {
    for (unsigned int i = 0; i < currentState.bandwidth.size(); ++i)
    {
      if (currentState.bandwidth.at(i).type != newState.bandwidth.at(i).type ||
          currentState.bandwidth.at(i).value != newState.bandwidth.at(i).value)
      {
        Logger::getLogger()->printNormal("SDPMeshConference", "Bandwidth changed");
        currentState.bandwidth[i].type = newState.bandwidth.at(i).type;
        currentState.bandwidth[i].value = newState.bandwidth.at(i).value;
      }
    }
  }

  if (currentState.encryptionKey != newState.encryptionKey)
  {
    Logger::getLogger()->printNormal("SDPMeshConference", "Encryption key changed");
    currentState.encryptionKey = newState.encryptionKey;
  }

  currentState.rtpNums = newState.rtpNums;
  currentState.rtpMaps = newState.rtpMaps;

  currentState.fmtpAttributes = newState.fmtpAttributes;
  currentState.candidates = newState.candidates;
  currentState.zrtp = newState.zrtp;

  // update the attributes
  currentState.flagAttributes = newState.flagAttributes;
  currentState.valueAttributes = newState.valueAttributes;

  handleSSRCUpdate(currentState.multiAttributes, newState.multiAttributes);
}


bool SDPConference::verifyMediaInfoMatch(const MediaInfo& currentState,
                                             const MediaInfo& newState) const
{
  if (currentState.type != newState.type)
  {
    return false;
  }

  if (currentState.receivePort != newState.receivePort)
  {
    return false;
  }

  if (currentState.proto != newState.proto)
  {
    return false;
  }

  if (currentState.connection_address != newState.connection_address ||
      currentState.connection_addrtype != newState.connection_addrtype ||
      currentState.connection_nettype != newState.connection_nettype)
  {
    return false;
  }

  return true;
}


void SDPConference::handleSSRCUpdate(QList<QList<SDPAttribute>>& currentAttributes,
                                         const QList<QList<SDPAttribute>>& newAttributes)
{
  // if the current media has no SSRC, we add it from the new state
  QSet<QString> currentSSRCs;
  for (auto& attributeList : currentAttributes)
  {
    for (auto& attribute : attributeList)
    {
      if (attribute.type == A_SSRC)
      {
        currentSSRCs.insert(attribute.value);
      }
    }
  }

  if (currentSSRCs.size() == 0)
  {
    // we can safely copy attributes without overwriting any SSRCs
    currentAttributes = newAttributes;
  }
  else if (currentSSRCs.size() == 1 || currentSSRCs.size() == 2)
  {
    for (auto& attributeList : newAttributes)
    {
      if (attributeList.first().type == A_SSRC)
      {
        // check if this new SSRC is already in the current state
        if (!currentSSRCs.contains(attributeList.first().value))
        {
          Logger::getLogger()->printWarning("SDPMeshConference", "Found a new SSRC in incoming SDP",
                                            "SSRC", attributeList.first().value);

          if (currentSSRCs.size() == 1)
          {
            currentAttributes.push_back(attributeList);
          }
          else  //currentSSRCs.size() == 2
          {
            Logger::getLogger()->printError("SDPMeshConference",
                                            "New SSRC is not found in previously recorded state, "
                                            "even though it has two SSRCs");
          }
        }
        else
        {
          Logger::getLogger()->printNormal("SDPMeshConference",
                                          "SSRC already in current state",
                                          "SSRC", attributeList.first().value);
        }
      }
    }
  }
  else
  {
    Logger::getLogger()->printProgramError("SDPMeshConference", "Too many SSRCs in existing media");
  }
}
