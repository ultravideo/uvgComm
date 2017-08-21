#pragma once

#include "mediamanager.h"

#include <QObject>

struct SDPMessageInfo;
class StatisticsInterface;

class CallManager : public QObject
{
  Q_OBJECT
public:
  CallManager(StatisticsInterface* stats);

  void init(VideoWidget *selfview);

  void createParticipant(QString& callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                         const std::shared_ptr<SDPMessageInfo> localInfo,
                         VideoWidget *videoWidget,
                         StatisticsInterface* stats);

  void removeParticipant(QString callID);

  void endCall();

  bool roomForMoreParticipants() const;

  void uninit();

public slots:

  void recordChangedSettings();

  void micState();
  void cameraState();

private:
    MediaManager media_;

    uint16_t portsOpen_;
};

