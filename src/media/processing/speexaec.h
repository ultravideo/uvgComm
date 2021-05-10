#pragma once

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include <QAudioFormat>
#include <QMutex>

#include <deque>
#include <memory>


class SpeexAEC : public QObject
{
  Q_OBJECT
public:
  SpeexAEC(QAudioFormat format);

  void updateSettings();

  void init();
  void cleanup();

  std::unique_ptr<uchar[]> processInputFrame(std::unique_ptr<uchar[]> input,
                                             uint32_t dataSize);

  void processEchoFrame(uint8_t *echo,
                        uint32_t dataSize);

private:

  uint8_t* createEmptyFrame(uint32_t size);

  QAudioFormat format_;
  uint32_t samplesPerFrame_;

  QMutex echoMutex_;

  SpeexEchoState *echo_state_;
  SpeexPreprocessState *preprocessor_;

  uint32_t echoSize_;
  uint8_t* echoSample_;

  bool enabled_;
};
