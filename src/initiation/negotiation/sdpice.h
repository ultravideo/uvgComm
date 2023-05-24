#pragma once

#include "initiation/sipmessageprocessor.h"

#include "networkcandidates.h"

#include "icetypes.h"

/* This class adds local ICE candidates to outgoing SDP messages */

class SDPICE : public SIPMessageProcessor
{
  Q_OBJECT
public:

  SDPICE(std::shared_ptr<NetworkCandidates> candidates, uint32_t sessionID, bool useICE);

  void limitMediaCandidates(int limit);

  virtual void uninit();

public slots:

  virtual void processOutgoingRequest(SIPRequest& request, QVariant& content);
  virtual void processOutgoingResponse(SIPResponse& response, QVariant& content);

  virtual void processIncomingRequest(SIPRequest& request, QVariant& content,
                                      SIPResponseStatus generatedResponse);
  virtual void processIncomingResponse(SIPResponse& response, QVariant& content,
                                       bool retryRequest);

signals:

  void localSDPWithCandidates(uint32_t sessionID, std::shared_ptr<SDPMessageInfo> local);

private:

  // generate a list of local candidates for media streaming
  QList<std::shared_ptr<ICEInfo>>
      generateICECandidates(std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> localCandidates,
                            std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> globalCandidates,
                            std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> stunCandidates,
                            std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> stunBindings,
                            std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> turnCandidates,
                            int components);

  // creates component candidates for this address list
  void addCandidates(std::shared_ptr<QList<std::pair<QHostAddress,
                     uint16_t>>> addresses,
                     std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>> relayAddresses,
                     quint32 &foundation,
                     CandidateType type, quint16 localPriority,
                     QList<std::shared_ptr<ICEInfo>>& candidates,
                     int components);



  // create one component candidate
  std::shared_ptr<ICEInfo> makeCandidate(uint32_t foundation,
                                         CandidateType type,
                                         uint8_t component,
                                         const QHostAddress address,
                                         quint16 port,
                                         const QHostAddress relayAddress,
                                         quint16 relayPort,
                                         quint16 localPriority);



  // calculate priority as recommended by specification
  int candidateTypePriority(CandidateType type, quint16 local, uint8_t component) const;

  // Adds local ICE candidates to SDP and starts nomination if we have
  // their candidates.
  void addLocalCandidatesToSDP(QVariant& content);
  void addLocalCandidatesToMedia(MediaInfo& media, int mediaIndex);
  void setConnectionAddressWithoutICE(MediaInfo& media, int mediaIndex);

  void printCandidates(QList<std::shared_ptr<ICEInfo>>& candidates);

  void addICEToSupported(std::shared_ptr<QStringList> &supported);
  bool isICEToSupported(std::shared_ptr<QStringList> supported);

  void setMediaAddress(const std::vector<std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>>> candidates,
                       MediaInfo& media, int mediaIndex);

  uint32_t sessionID_;

  std::shared_ptr<NetworkCandidates> networkCandidates_;

  bool peerSupportsICE_;

  // In joint RTP session, we want to limit the dfferent ports in candidates,
  // since we always have just one per media.
  int mediaLimit_;

  // these are saved so that when we want to renegotiate call parameters,
  // we don't have to redo ICE

  std::vector<std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>>> existingLocalCandidates_;
  std::vector<std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>>> existingGlobalCandidates_;
  std::vector<std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>>> existingStunCandidates_;
  std::vector<std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>>> existingStunBindings_;
  std::vector<std::shared_ptr<QList<std::pair<QHostAddress, uint16_t>>>> existingturnCandidates_;

  bool useICE_;
};
