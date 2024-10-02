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
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                    "Received SDP message as a reply to generated SSRCs",
                                    {"Number of generated SSRCs", "Number of medias"},
                                    {QString::number(generatedSSRCs_[sessionID].size()),
                                     QString::number(sdp.media.size())});

    // go through received medias for updating
    for (auto& media : sdp.media)
    {
      int mid = 0;
      uint32_t ssrc = 0;
      QString cname = "";

      // we need to find the mid and ssrc to know which session this media belongs to
      if (findMID(media, mid) && findSSRC(media, ssrc) && findCname(media, cname))
      {
        // if we have generated an SSRC for this media, we need to generate the corresponding media for the existing participants
        if (generatedSSRCs_[sessionID].find(mid) != generatedSSRCs_[sessionID].end())
        {
          GeneratedSSRC details = generatedSSRCs_[sessionID][mid];

          if (preparedMessages_.find(details.sessionID) != preparedMessages_.end())
          {
            Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                            "Distributing media to existing session from new participant",
                                            {"Target SessionID", "MID"}, {QString::number(details.sessionID), QString::number(mid)});

            preparedMessages_[details.sessionID].push_back(media);
            // the ssrc we generated for them
            preparedMessages_[details.sessionID].last().multiAttributes.push_back({{A_SSRC, QString::number(details.ssrc)},
                                                                                   {A_CNAME, details.cname}});

            removeMID(preparedMessages_[details.sessionID].last());
            preparedMessages_[details.sessionID].last().valueAttributes.push_back({A_MID, QString::number(nextMID(details.sessionID))});
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
  else // update the existing sessions with details so new participants get correct media state
  {
    // go through received medias for updating
    for (int i = MEDIA_COUNT - 1; i < sdp.media.size(); ++i)
    {
      uint32_t recvSSRC = 0;

      if (findSSRC(sdp.media[i], recvSSRC))
      {
        for (auto& message : preparedMessages_)
        {
          for (auto& preparedMedia : message.second)
          {
            std::vector<uint32_t> ssrcs;
            if (findSSRC(preparedMedia, ssrcs) &&
                (ssrcs.size() >= 1 && ssrcs[0] == recvSSRC ||
                 ssrcs.size() >= 2 && ssrcs[1] == recvSSRC) &&
                verifyMediaInfoMatch(preparedMedia, sdp.media[i]))
            {
              Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                              "Updated media for session from new participant",
                                              {"SessionID", "index"},
                                              {QString::number(message.first), QString::number(i)});

              updateMediaState(preparedMedia, sdp.media[i]);
            }
          }
        }
      }
    }
  }
}


void SDPMeshConference::removeSession(uint32_t sessionID)
{
  // take note of cname for media removal
  QString cname = cnames_[sessionID];

  // remove session from all data structures
  cnames_.erase(sessionID);
  singleSDPTemplates_.erase(sessionID);
  preparedMessages_.erase(sessionID);
  generatedSSRCs_.erase(sessionID);
  nextMID_.erase(sessionID);

  // remove session media from prepared messages
  for (auto& message : preparedMessages_)
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
    preparedMessages_[sessionID] = {};

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

    Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                    "Generated media for session from templates",
                                    {"Number of medias"},
                                    {QString::number(preparedMessages_[sessionID].size())});
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_NORMAL, "SDPMeshConference",
                                    "Using previously generated SDP mesh message for session",
                                    {"SessionID", "Number of medias"},
                                    {QString::number(sessionID),
                                     QString::number(preparedMessages_[sessionID].size())});
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

    removeMID(singleSDPTemplates_[sessionID].last());
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

  int mid = nextMID(sessionID);
  media.valueAttributes.push_back({A_MID, QString::number(mid)});

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


bool SDPMeshConference::findSSRC(MediaInfo& media, std::vector<uint32_t> &ssrc) const
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


int SDPMeshConference::nextMID(uint32_t sessionID)
{
  if (nextMID_.find(sessionID) == nextMID_.end())
  {
    nextMID_[sessionID] = 3;
  }

  int mid = nextMID_[sessionID];
  nextMID_[sessionID]++;
  return mid;
}


void SDPMeshConference::removeMID(MediaInfo& media)
{
  // remove mid
  for (int i = 0;  i < media.valueAttributes.size(); ++i)
  {
    if (media.valueAttributes[i].type == A_MID)
    {
      media.valueAttributes.erase(media.valueAttributes.begin() + i);
      break;
    }
  }
}


void SDPMeshConference::updateMediaState(MediaInfo& currentState, const MediaInfo& newState)
{
  if (currentState.title != newState.title)
  {
    Logger::getLogger()->printNormal("SDPMeshConference", "Session title changed");
    currentState.title = newState.title;
  }

  if (currentState.bitrate != newState.bitrate)
  {
    Logger::getLogger()->printNormal("SDPMeshConference", "Bitrate changed");
    currentState.bitrate = newState.bitrate;
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


bool SDPMeshConference::verifyMediaInfoMatch(const MediaInfo& currentState,
                                             const MediaInfo& newState) const
{
  if (currentState.type != newState.type)
  {
    Logger::getLogger()->printError("SDPMeshConference", "Media types do not match");
    return false;
  }

  if (currentState.receivePort != newState.receivePort)
  {
    Logger::getLogger()->printError("SDPMeshConference", "Receive ports do not match");
    return false;
  }

  if (currentState.proto != newState.proto)
  {
    Logger::getLogger()->printError("SDPMeshConference", "Protocols do not match");
    return false;
  }

  if (currentState.connection_address != newState.connection_address ||
      currentState.connection_addrtype != newState.connection_addrtype ||
      currentState.connection_nettype != newState.connection_nettype)
  {
    Logger::getLogger()->printError("SDPMeshConference", "Connection addresses do not match");
    return false;
  }

  return true;
}


void SDPMeshConference::handleSSRCUpdate(QList<QList<SDPAttribute>>& currentAttributes,
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
