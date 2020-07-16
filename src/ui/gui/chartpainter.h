#pragma once

#include <QFrame>
#include <QMutex>

#include <deque>

class ChartPainter : public QFrame
{
    Q_OBJECT
public:
  ChartPainter(QWidget *parent);
  ~ChartPainter();

  // Initiate the values for this chart.
  // - yLines is the number of background lines
  // - xWindowSize is the number of previous points shown in chart
  // - adaptive tells whether the graph adjusts to changes in values or not.
  //   If the values exeed maxY, the adaptive will bne turned on.
  // Things to consider when selecting values:
  // 1) what happes when the maxY or yLines when they are multiplied or divided by two
  // 2) if yLines is 5 or less, the graph wont go smaller
  // 3) yLines is capped at 10
  void init(int maxY, int yLines, bool adaptive, int xWindowSize, QString chartTitle);

  // returns the line ID of added line
  int addLine(QString name);

  // Reduces the index of all following lineID:s by one so make changes accordingly
  void removeLine(int lineID);

  // add one point to line and remove point if x window size is exceeded
  // also adjusts the graph max and ylines values if adaptivity is enabled.
  void addPoint(int lineID, float y);

  // clears all points from graph, but keeps the lines
  void clearPoints();

protected:
  void paintEvent(QPaintEvent *event);
  void resizeEvent(QResizeEvent *event);
  void keyPressEvent(QKeyEvent *event);

  void mouseDoubleClickEvent(QMouseEvent *e);

private:

  // draw stuff that sits one the background such as frame and ylines
  void drawBackground(QPainter& painter);

  // draw the actual lines and points in them
  void drawPoints(QPainter& painter, int lineID, bool &outDrawZero, bool &outDrawMax);

  // draw stuff like legends
  void drawForeground(QPainter& painter, bool drawZero, bool drawMax);

  // get the graph area limits excluding stuff like title, legends or numbers
  int getDrawMinX() const;
  int getDrawMaxX() const;
  int getDrawMinY() const;
  int getDrawMaxY() const;

  // draw the whole legend
  void drawLegend(QPainter& painter, float x, float y, int legendMargin, int markSize,
                  int lineID, QString name);

  // draw a mark for this line based on lineID
  void drawMark(QPainter& painter, int lineID, float x, float y);

  // helper functions for drawing different shapes
  // TODO: test these and fix them
  void drawCircle(QPainter& painter, float x, float y);
  void drawSquare(QPainter& painter, float x, float y);
  void drawTriangle(QPainter& painter, float x, float y);
  void drawCross(QPainter& painter, float x, float y);

  // what is the maximum y value of the chart
  int maxY_;

  // how many points are drawn at maximum per
  int xWindowCount_;

  // how large are the numbers
  QSize numberSize_;

  // is the scale of graph adaptive or is it constant.
  // constant makes sense if the graph is only expected to have limited changes
  // if such graph exceeds the maxY value, the adaptiveLines is set true to
  // still show all values to user.
  bool adaptiveLines_;

  // how many background lines are drawn
  int yLines_;

  // how many times we skipped increasing the lines
  int overLines_;

  // this mutex makes sure we are not adding lines or points while we are drawing them
  QMutex lineMutex_;

  // lines and their points
  std::vector<std::shared_ptr<std::deque<float>>> points_;

  // names of lines for legends
  QStringList names_;

  // the width of longest name
  int maxNameWidth_;

  // title of the whole chart
  QString title_;

  // how big is the title
  QSize titleSize_;

  // how many rows will the legends need
  int legendRows_;

  // font used when drawing text
  QFont font_;

  // TODO: maybe a larger font for chart title?
};
