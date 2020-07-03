#pragma once

#include <QFrame>

#include <deque>

class ChartPainter : public QFrame
{
    Q_OBJECT
public:
  ChartPainter(QWidget *parent);
  ~ChartPainter();

  void init(int maxY, int yLines, int xWindowSize);

  void addPoint(float y);

  void clearPoints()
  {
    points_.clear();
  }

protected:
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);
  void keyPressEvent(QKeyEvent *event);

  void mouseDoubleClickEvent(QMouseEvent *e);

private:

  void drawBackground(QPainter& painter);
  void drawPoints(QPainter& painter);
  void drawForeground(QPainter& painter);

  int getDrawMinX() const;
  int getDrawMaxX() const;

  int getDrawMinY() const;
  int getDrawMaxY() const;

  int maxY_;
  int xWindowCount_;

  int numberHeight_;
  int numberWidth_;

  int yLines_;

  std::deque<float> points_;
};
