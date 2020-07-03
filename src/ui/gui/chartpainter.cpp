#include "chartpainter.h"

#include <QPainter>
#include <QPaintEvent>


const int MARGIN = 5;
const int NUMBERMARGIN = 2;

ChartPainter::ChartPainter(QWidget* parent)
  : QFrame (parent),
  maxY_(0),
  xWindowCount_(0),
  numberHeight_(0),
  numberWidth_(0),
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


void ChartPainter::init(int maxY, int yLines, int xWindowSize)
{
  maxY_ = maxY;
  xWindowCount_ = xWindowSize;
}


void ChartPainter::addPoint( float y)
{
  points_.push_front(y);
  if (points_.size() > xWindowCount_)
  {
    points_.pop_back();
  }
}


void ChartPainter::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);
  QPainter painter(this);

  numberWidth_ = QFontMetrics(painter.font()).size(Qt::TextSingleLine, "000").width();
  numberHeight_ = QFontMetrics(painter.font()).size(Qt::TextSingleLine, "000").height();

  drawBackground(painter);


  drawPoints(painter);
  drawForeground(painter);
}


void ChartPainter::drawBackground(QPainter& painter)
{
  painter.fillRect(rect(), QBrush(QColor(250,250,250)));
}


void ChartPainter::drawPoints(QPainter& painter)
{
  painter.setPen(QPen(Qt::green, 2, Qt::SolidLine, Qt::RoundCap));

  int drawLength = getDrawMaxX() - getDrawMinX();
  int drawHeight = getDrawMaxY() - getDrawMinY();

  int previousX = 0;
  int previousY = 0;

  for (int i = 0; i < points_.size(); ++i)
  {
    int xPoint = getDrawMinX() + float(i)/(xWindowCount_ - 1)*drawLength;
    int yPoint = getDrawMaxY() - points_.at(i)/maxY_*drawHeight;

    painter.drawEllipse(QPointF(xPoint, yPoint), 3, 3);

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
  int numberWidth = QFontMetrics(painter.font()).size(Qt::TextSingleLine, "000").width();
  int numberHeight = QFontMetrics(painter.font()).size(Qt::TextSingleLine, "000").height();

  painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::FlatCap));

  // x-axis
  painter.drawLine(getDrawMinX(),      getDrawMinY(),
                   getDrawMinX(),      getDrawMaxY());

  // y-axis
  painter.drawLine(getDrawMinX(),      getDrawMaxY(),
                   getDrawMaxX(),      getDrawMaxY());

  // max x
  painter.drawText(MARGIN, MARGIN + numberHeight/2, QString::number(maxY_));

  // small
  painter.drawLine(getDrawMinX(),           getDrawMinY(),
                   getDrawMinX() + 3,       getDrawMinY());
}


void ChartPainter::resizeEvent(QResizeEvent *event)
{}

void ChartPainter::keyPressEvent(QKeyEvent *event)
{}

void ChartPainter::mouseDoubleClickEvent(QMouseEvent *e)
{}
