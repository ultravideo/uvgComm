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
  {QColor(0,   0,   245), CIRCLE},   // blue
  {QColor(230, 0,   230), SQUARE},   // violet
  {QColor(180, 180, 0  ), CROSS},    // yellow
  {QColor(200, 100, 0  ), TRIANGLE}, // orange
  {QColor(0,   245, 0  ), CIRCLE},    // green
  {QColor(245, 0,   0  ), SQUARE},   // red
  {QColor(150, 150, 245), CROSS}, // light blue
  {QColor(0,   245, 245), TRIANGLE}    // cyan
};


const int MARGIN = 5;
const int NUMBERMARGIN = 6;
const int TITLEMARGIN = 15;

ChartPainter::ChartPainter(QWidget* parent)
  : QFrame (parent),
  maxY_(0),
  maxYSize_(0,0),
  xWindowCount_(0),
  adaptiveLines_(true),
  yLines_(0),
  overLines_(0),
  points_(),
  legends_(),
  legendRows_(0),
  legendSize_(0,0),
  title_(""),
  titleSize_(0,0),

  font_(QFont("times", 14))
{}


ChartPainter::~ChartPainter()
{}


int ChartPainter::getDrawMinX() const
{
  return MARGIN + maxYSize_.width() + NUMBERMARGIN;
}


int ChartPainter::getDrawMaxX() const
{
  return rect().size().width() - MARGIN - NUMBERMARGIN;
}


int ChartPainter::getDrawMinY() const
{
  return MARGIN + TITLEMARGIN + titleSize_.height();
}


int ChartPainter::getDrawMaxY() const
{
  return rect().size().height() - MARGIN - NUMBERMARGIN
      - legendSize_.height()*legendRows_;
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
  legends_.push_back(name);
  points_.push_back(std::make_shared<std::deque<float>>());
  int lineID = points_.size();

  // check if this is the widest name of all for drawing the legends
  QSize newSize = QFontMetrics(font_).size(Qt::TextSingleLine, name);
  if (newSize.width() > legendSize_.width())
  {
    legendSize_ = newSize;
  }
  lineMutex_.unlock();
  return lineID;
}


void ChartPainter::removeLine(int lineID)
{
  Q_ASSERT(lineID > 0);

  lineMutex_.lock();
  if (legends_.size() >= lineID)
  {
    // lineID refers to both
    legends_.erase(legends_.begin() + lineID - 1);
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
    // if the removed is in the bottom half
    if (y < maxY_/2)
    {
      // Consider division only if we have enough yLines left.
      if (yLines_ > 5)
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
        // There is also a system where the yLines are not increased
        // if there would be too many of them and this is synchronized using
        // overLines_ variable.
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
    }
    else
    {
      // if y is larger than maximum, then double the maxY and yLines
      while (y > maxY_)
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
  else if (maxY_ + maxY_/10 < y)
  {
    // force the adaptivity on
    // This assumes that we never want the graph to be too much over the maximum.
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
  maxYSize_ = QFontMetrics(painter.font()).size(Qt::TextSingleLine,
                                                QString::number(maxY_));

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

  // chart title
  QFont titleFont = QFont("times", 16);
  painter.setFont(titleFont);
  titleSize_ = QFontMetrics(painter.font()).size(Qt::TextSingleLine, title_);

  painter.drawText(rect().width()/2 - titleSize_.width()/2,
                   MARGIN + titleSize_.height(), title_);

  // return font to normal
  painter.setFont(font_);

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
      QString number = QString::number(points_.at(lineID - 1)->at(i), 10, 0);
      QSize numberSize = QFontMetrics(painter.font()).size(Qt::TextSingleLine,
                                                           number);

      painter.drawText(getDrawMinX() - numberSize.width() - NUMBERMARGIN,
                       yPoint + maxYSize_.height()/4, number);

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
    painter.drawText(getDrawMinX() - maxYSize_.width() - NUMBERMARGIN,
                     getDrawMinY() + maxYSize_.height()/4 + 1,
                     QString::number(maxY_));

    // small nod for max x
    painter.drawLine(getDrawMinX(),           getDrawMinY(),
                     getDrawMinX() + 3,       getDrawMinY());
  }

  // min number
  if (drawZero)
  {
    // mix x
    QSize zeroSize = QFontMetrics(painter.font()).size(Qt::TextSingleLine,
                                                       QString::number(0));
    painter.drawText(getDrawMinX() - zeroSize.width() - NUMBERMARGIN,
                     getDrawMaxY() + zeroSize.height()/4 + 1,
                     QString::number(0));
  }

  if (legends_.size() > 0 && legends_.size() == points_.size())
  {
    // draw legends

    int legendMargin = 3;
    int markSize = 7;

    int legendWidth = markSize + legendMargin + legendSize_.width() + 2;
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
        int y = getDrawMaxY() + NUMBERMARGIN + (i/3)*legendSize_.height();

        drawLegend(painter, x, y, legendMargin, markSize, i + 1, legends_.at(i));

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
        int y = getDrawMaxY() + NUMBERMARGIN + (i/2)*legendSize_.height();

        drawLegend(painter, x, y, legendMargin, markSize, i + 1, legends_.at(i));

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
        int y = getDrawMaxY() + i*legendSize_.height() + NUMBERMARGIN;

        drawLegend(painter, x, y, legendMargin, markSize, i + 1, legends_.at(i));

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
    drawMark(painter, lineID, x, y + legendSize_.height()/2 + markSize/2 + 1);

    // draw the legend text
    painter.setPen(QPen(Qt::black, 1, Qt::SolidLine, Qt::FlatCap));
    painter.drawText(QPointF(x + markSize + legendMargin, y + legendSize_.height()),
                     name);
  }
}


void ChartPainter::drawMark(QPainter& painter, int lineID, float x, float y)
{
  Q_ASSERT(lineID >= 1);

  // Get appearance. We loop appearances once all are exauhsted
  int appearanceIndex = (lineID - 1)%appearances.size();

  painter.setPen(QPen(appearances.at(appearanceIndex).color,
                      2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));

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
      QPointF(x - 3, y - 3),
      QPointF(x + 3, y - 3),
      QPointF(x,     y + 3)
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
