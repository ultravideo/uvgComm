#include "avatarview.h"
#include "ui_avatarview.h"


AvatarView::AvatarView(QWidget *parent) :
  QWidget(parent),
  ui_(new Ui::AvatarView)
{
  ui_->setupUi(this);
}


AvatarView::~AvatarView()
{
  delete ui_;
}


void AvatarView::setName(QString name)
{
  ui_->name->setText(name);
}
