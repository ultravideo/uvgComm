#include "chartpainter.h"

#include <QPainter>
#include <QPaintEvent>


enum Shape {CIRCLE, SQUARE, TRIANGLE, CROSS};

struct LineAppearance
{
  QColor color;
  Shape pointShape;
};

std::vector<LineAppearance> appearances = {
  {Qt::green, CIRCLE},
  {Qt::red, SQUARE},
  {Qt::blue, TRIANGLE},
  {Qt::yellow, CROSS},
  {Qt::cyan, SQUARE},
  {Qt::magenta, TRIANGLE},
  {Qt::darkGreen, CROSS},
  {Qt::darkYellow, CIRCLE}
};




const int MARGIN = 5;
const int NUMBERMARGIN = 3;

ChartPainter::ChartPainter(QWidget* parent)
  : QFrame (parent),
  maxY_(0),
  xWindowCount_(0),
  numberHeight_(0),
  numberWidth_(0),
  yLines_(0),
  points_()
{}


ChartPainter::~ChartPainter()
{}


int ChartPainter::getDrawMinX() const
{
  return MARGIN + numberWidth_ + NUMBERMARGIN;
}


int ChartPainter::getDrawMaxX() const
{
  return rect().size().width() - MARGIN - NUMBERMARGIN;
}


int ChartPainter::getDrawMinY() const
{
  return MARGIN + numberHeight_/4;
}


int ChartPainter::getDrawMaxY() const
{
  return rect().size().height() - MARGIN - NUMBERMARGIN;
}


void ChartPainter::clearPoints()
{
  for (auto points : points_)
  {
    points->clear();
  }
}

void ChartPainter::init(int maxY, int yLines, int xWindowSize)
{
  maxY_ = maxY;
  xWindowCount_ = xWindowSize;
  yLines_ = yLines;
}


// return line ID
int ChartPainter::addLine(QString name)
{
  names_.push_back(name);
  points_.push_back(std::make_shared<std::deque<float>>());
  return points_.size();
}

void ChartPainter::addPoint(int lineID, float y)
{
  if (lineID > points_.size())
  {
    return;
  }

  points_.at(lineID - 1)->push_front(y);
  if (points_.size() > xWindowCount_)
  {
    points_.pop_back();
  }
}


void ChartPainter::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);
  QPainter painter(this);

  numberWidth_ = QFontMetrics(painter.font()).size(Qt::TextSingleLine,
                                                   QString::number(maxY_)).width();
  numberHeight_ = QFontMetrics(painter.font()).size(Qt::TextSingleLine,
                                                    QString::number(maxY_)).height();

  drawBackground(painter);

  for (unsigned int i = 0; i < points_.size(); ++i)
  {
    drawPoints(painter, i + 1);
  }

  drawForeground(painter);
}


void ChartPainter::drawBackground(QPainter& painter)
{
  painter.fillRect(rect(), QBrush(QColor(250,250,250)));
  painter.setPen(QPen(Qt::gray, 1, Qt::SolidLine, Qt::RoundCap));

  int drawHeight = getDrawMaxY() - getDrawMinY();

  for (int i = 1; i <= yLines_; ++i)
  {
    int yLoc = getDrawMaxY() - float(i)/yLines_*drawHeight;
    painter.drawLine(getDrawMinX(), yLoc, getDrawMaxX(), yLoc);
  }
}


void ChartPainter::drawPoints(QPainter& painter, int lineID)
{
  if (lineID > points_.size())
  {
    return;
  }

  int appearanceIndex = (lineID - 1)%appearances.size();

  painter.setPen(QPen(appearances.at(appearanceIndex).color,
                      2, Qt::SolidLine, Qt::RoundCap));

  int drawLength = getDrawMaxX() - getDrawMinX();
  int drawHeight = getDrawMaxY() - getDrawMinY();

  int previousX = 0;
  int previousY = 0;

  for (unsigned int i = 0; i < points_.at(lineID - 1)->size(); ++i)
  {
    int xPoint = getDrawMinX() + float(i)/(xWindowCount_ - 1)*drawLength;
    int yPoint = getDrawMaxY() - points_.at(lineID - 1)->at(i)/maxY_*drawHeight;


    switch (appearances.at(appearanceIndex).pointShape)
    {
      case CIRCLE:
      {
        painter.drawEllipse(QPointF(xPoint, yPoint), 3, 3);
        break;
      }
      case SQUARE:
      {
      QPointF points[4] = {
          QPointF(xPoint - 3, yPoint - 3),
          QPointF(xPoint - 3, yPoint + 3),
          QPointF(xPoint + 3, yPoint + 3),
          QPointF(xPoint + 3, yPoint - 3)

      };
        painter.drawConvexPolygon(points, 4);
        break;
      }
      case TRIANGLE:
      {
      QPointF points[3] = {
          QPointF(xPoint, yPoint - 3),
          QPointF(xPoint - 3, yPoint + 3),
          QPointF(xPoint + 3, yPoint - 3)
      };
        painter.drawConvexPolygon(points, 3);
        break;
      }
      case CROSS:
      {
      painter.drawLine(xPoint - 3, yPoint - 3,
                       xPoint + 3, yPoint + 3);
      painter.drawLine(xPoint - 3, yPoint + 3,
                       xPoint + 3, yPoint - 3);
        break;
      }
    }

    if (i != 0)
    {
      painter.drawLine(previousX, previousY, xPoint, yPoint);
    }

    previousX = xPoint;
    previousY = yPoint;
  }
}


void ChartPainter::drawForeground(QPainter& painter)
{
  painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::FlatCap));

  // x-axis
  painter.drawLine(getDrawMinX(),      getDrawMinY(),
                   getDrawMinX(),      getDrawMaxY());

  // y-axis
  painter.drawLine(getDrawMinX(),      getDrawMaxY(),
                   getDrawMaxX(),      getDrawMaxY());

  // max x
  painter.drawText(MARGIN, MARGIN + numberHeight_/2, QString::number(maxY_));

  // small nod for max x
  painter.drawLine(getDrawMinX(),           getDrawMinY(),
                   getDrawMinX() + 3,       getDrawMinY());

  // mix x
  int zeroLength = QFontMetrics(painter.font()).size(Qt::TextSingleLine,
                                                     QString::number(0)).width();
  painter.drawText(getDrawMinX() - zeroLength - NUMBERMARGIN,
                   rect().size().height() - MARGIN - NUMBERMARGIN + numberHeight_/4,
                   QString::number(0));
}


void ChartPainter::resizeEvent(QResizeEvent *event)
{}

void ChartPainter::keyPressEvent(QKeyEvent *event)
{}

void ChartPainter::mouseDoubleClickEvent(QMouseEvent *e)
{}
