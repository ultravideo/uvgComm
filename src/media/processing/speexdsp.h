#pragma once


#include <speex/speex_preprocess.h>

#include <QAudioFormat>
#include <QMutex>

#include <deque>
#include <memory>

// This class implements Speex Acoustic Echo Cancellation (AEC). After some testing
// I think it is implemented optimally, but it does not seem very good. It blocks some
// voices, but not nearly all of them. I guess it is better than nothing, but
// it could be replaced at some point. The Automatic gain control (AGC) is very
// is very important for preventing feedback loops

class SpeexDSP : public QObject
{
  Q_OBJECT
public:
  SpeexDSP(QAudioFormat format);

  void updateSettings();

  void init();
  void cleanup();

  std::unique_ptr<uchar[]> processInputFrame(std::unique_ptr<uchar[]> input,
                                             uint32_t dataSize);
private:

  QAudioFormat format_;
  uint32_t samplesPerFrame_;

  QMutex processMutex_;

  // preprocessor refers to running it before the encoder
  SpeexPreprocessState *preprocessor_;

};
