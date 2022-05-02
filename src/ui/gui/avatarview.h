#pragma once

#include <QWidget>

namespace Ui {
class AvatarView;
}

class AvatarView : public QWidget
{
  Q_OBJECT

public:
  explicit AvatarView(QWidget *parent = nullptr);
  ~AvatarView();

private:
  Ui::AvatarView *ui;
};
