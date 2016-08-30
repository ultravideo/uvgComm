#pragma once

#include <QMainWindow>

#include "filtergraph.h"

class StatisticsWindow;

namespace Ui {
class CallWindow;
}

class CallWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit CallWindow(QWidget *parent, uint16_t width, uint16_t height);
  ~CallWindow();

  void startStream();

  void closeEvent(QCloseEvent *event);

public slots:
   void addParticipant();
   void openStatistics();

private:
  Ui::CallWindow *ui_;
  StatisticsWindow *stats_;

  FilterGraph fg_;

  unsigned int participants_;

  QTimer *timer_;

  int row_;
  int column_;

  QSize currentResolution_;
};
