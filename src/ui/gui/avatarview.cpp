#include "avatarview.h"
#include "ui_avatarview.h"

AvatarView::AvatarView(QWidget *parent) :
  QWidget(parent),
  ui(new Ui::AvatarView)
{
  ui->setupUi(this);
}

AvatarView::~AvatarView()
{
  delete ui;
}
