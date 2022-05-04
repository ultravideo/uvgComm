#include "avatarimage.h"

#include <QPainter>

AvatarImage::AvatarImage(QWidget *parent)
  : QWidget(parent),
    image_(QString("icons/avatar.svg"))
{}


void AvatarImage::paintEvent(QPaintEvent *event)
{
  QPainter painter(this);
  image_.render(&painter, rect());
}
