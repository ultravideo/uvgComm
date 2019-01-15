#pragma once

class QWidget;
class QMouseEvent;
class QKeyEvent;

#include <QObject>

/*
Purpose of the VideoDrawHelper is to hold all the common code between all the video widgets.
*/


class VideoDrawHelper : public QObject
{
  Q_OBJECT
public:
  VideoDrawHelper(uint32_t sessionID);

  void mouseDoubleClickEvent(QWidget* widget, QMouseEvent *e);
  void keyPressEvent(QWidget* widget, QKeyEvent* event);

signals:

  void reattach(uint32_t sessionID_, QWidget* view);
  void deattach(uint32_t sessionID_);

private:
  void enterFullscreen(QWidget* widget);
  void exitFullscreen(QWidget* widget);

  uint32_t sessionID_;

  QWidget* tmpParent_;
};
