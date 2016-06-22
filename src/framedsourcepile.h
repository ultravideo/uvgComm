#pragma once

#include "filter.h"

#include <FramedSource.hh>
#include <QMutex>


class FramedSourcePile : public FramedSource
{
public:
  FramedSourcePile(UsageEnvironment &env);

  void putInput(std::unique_ptr<Data> data);


  virtual void doGetNextFrame();

  virtual Boolean isH265VideoStreamFramer() const
  {
    return type_ == HEVCVIDEO;
  }
private:
//  EventTriggerId eventTriggerId_;
//  uint32_t referenceCount_;


  std::unique_ptr<Data> getInput();

  DataType type_;

  QMutex pileMutex_;
  std::queue<std::unique_ptr<Data>> pile_;

};
