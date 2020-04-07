#pragma once

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include <QAudioFormat>
#include <QMutex>

#include <deque>
#include <memory>

class AECProcessor : public QObject
{
  Q_OBJECT
public:
  AECProcessor(QAudioFormat format);

  void cleanup();

  std::unique_ptr<uchar[]> processInputFrame(std::unique_ptr<uchar[]> input,
                                             uint32_t dataSize);

  void processEchoFrame(std::unique_ptr<uchar[]> echo,
                        uint32_t dataSize,
                        uint32_t sessionID);

private:

  void initEcho(uint32_t sessionID);

  std::unique_ptr<uchar[]> processInput(SpeexEchoState *echo_state,
                                        std::unique_ptr<uchar[]> input,
                                        std::unique_ptr<uchar[]> echo, uint32_t pos);

  struct EchoBuffer
  {
    SpeexPreprocessState *preprocess_state;
    SpeexEchoState *echo_state;
    std::deque<std::unique_ptr<uchar[]>> frames;
  };

  QMutex echoMutex_;
  std::map<uint32_t, std::shared_ptr<EchoBuffer>> echoes_;

  QAudioFormat format_;

  uint32_t samplesPerFrame_;


  int32_t max_data_bytes_;
};
