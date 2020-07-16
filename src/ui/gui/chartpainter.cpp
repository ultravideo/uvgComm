#include "chartpainter.h"

#include "common.h"

#include <QPainter>
#include <QPaintEvent>


enum Shape {CIRCLE, SQUARE, TRIANGLE, CROSS};

struct LineAppearance
{
  QColor color;
  Shape pointShape;
};

// what the points will look like
const std::vector<LineAppearance> appearances = {
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
  adaptiveLines_(true),
  yLines_(0),
  overLines_(0),
  points_(),
  names_(),
  maxNameWidth_(0),
  title_(""),
  titleSize_(),
  legendRows_(0),
  font_(QFont("times", 10))
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
  // removes points from lines
  for (auto points : points_)
  {
    points->clear();
  }
}

void ChartPainter::init(int maxY, int yLines, bool adaptive, int xWindowSize,
                        QString chartTitle)
{
  Q_ASSERT(maxY >= 1);
  Q_ASSERT(yLines >= 1);
  Q_ASSERT(xWindowSize >= 2);

  printNormal(this, "Initiating chart", "Title", chartTitle);

  // just set variables
  maxY_ = maxY;
  xWindowCount_ = xWindowSize;
  yLines_ = yLines;
  title_ = chartTitle;
  adaptiveLines_ = adaptive;
}


// return line ID
int ChartPainter::addLine(QString name)
{
  Q_ASSERT(name != "" && !name.isNull());

  lineMutex_.lock();
  // lineID should refer to position in both arrays
  names_.push_back(name);
  points_.push_back(std::make_shared<std::deque<float>>());
  int lineID = points_.size();

  // check if this is the widest name of all for drawing the legends
  int newWidth = QFontMetrics(font_).size(Qt::TextSingleLine, name).width();
  if (newWidth > maxNameWidth_)
  {
    maxNameWidth_ = newWidth;
  }
  lineMutex_.unlock();
  return lineID;
}


void ChartPainter::removeLine(int lineID)
{
  Q_ASSERT(lineID > 0);

  lineMutex_.lock();
  if (names_.size() >= lineID)
  {
    // lineID refers to both
    names_.erase(names_.begin() + lineID - 1);
    points_.erase(points_.begin() + lineID - 1);
  }
  lineMutex_.unlock();
}


void ChartPainter::addPoint(int lineID, float y)
{
  Q_ASSERT(lineID > 0);
  Q_ASSERT(y >= 0);
  Q_ASSERT(yLines_ > 0);

  lineMutex_.lock();
  Q_ASSERT(lineID <= points_.size());
  Q_ASSERT(points_.at(lineID - 1) != nullptr);

  // check that lineID makes sense
  if (lineID > points_.size() || points_.at(lineID - 1) == nullptr)
  {
    lineMutex_.unlock();
    return;
  }

  // add point as newest
  points_.at(lineID - 1)->push_front(y);

  // remove oldest if we have enough points.
  if (points_.at(lineID - 1)->size() > xWindowCount_)
  {
    points_.at(lineID - 1)->pop_back();
  }

  // adapt to change in values. This makes sure that the chart keeps up if values
  // rise over the maximum or fall to the bottom part of the graph
  if (adaptiveLines_)
  {
    // if the removed is in the
    // TODO: Fix this
    if (y < (maxY_/yLines_)*(yLines_ - 1))
    {
      // got through all the points in all lines to find the largest one
      float largest = y;
      for (auto& line : points_)
      {
        for (auto& value : *line.get())
        {
          if (value > largest)
          {
            largest = value;
          }
        }
      }

      // Divide the maximum and number of lines by two when all points are
      // in bottom half. This way the jumps are not so frequent.
      // Also only divide if we have enough yLines left.
      // There is also a system where the yLines are not increased
      // if there would be too many of them and this is synchronized using
      // overLines_ variable
      while (largest < maxY_/2 && yLines_ > 5)
      {
        maxY_ /= 2;

        if (overLines_ == 0)
        {
          yLines_ /= 2;
        }
        else // no yLines division for now
        {
          --overLines_;
        }
      }
    }
    else
    {
      // if y is larger than maximum, then double the maxY and yLines
      while (maxY_ < y)
      {
        maxY_ *= 2;

        // only double until 10
        if (yLines_*2 <= 10)
        {
          yLines_ *= 2;
        }
        else // put multiplication in storage so we know not to divide later
        {
          ++overLines_;
        }
      }
    }
  }
  // if value over the chart maximum and we dont have adaptivity to deal with it
  else if (maxY_ + maxY_/5 < y)
  {
    // force the adaptivity on
    adaptiveLines_ = true;
  }

  lineMutex_.unlock();
}


