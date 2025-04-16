#pragma once

#include <QSettings>
#include <QString>
#include <QThread>

class ParticipantInterface;

class Scripting : public QThread
{
  Q_OBJECT
public:
  Scripting();

  void setPartInterface(ParticipantInterface *partInt)
  {
    interface_ = partInt;
  }

  void fileScripting(const QString &filename);
  void stdinScripting();

signals:
  void endCall();

  void updateVideoSetting();
  void updateAudioSetting();
  void updateCallSetting();

  void quitScript();

protected:

  void run() override;

private:
  void processStream(QTextStream& stream);
  void executeCommand(const QString& command);

  QString filename_;
  QSettings settings_;

  ParticipantInterface* interface_;
};

