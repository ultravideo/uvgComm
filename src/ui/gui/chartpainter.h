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

  int maxY_;
  int xWindowCount_;

  QSize numberSize_;

  int yLines_;

  //std::deque<float> points_;

  std::vector<std::shared_ptr<std::deque<float>>> points_;

  QStringList names_;

  QString title_;

  QSize titleSize_;
};