void ChartPainter::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  // paint everything from scratch
  QPainter painter(this);

  painter.setFont(font_);

  // calculate sizes to be used when determining draw are limits
  numberSize_ = QFontMetrics(painter.font()).size(Qt::TextSingleLine,
                                                  QString::number(maxY_));

  titleSize_ = QFontMetrics(painter.font()).size(Qt::TextSingleLine, title_);

  // draw everything that is on the background first
  drawBackground(painter);

  bool drawZero = true;
  bool drawMax = true;

  lineMutex_.lock();
  for (unsigned int i = 0; i < points_.size(); ++i)
  {
    drawPoints(painter, i + 1, drawZero, drawMax);
  }

  // draw stuff like legends and numbers
  drawForeground(painter, drawZero, drawMax);
  lineMutex_.unlock();
}


void ChartPainter::drawBackground(QPainter& painter)
{
  painter.fillRect(rect(), QBrush(QColor(250,250,250)));
  painter.setPen(QPen(Qt::gray, 1, Qt::SolidLine, Qt::RoundCap));

  int drawHeight = getDrawMaxY() - getDrawMinY();

  // draw y-lines
  if (drawHeight > 0 && yLines_ > 0)
  {
    for (int i = 1; i <= yLines_; ++i)
    {
      int yLoc = getDrawMaxY() - float(i)/yLines_*drawHeight;
      painter.drawLine(getDrawMinX(), yLoc, getDrawMaxX(), yLoc);
    }
  }

  painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::RoundCap));

  // chart title
  painter.drawText(rect().width()/2 - titleSize_.width()/2,
                   MARGIN/2 + titleSize_.height(), title_);
}


void ChartPainter::drawPoints(QPainter& painter, int lineID,
                              bool& outDrawZero, bool& outDrawMax)
{
  Q_ASSERT(lineID >= 1);
  Q_ASSERT(lineID <= points_.size());
  Q_ASSERT(points_.at(lineID - 1) != nullptr);

  // check that lineID makes sense
  if (lineID == 0 || lineID > points_.size() || points_.at(lineID - 1) == nullptr)
  {
    return;
  }

  // we loop the outlook when we have too many lines
  int appearanceIndex = (lineID - 1)%appearances.size();

  painter.setPen(QPen(appearances.at(appearanceIndex).color,
                      2, Qt::SolidLine, Qt::RoundCap));

  // get the actual size of draw area
  int drawLength = getDrawMaxX() - getDrawMinX();
  int drawHeight = getDrawMaxY() - getDrawMinY();

  int previousX = 0;
  int previousY = 0;


  for (unsigned int i = 0; i < points_.at(lineID - 1)->size(); ++i)
  {
    // get points position
    int xPoint = getDrawMinX() + float(i)/(xWindowCount_ - 1)*drawLength;
    int yPoint = getDrawMaxY() - points_.at(lineID - 1)->at(i)/maxY_*drawHeight;

    drawMark(painter, lineID, xPoint, yPoint);

    // draw a number on left. Disable min/max if they get in the way
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
                       QString::number(points_.at(lineID - 1)->at(i), 10, 0));

    }
    else // draw line if we have at least two points
    {
      painter.drawLine(previousX, previousY, xPoint, yPoint);
    }

    // saved for drawing the line
    previousX = xPoint;
    previousY = yPoint;
  }
}


