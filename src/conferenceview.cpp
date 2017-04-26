#include "conferenceview.h"

#include "videowidget.h"

#include <QLabel>
#include <QGridLayout>

ConferenceView::ConferenceView(QWidget *parent):
  parent_(parent),
  layout_(NULL),
  row_(0),
  column_(0)
{}

void ConferenceView::init(QGridLayout* conferenceLayout)
{
  layout_ = conferenceLayout;
}

void ConferenceView::callingTo(QString callID)
{
  hideLabel();

  QLabel* label = new QLabel(parent_);
  label->setText("Calling...");

  QFont font = QFont("Times", 16);
  label->setFont(font);
  label->setAlignment(Qt::AlignHCenter);
  layout_->addWidget(label, row_, column_);
}

void ConferenceView::incomingCall()
{}


// if our call is accepted or we accepted their call
VideoWidget* ConferenceView::addParticipant(QString callID)
{
  VideoWidget* view = new VideoWidget;

  layout_->addWidget(view, row_, column_);

  // TODO improve this algorithm for more optimized layout
  ++column_;
  if(column_ == 3)
  {
    column_ = 0;
    ++row_;
  }

  return view;
}

void ConferenceView::removeCaller(QString callID)
{
    hideLabel();
}

void ConferenceView::hideLabel()
{
  QLayoutItem* label = layout_->itemAtPosition(row_,column_);
  if(label)
    label->widget()->hide();
}
