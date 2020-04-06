#pragma once

#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>

#include <QAudioFormat>

#include <memory>

class AECProcessor
{
public:
  AECProcessor();

  void init(QAudioFormat format);
  void cleanup();

  std::unique_ptr<uchar[]> processInputFrame(std::unique_ptr<uchar[]> input,
                                             uint32_t dataSize);

  std::unique_ptr<uchar[]> processEchoFrame(std::unique_ptr<uchar[]> echo,
                                            uint32_t dataSize);

private:

  QAudioFormat format_;

  uint32_t samplesPerFrame_;

  int16_t* pcmOutput_;
  int32_t max_data_bytes_;

  SpeexEchoState *echo_state_;
  SpeexPreprocessState *preprocess_state_;
};
