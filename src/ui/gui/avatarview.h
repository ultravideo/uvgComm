#pragma once

#include <QWidget>
#include <QSvgRenderer>

namespace Ui {
class AvatarView;
}

class AvatarView : public QWidget
{
  Q_OBJECT

public:
  explicit AvatarView(QWidget *parent = nullptr);
  ~AvatarView();

  void setName(QString name);

private:
  Ui::AvatarView *ui_;
};
