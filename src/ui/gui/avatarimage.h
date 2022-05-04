#pragma once

#include <QWidget>
#include <QSvgRenderer>

class AvatarImage : public QWidget
{
public:
  AvatarImage(QWidget* parent = nullptr);

protected:
  void paintEvent(QPaintEvent *event);

private:
  QSvgRenderer  image_;
};
