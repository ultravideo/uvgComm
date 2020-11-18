#pragma once

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include <QAudioFormat>
#include <QMutex>

#include <deque>
#include <memory>

// This class implements Speex Echo cancellation. After some testing I think
// it is implemented optimally, but it does not seem very good. It blocks some
// voices, but not nearly all of them. I guess it is better than nothing, but
// it could be replaced at some point. The Automatic gain control is enabled

class AECProcessor : public QObject
{
  Q_OBJECT
public:
  AECProcessor(QAudioFormat format);

  void updateSettings();

  void init();
  void cleanup();

  std::unique_ptr<uchar[]> processInputFrame(std::unique_ptr<uchar[]> input,
                                             uint32_t dataSize);

  void processEchoFrame(uint8_t *echo,
                        uint32_t dataSize);

  uint8_t* createEmptyFrame(uint32_t size);

private:

  QAudioFormat format_;
  uint32_t samplesPerFrame_;

  SpeexPreprocessState *preprocessor_;
  SpeexEchoState *echo_state_;

  QMutex echoMutex_;
  uint32_t echoSize_;
  uint8_t* echoSample_;
};
