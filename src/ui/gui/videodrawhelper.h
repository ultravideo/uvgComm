#pragma once

#include "global.h"

#include <QObject>
#include <QSize>
#include <QImage>
#include <QRect>
#include <QWidget>
#include <QMouseEvent>
#include <QKeyEvent>

#include <QElapsedTimer>
#include <QMutex>
#include <QSvgRenderer>

#include <deque>
#include <memory>

/*
 * Purpose of the VideoDrawHelper is to process all the mouse and keyboard events
 * for video view widgets. It also includes the buffer for images waiting to be drawn.
*/

// This class could possibly be combined with displayfilter.

// TODO: When the fullscreen mode is activated and there is another program on the background,
// that another program can be on top of the fullscreen for some reason.

class VideoDrawHelper : public QObject
{
  Q_OBJECT
public:
  VideoDrawHelper(uint32_t sessionID, LayoutID layoutID, uint8_t borderSize);
  ~VideoDrawHelper();

  void initWidget(QWidget* widget);

  void enableOverlay(int roiQP, int backgroundQP, int brushSize, bool showGrid, bool pixelBased,
                     QSize videoResolution);
  void resetOverlay();
  void updateROIMask();

  void setDrawMicOff(bool state);

  bool readyToDraw();
  void inputImage(QWidget *widget, std::unique_ptr<uchar[]> data,
                  QImage &image, int64_t timestamp);

  // returns whether this is a new image or the previous one
  bool getRecentImage(QImage& image);

  void draw(QPainter& painter);

  void addPointToOverlay(const QPointF& position, bool addPoint, bool removePoint);

  void mouseDoubleClickEvent(QWidget* widget);
  void keyPressEvent(QWidget* widget, QKeyEvent* event);

  // update the rect in case the window or input has changed.
  void updateTargetRect(QWidget* widget);

  QRect getTargetRect()
  {
    return imageRect_;
  }

  QRect getFrameRect()
  {
    return borderRect_;
  }

  std::unique_ptr<int8_t[]> getRoiMask(int& width, int& height, int qp, bool scaleToInput);

signals:

  void reattach(LayoutID layoutID);
  void detach(LayoutID layoutID);

private:
  void enterFullscreen(QWidget* widget);
  void exitFullscreen(QWidget* widget);

  void updateROIMask(int& width, int& height, int qp, bool scaleToInput);

  inline void clipValue(int& value, int maximumChange);

  int colorToQP(QColor& color, int baseQP);
  QColor qpToColor(int qp);

  void drawGrid();
  void setCTUQP(QPainter &painter, const QPointF &viewPosition, QSizeF viewMultiplier);

  QSizeF getSizeMultipliers(int width, int height);

  uint32_t sessionID_;
  LayoutID layoutID_;

  QWidget* tmpParent_;

  QRect imageRect_;
  QRect iconRect_;
  QRect borderRect_;

  bool firstImageReceived_;
  QSize previousSize_;

  int borderSize_;

  struct Frame
  {
    QImage image;
    std::unique_ptr<uchar[]> data;
    int64_t timestamp;
  };

  Frame lastFrame_;
  std::deque<Frame> frameBuffer_;

  int64_t currentFrame_;

  QMutex roiMutex_;
  size_t currentSize_;
  std::unique_ptr<int8_t[]> currentMask_;

  bool drawOverlay_;
  QImage overlay_;
  QImage grid_;

  int roiQP_;
  int backgroundQP_;
  int brushSize_;

  QSize videoResolution_;

  bool showGrid_;
  bool pixelBasedDrawing_;

  QSvgRenderer  micIcon_;
  bool drawIcon_;

  bool fullscreen_;

  int bufferFullWarnings_;
};
