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
  {Qt::darkYellow, CROSS},
  {Qt::cyan, SQUARE},
  {Qt::magenta, TRIANGLE},
  {Qt::darkGreen, CROSS},
  {Qt::darkRed, CIRCLE}
};


const int MARGIN = 5;
const int NUMBERMARGIN = 3;

ChartPainter::ChartPainter(QWidget* parent)
  : QFrame (parent),
  maxY_(0),
  xWindowCount_(0),
  numberSize_(),
  yLines_(0),
  points_(),
  names_(),
  title_(""),
  titleSize_(),
  legendRows_(0)
{}


ChartPainter::~ChartPainter()
{}


int ChartPainter::getDrawMinX() const
{
  return MARGIN + numberSize_.width() + NUMBERMARGIN;
}


int ChartPainter::getDrawMaxX() const
{
  return rect().size().width() - MARGIN - NUMBERMARGIN;
}


int ChartPainter::getDrawMinY() const
{
  return MARGIN + numberSize_.height()/4 + titleSize_.height();
}


int ChartPainter::getDrawMaxY() const
{
  return rect().size().height() - MARGIN - NUMBERMARGIN
      - titleSize_.height()*legendRows_;
}


void ChartPainter::clearPoints()
{
  for (auto points : points_)
  {
    points->clear();
  }
}

void ChartPainter::init(int maxY, int yLines, int xWindowSize,
                        QString chartTitle)
{
  maxY_ = maxY;
  xWindowCount_ = xWindowSize;
  yLines_ = yLines;
  title_ = chartTitle;
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

  // remove oldest if we have enough points.
  if (points_.at(lineID - 1)->size() > xWindowCount_)
  {
    points_.at(lineID - 1)->pop_back();

    if (y < (maxY_/yLines_)*(yLines_ - 1))
    {
      bool inLastSegment = false;
      for (auto& line : points_)
      {
        for (auto& value : *line.get())
        {
          if (value >= (maxY_/yLines_)*(yLines_ - 1))
          {
            inLastSegment = true;
            break;
          }
        }
        if (inLastSegment)
        {
          break;
        }
      }

      if (!inLastSegment && yLines_ > 1)
      {
        maxY_ -= maxY_/yLines_;
        --yLines_;
      }
    }
  }

  while (maxY_ < y)
  {
    maxY_ += maxY_/yLines_;
    ++yLines_;
  }
}


void ChartPainter::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);
  QPainter painter(this);

  numberSize_ = QFontMetrics(painter.font()).size(Qt::TextSingleLine,
                                                  QString::number(maxY_));

  titleSize_ = QFontMetrics(painter.font()).size(Qt::TextSingleLine, title_);


  drawBackground(painter);

  bool drawZero = true;
  bool drawMax = true;

  for (unsigned int i = 0; i < points_.size(); ++i)
  {
    drawPoints(painter, i + 1, drawZero, drawMax);
  }

  drawForeground(painter, drawZero, drawMax);
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

  painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::RoundCap));

  painter.drawText(rect().width()/2 - titleSize_.width()/2,
                   MARGIN/2 + titleSize_.height(), title_);
}


void ChartPainter::drawPoints(QPainter& painter, int lineID,
                              bool& outDrawZero, bool& outDrawMax)
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

    drawMark(painter, lineID, xPoint, yPoint);

    if (i == 0)
    {
      // we don't want to draw min/max if the current value would overlap them
      if (points_.at(lineID - 1)->at(i) < maxY_/10)
      {
        outDrawZero = false;
      }
      else if (points_.at(lineID - 1)->at(i) > 9*maxY_/10)
      {
        outDrawMax = false;
      }

      // draw current value
      painter.drawText(MARGIN, yPoint + numberSize_.height()/4,
                       QString::number(points_.at(lineID - 1)->at(i)));

    }
    else // draw line if we have to points
    {
      painter.drawLine(previousX, previousY, xPoint, yPoint);
    }

    previousX = xPoint;
    previousY = yPoint;
  }
}


