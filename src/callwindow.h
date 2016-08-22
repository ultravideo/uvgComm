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
  explicit CallWindow(QWidget *parent);
  ~CallWindow();

  void startStream();

  void closeEvent(QCloseEvent *event);

public slots:
   void addParticipant();

private:
  Ui::CallWindow *ui_;

  FilterGraph fg_;

  unsigned int participants_;

  QTimer *timer_;
};
