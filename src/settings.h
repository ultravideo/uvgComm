#pragma once

#include <QDialog>

namespace Ui {
class Settings;
}

class Settings : public QDialog
{
  Q_OBJECT

public:
  explicit Settings(QWidget *parent = 0);
  ~Settings();

  void getSettings(std::map<QString, QString>& settings);

private:
  Ui::Settings *ui;
};
