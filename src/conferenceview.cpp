#include "conferenceview.h"

#include "videowidget.h"
#include "ui_callingwidget.h"

#include <QLabel>
#include <QGridLayout>
#include <QDebug>

uint16_t ROWMAXLENGTH = 3;

ConferenceView::ConferenceView(QWidget *parent):
  parent_(parent),
  layout_(NULL),
  layoutWidget_(NULL),
  callingWidget_(NULL),
  row_(0),
  column_(0),
  activeCalls_(),
  deniedCalls_()
{}

void ConferenceView::init(QGridLayout* conferenceLayout, QWidget* layoutwidget,
                          Ui::CallerWidget* callingwidget, QWidget* holdingWidget)
{
  layout_ = conferenceLayout;
  layoutWidget_ = layoutwidget;
  callingWidget_ = callingwidget;
  holdingWidget_ = holdingWidget;
}

void ConferenceView::callingTo(QString callID, QString name)
{
  if(activeCalls_.find(callID) != activeCalls_.end())
  {
    qWarning() << "WARNING: Outgoing call already has an allocated view";
    return;
  }

  QLabel* label = new QLabel(parent_);
  label->setText("Calling...");

  QFont font = QFont("Times", 16);
  label->setFont(font);
  label->setAlignment(Qt::AlignHCenter);
  layout_->addWidget(label, row_, column_);

  activeCalls_[callID] = new CallInfo{WAITINGPEER, name, layout_->itemAtPosition(row_,column_),
                         row_, column_};

  nextSlot();
}

void ConferenceView::incomingCall(QString callID, QString name)
{
  Q_ASSERT(callingWidget_);
  if(callingWidget_ == NULL)
  {
    qWarning() << "WARNING: Calling widget is missing!!";
  }
  if(activeCalls_.find(callID) != activeCalls_.end())
  {
    qWarning() << "WARNING: Incoming call already has an allocated view";
    return;
  }

  // TODO display a incoming call widget instead of layout/stream?
  //QLabel* label = new QLabel(parent_);
  QLabel* label = new QLabel(NULL);
  label->setText("Incoming call from " + name);

  QFont font = QFont("Times", 16);
  label->setFont(font);
  label->setAlignment(Qt::AlignHCenter);
  layout_->addWidget(label, row_, column_);

  activeCalls_[callID] = new CallInfo{ASKINGUSER, name, layout_->itemAtPosition(row_,column_),
                          row_, column_};

  qDebug() << "Displaying pop-up for somebody calling";
  callingWidget_->CallerLabel->setText(name + " is calling..");
  holdingWidget_->show();

  askingQueue_.append(callID);
  nextSlot();
}

// if our call is accepted or we accepted their call
VideoWidget* ConferenceView::addVideoStream(QString callID)
{
  VideoWidget* view = new VideoWidget();
  if(activeCalls_.find(callID) == activeCalls_.end())
  {
    qWarning() << "WARNING: Adding a videostream without previous.";
    return NULL;
  }
  else if(activeCalls_[callID]->state != ASKINGUSER
          && activeCalls_[callID]->state != WAITINGPEER)
  {
    qWarning() << "WARNING: activating stream without previous state set";
    return NULL;
  }

  // add the widget in place of previous one
  activeCalls_[callID]->state = ACTIVE;

  // TODO delete previous widget now instead of with parent.
  // Now they accumulate in memory until call has ended
  delete activeCalls_[callID]->item->widget();
  layout_->removeItem(activeCalls_[callID]->item);
  layout_->addWidget(view, activeCalls_[callID]->row, activeCalls_[callID]->column);
  activeCalls_[callID]->item = layout_->itemAtPosition(activeCalls_[callID]->row,
                                                       activeCalls_[callID]->column);
  return view;
}

void ringing(QString callID)
{
  // get widget from layout and change the text.
}

void ConferenceView::removeCaller(QString callID)
{
  if(activeCalls_.find(callID) == activeCalls_.end() || activeCalls_[callID]->item == NULL )
  {
    qWarning() << "WARNING: Trying to remove nonexisting call from ConferenceView";
    return;
  }

  delete activeCalls_[callID]->item->widget();
  layout_->removeItem(activeCalls_[callID]->item);

  activeCalls_.erase(callID);
}

void ConferenceView::nextSlot()
{
  // TODO improve this algorithm for more optimized layout
  ++column_;
  if(column_ == ROWMAXLENGTH)
  {
    column_ = 0;
    ++row_;
  }
}

QString ConferenceView::acceptNewest()
{
  QString last = askingQueue_.last();
  askingQueue_.pop_back();
  if(askingQueue_.size() == 0)
  {
    holdingWidget_->hide();
  }

  return last;
}

QString ConferenceView::rejectNewest()
{
  QString last = askingQueue_.last();
  askingQueue_.pop_back();
  if(askingQueue_.size() == 0)
  {
    holdingWidget_->hide();
  }

  removeCaller(last);
  return last;
}

void ConferenceView::close()
{
  while(!activeCalls_.empty())
  {
    removeCaller((*activeCalls_.begin()).first);
  }

  askingQueue_.clear();

  row_ = 0;
  column_ = 0;
}
