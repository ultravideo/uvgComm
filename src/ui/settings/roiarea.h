#pragma once

#include <QWidget>

namespace Ui {
class RoiArea;
}

class RoiArea : public QWidget
{
  Q_OBJECT

public:
  explicit RoiArea(QWidget *parent = nullptr);
  ~RoiArea();

private:
  Ui::RoiArea *ui;
};
