#pragma once

#include <QMainWindow>

#include "filtergraph.h"

namespace Ui {
class CallWindow;
}

class CallWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit CallWindow(QWidget *parent = 0);
  ~CallWindow();

  void startStream();


private:
  Ui::CallWindow *ui;

  FilterGraph video_;
  FilterGraph audio_;
};
