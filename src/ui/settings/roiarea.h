#pragma once

#include <QWidget>

namespace Ui {
class RoiArea;
}

class VideoWidget;

class RoiArea : public QWidget
{
  Q_OBJECT

public:
  explicit RoiArea(QWidget *parent = nullptr);
  ~RoiArea();

  VideoWidget* getSelfVideoWidget();

signals:

  void closed();

private:
  Ui::RoiArea *ui_;
};
