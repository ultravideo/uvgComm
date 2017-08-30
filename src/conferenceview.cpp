#include "conferenceview.h"

#include "videowidget.h"
#include "ui_callingwidget.h"

#include <QLabel>
#include <QGridLayout>
#include <QDebug>

ConferenceView::ConferenceView(QWidget *parent):
  parent_(parent),
  layout_(NULL),
  layoutWidget_(NULL),
  callingWidget_(NULL),
  row_(0),
  column_(0),
  rowMaxLength_(2),
  activeCalls_()//,
  //deniedCalls_()
{}

void ConferenceView::init(QGridLayout* conferenceLayout, QWidget* layoutwidget)
{
  // TODO: move to conferenceview somehow
  Ui::CallerWidget *ui_widget = new Ui::CallerWidget;
  QWidget* holderWidget = new QWidget;
  ui_widget->setupUi(holderWidget);
  connect(ui_widget->AcceptButton, SIGNAL(clicked()), this, SLOT(accept()));
  connect(ui_widget->DeclineButton, SIGNAL(clicked()), this, SLOT(reject()));

  layoutMutex_.lock();
  layout_ = conferenceLayout;
  layoutMutex_.unlock();
  layoutWidget_ = layoutwidget;
  callingWidget_ = ui_widget;
  holdingWidget_ = holderWidget;
}

void ConferenceView::callingTo(QString callID, QString name)
{
  if(activeCalls_.find(callID) != activeCalls_.end())
  {
    qWarning() << "WARNING: Outgoing call already has an allocated view";
    return;
  }

  QLabel* label = new QLabel(parent_);
  label->setText("Calling " + name);

  QFont font = QFont("Times", 16); // TODO: change font
  label->setFont(font);
  label->setAlignment(Qt::AlignHCenter);
  layoutMutex_.lock();
  locMutex_.lock();
  layout_->addWidget(label, row_, column_);

  activeCalls_[callID] = new CallInfo{WAITINGPEER, name, layout_->itemAtPosition(row_,column_),
                         row_, column_};
  locMutex_.unlock();
  layoutMutex_.unlock();
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
  qDebug() << "Displaying pop-up for somebody calling";
  callingWidget_->CallerLabel->setText(name + " is calling..");
  holdingWidget_->show();

  layoutMutex_.lock();
  locMutex_.lock();
  layout_->addWidget(holdingWidget_, row_, column_);

  activeCalls_[callID] = new CallInfo{ASKINGUSER, name, layout_->itemAtPosition(row_,column_),
                          row_, column_};
  locMutex_.unlock();
  layoutMutex_.unlock();
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
  layoutMutex_.lock();
  layout_->removeItem(activeCalls_[callID]->item);
  layout_->addWidget(view, activeCalls_[callID]->row, activeCalls_[callID]->column);
  activeCalls_[callID]->item = layout_->itemAtPosition(activeCalls_[callID]->row,
                                                       activeCalls_[callID]->column);
  layoutMutex_.unlock();
  //view->setParent(0);
  //view->showMaximized();
  //view->show();
  //view->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);

  return view;
}

void ConferenceView::ringing(QString callID)
{
  // get widget from layout and change the text.
  qDebug() << callID << "call is ringing. TODO: display it to user";
}

void ConferenceView::removeCaller(QString callID)
{
  if(activeCalls_.find(callID) == activeCalls_.end() || activeCalls_[callID]->item == NULL )
  {
    qWarning() << "WARNING: Trying to remove nonexisting call from ConferenceView";
    return;
  }
  if(activeCalls_[callID]->state == ACTIVE)
  {
    delete activeCalls_[callID]->item->widget();
  }
  layoutMutex_.lock();
  layout_->removeItem(activeCalls_[callID]->item);
  layoutMutex_.unlock();

  activeCalls_.erase(callID);
}

void ConferenceView::nextSlot()
{
  locMutex_.lock();
  ++column_;
  if(column_ == rowMaxLength_)
  {
    // move to first row, third place
    if(row_ == 1 && column_ == 2)
    {
      rowMaxLength_ = 3;
      row_ = 0;
      column_ = 2;
    }
    // move to second row, third place
    else if(row_ == 0 && column_ == 3)
    {
      row_ = 1;
      column_ = 2;
    }
    else if(row_ == 2 && column_ == 3)
    {
      ++row_;
      column_ = 1;
    }
    else // move forward
    {
      column_ = 0;
      ++row_;
    }
  }
  locMutex_.unlock();
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
    layoutMutex_.lock();
    layout_->removeItem((*activeCalls_.begin()).second->item);
    layoutMutex_.unlock();
    removeCaller((*activeCalls_.begin()).first);
  }

  askingQueue_.clear();
  locMutex_.lock();
  row_ = 0;
  column_ = 0;
  rowMaxLength_ = 2;
  locMutex_.unlock();
}

void ConferenceView::accept()
{
  QString callID = acceptNewest();
  qDebug() << callID << ": We accepted";
  emit acceptCall(callID);
}

void ConferenceView::reject()
{
  QString callID = rejectNewest();
  qDebug() << callID << ": We rejected";
  emit rejectCall(callID);
}