void ChartPainter::drawForeground(QPainter& painter, bool drawZero, bool drawMax)
{
  painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::FlatCap));

  // draw x-axis
  painter.drawLine(getDrawMinX(),      getDrawMinY(),
                   getDrawMinX(),      getDrawMaxY());

  // draw y-axis
  painter.drawLine(getDrawMinX(),      getDrawMaxY(),
                   getDrawMaxX(),      getDrawMaxY());

  // max number
  if (drawMax)
  {
    // max x
    painter.drawText(MARGIN, MARGIN + numberSize_.height()/2 + titleSize_.height(),
                     QString::number(maxY_));

    // small nod for max x
    painter.drawLine(getDrawMinX(),           getDrawMinY(),
                     getDrawMinX() + 3,       getDrawMinY());
  }

  // min number
  if (drawZero) // TODO: The y in this one is not correct
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
    // draw legends

    int legendMargin = 3;
    int markSize = 7;

    int legendWidth = markSize + legendMargin + maxNameWidth_ + 2;
    int totalWidthOfTwo = legendWidth*2;
    int totalWidthOfThree = legendWidth*3;

    int wordSpace = 14;

    // draw each legend
    for (unsigned int i = 0; i < points_.size(); ++i)
    {
      // Draw 3 legends on one row. First check that this makes sense
      // and that we have enough space.
      if (points_.size() > 2 &&
          points_.size() != 4 &&
          rect().width() >= totalWidthOfThree + 2 * (legendMargin + wordSpace))
      {
        int extraSpace = rect().width() - totalWidthOfThree;
        // location
        int x = extraSpace/2 + (i%3)*(legendWidth + wordSpace);
        int y = getDrawMaxY() + NUMBERMARGIN + (i/3)*titleSize_.height();

        drawLegend(painter, x, y, legendMargin, markSize, i + 1, names_.at(i));

        // calculate how many rows our legend takes
        if (points_.size()%3 != 0)
        {
          legendRows_ = points_.size()/3 + 1;
        }
        else
        {
          legendRows_ = points_.size()/3;
        }
      }
      // Draw 2 legends on one row.
      else if (points_.size() >= 2 &&
               rect().width() > totalWidthOfTwo + wordSpace)
      {
        int extraSpace = rect().width() - totalWidthOfTwo;
        // location
        int x = extraSpace/2 + (i%2)*(legendWidth + wordSpace);
        int y = getDrawMaxY() + NUMBERMARGIN + (i/2)*titleSize_.height();

        drawLegend(painter, x, y, legendMargin, markSize, i + 1, names_.at(i));

        // how many rows the legend takes
        if (points_.size()%2 != 0)
        {
          legendRows_ = points_.size()/2 + 1;
        }
        else
        {
          legendRows_ = points_.size()/2;
        }
      }
      // Draw one legend per row.
      else
      {
        int extraSpace = rect().width() - legendWidth;
        // location
        int x = extraSpace/2;
        int y = getDrawMaxY() + i*titleSize_.height() + NUMBERMARGIN;

        drawLegend(painter, x, y, legendMargin, markSize, i + 1, names_.at(i));

        legendRows_ = points_.size();
      }
    }
  }
}


void ChartPainter::drawLegend(QPainter& painter, float x, float y,
                              int legendMargin, int markSize, int lineID,
                              QString name)
{
  if (x < rect().width() && y < rect().height())
  {
    drawMark(painter, lineID, x, y + titleSize_.height()/2 + markSize/2 + 1);

    // draw the legend text
    painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::FlatCap));
    painter.drawText(QPointF(x + markSize + legendMargin, y + titleSize_.height()),
                     name);
  }
}


void ChartPainter::drawMark(QPainter& painter, int lineID, float x, float y)
{
  Q_ASSERT(lineID >= 1);

  // Get appearance. We loop appearances once all are exauhsted
  int appearanceIndex = (lineID - 1)%appearances.size();

  painter.setPen(QPen(appearances.at(appearanceIndex).color,
                      2, Qt::SolidLine, Qt::FlatCap));

  // TODO: Test/fix these
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



void ChartPainter::resizeEvent(QResizeEvent *event)
{}


void ChartPainter::keyPressEvent(QKeyEvent *event)
{}


void ChartPainter::mouseDoubleClickEvent(QMouseEvent *e)
{}