void ChartPainter::drawForeground(QPainter& painter, bool drawZero, bool drawMax)
{
  painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::FlatCap));

  // x-axis
  painter.drawLine(getDrawMinX(),      getDrawMinY(),
                   getDrawMinX(),      getDrawMaxY());

  // y-axis
  painter.drawLine(getDrawMinX(),      getDrawMaxY(),
                   getDrawMaxX(),      getDrawMaxY());

  if (drawMax)
  {
    // max x
    painter.drawText(MARGIN, MARGIN + numberSize_.height()/2 + titleSize_.height(),
                     QString::number(maxY_));

    // small nod for max x
    painter.drawLine(getDrawMinX(),           getDrawMinY(),
                     getDrawMinX() + 3,       getDrawMinY());
  }

  if (drawZero)
  {
    // mix x
    int zeroLength = QFontMetrics(painter.font()).size(Qt::TextSingleLine,
                                                       QString::number(0)).width();
    painter.drawText(getDrawMinX() - zeroLength - NUMBERMARGIN,
                     rect().size().height() - MARGIN - NUMBERMARGIN + numberSize_.height()/4,
                     QString::number(0));
  }

  if (names_.size() > 0 && names_.size() == points_.size())
  {
    // draw legend

    int legendMargin = 3;
    int markSize = 7;

    // +2 is an extra precausion
    int nameWidth = QFontMetrics(painter.font()).size(Qt::TextSingleLine,
                                                      names_.at(0)).width() + 2;

    int legendWidth = markSize + legendMargin + nameWidth;
    int totalWidthOfTwo = legendWidth*2;
    int totalWidthOfThree = legendWidth*3;

    int wordSpace = 14;

    for (unsigned int i = 0; i < points_.size(); ++i)
    {
      if (points_.size() > 2 &&
          points_.size() != 4 &&
          rect().width() >= totalWidthOfThree + 2 * (legendMargin + wordSpace))
      {
        // draw three legends next to each other
        // the if above means this will include at least the margins.
        int extraSpace = rect().width() - totalWidthOfThree;
        int x = extraSpace/2 + (i%3)*(legendWidth + wordSpace);
        int y = getDrawMaxY() + NUMBERMARGIN + (i/3)*titleSize_.height();

        drawLegend(painter, x, y, legendMargin, markSize, i + 1, names_.at(i));

        if (points_.size()%3 != 0)
        {
          legendRows_ = points_.size()/3 + 1;
        }
        else
        {
          legendRows_ = points_.size()/3;
        }
      }
      else if (points_.size() >= 2 &&
               rect().width() > totalWidthOfTwo + wordSpace)
      {
        // draw legends in pairs
        // the if above means this will include at least the margins.
        int extraSpace = rect().width() - totalWidthOfTwo;
        int x = extraSpace/2 + (i%2)*(legendWidth + wordSpace);
        int y = getDrawMaxY() + NUMBERMARGIN + (i/2)*titleSize_.height();

        drawLegend(painter, x, y, legendMargin, markSize, i + 1, names_.at(i));

        if (points_.size()%2 != 0)
        {
          legendRows_ = points_.size()/2 + 1;
        }
        else
        {
          legendRows_ = points_.size()/2;
        }
      }
      else
      {
        // draw one legend per row
        // the if above means this will include at least the margins.
        int extraSpace = rect().width() - legendWidth;
        int x = extraSpace/2;
        int y = getDrawMaxY() + i*titleSize_.height() + NUMBERMARGIN;

        drawLegend(painter, x, y, legendMargin, i + 1, markSize, names_.at(i));

        legendRows_ = points_.size();
      }
    }
  }
}


void ChartPainter::drawMark(QPainter& painter, int lineID, float x, float y)
{
  int appearanceIndex = (lineID - 1)%appearances.size();

  painter.setPen(QPen(appearances.at(appearanceIndex).color,
                      2, Qt::SolidLine, Qt::FlatCap));

  switch (appearances.at(appearanceIndex).pointShape)
  {
    case CIRCLE:
    {
      drawCircle(painter, x, y);
      break;
    }
    case SQUARE:
    {
      drawSquare(painter, x, y);
      break;
    }
    case TRIANGLE:
    {
      drawTriangle(painter, x, y);
      break;
    }
    case CROSS:
    {
      drawCross(painter, x, y);
      break;
    }
  }
}


void ChartPainter::drawCircle(QPainter& painter, float x, float y)
{
  painter.drawEllipse(QPointF(x, y), 3, 3);
}


void ChartPainter::drawSquare(QPainter& painter, float x, float y)
{
  QPointF points[4] = {
      QPointF(x - 3, y - 3),
      QPointF(x - 3, y + 3),
      QPointF(x + 3, y + 3),
      QPointF(x + 3, y - 3)

  };
  painter.drawConvexPolygon(points, 4);
}


void ChartPainter::drawTriangle(QPainter& painter, float x, float y)
{
  QPointF points[3] = {
      QPointF(x,     y - 3),
      QPointF(x - 3, y + 3),
      QPointF(x + 3, y - 3)
  };
  painter.drawConvexPolygon(points, 3);
}


void ChartPainter::drawCross(QPainter& painter, float x, float y)
{
  painter.drawLine(x - 3, y - 3,
                   x + 3, y + 3);
  painter.drawLine(x - 3, y + 3,
                   x + 3, y - 3);
}

void ChartPainter::drawLegend(QPainter& painter, float x, float y,
                              int legendMargin, int markSize, int lineID, QString name)
{
  drawMark(painter, lineID, x, y + titleSize_.height()/2 + markSize - 1);
  painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::FlatCap));
  painter.drawText(QPointF(x + markSize + legendMargin, y + titleSize_.height()), name);
}


void ChartPainter::resizeEvent(QResizeEvent *event)
{}

void ChartPainter::keyPressEvent(QKeyEvent *event)
{}

void ChartPainter::mouseDoubleClickEvent(QMouseEvent *e)
{}
