#pragma once

#include <QDialog>

namespace Ui {
class StatisticsWindow;
}

class StatisticsWindow : public QDialog
{
  Q_OBJECT

public:
  explicit StatisticsWindow(QWidget *parent);
  ~StatisticsWindow();

  void closeEvent(QCloseEvent *event);

private:
  Ui::StatisticsWindow *ui_;
};
