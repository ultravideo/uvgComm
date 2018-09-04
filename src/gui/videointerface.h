#pragma once

#include <QImage>

#include <memory>

class QWidget;
class StatisticsInterface;

// a very simple interface for drawing video to screen.
// an object using this interface can be created using VideoviewFactory
// The purpose of this interface and videoviewfactory is to enable
// a more flexible way of modifying drawing interface and its appearance.

class VideoInterface
{
public:
  virtual ~VideoInterface(){}

  // set stats to use with this video view.
  virtual void setStats(StatisticsInterface* stats) = 0;

  // Takes ownership of the image data
  virtual void inputImage(std::unique_ptr<uchar[]> data, QImage &image) = 0;

signals:

  virtual void reattach(uint32_t sessionID_, QWidget* view) = 0;
};

Q_DECLARE_INTERFACE(VideoInterface, "VideoInterface")
