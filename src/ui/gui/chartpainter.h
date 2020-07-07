#pragma once

#include <QFrame>

#include <deque>

class ChartPainter : public QFrame
{
    Q_OBJECT
public:
  ChartPainter(QWidget *parent);
  ~ChartPainter();

  void init(int maxY, int yLines, int xWindowSize, QString chartTitle);

  // return line ID
  int addLine(QString name);

  // add one point to line
  void addPoint(int lineID, float y);

  void clearPoints();

protected:
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);
  void keyPressEvent(QKeyEvent *event);

  void mouseDoubleClickEvent(QMouseEvent *e);

private:

  void drawBackground(QPainter& painter);
  void drawPoints(QPainter& painter, int lineID, bool &outDrawZero, bool &outDrawMax);
  void drawForeground(QPainter& painter, bool drawZero, bool drawMax);

  int getDrawMinX() const;
  int getDrawMaxX() const;

  int getDrawMinY() const;
  int getDrawMaxY() const;

  void drawMark(QPainter& painter, int lineID, float x, float y);

  void drawCircle(QPainter& painter, float x, float y);
  void drawSquare(QPainter& painter, float x, float y);
  void drawTriangle(QPainter& painter, float x, float y);
  void drawCross(QPainter& painter, float x, float y);

  void drawLegend(QPainter& painter, float x, float y, int legendMargin, int markSize,
                  int lineID, QString name);

  int maxY_;
  int xWindowCount_;

  QSize numberSize_;

  int yLines_;

  //std::deque<float> points_;

  std::vector<std::shared_ptr<std::deque<float>>> points_;

  QStringList names_;

  QString title_;
  QSize titleSize_;

  int legendRows_;
};
