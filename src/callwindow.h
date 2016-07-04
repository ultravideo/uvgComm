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
  explicit CallWindow(QWidget *parent,
                      in_addr ip, uint16_t port);
  ~CallWindow();

  void startStream();

  void closeEvent(QCloseEvent *event);

private:
  Ui::CallWindow *ui;

  FilterGraph video_;
  FilterGraph audio_;
};
