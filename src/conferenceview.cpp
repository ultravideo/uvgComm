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
  row_(0),
  column_(0),
  rowMaxLength_(2),
  activeCalls_()
{}

void ConferenceView::init(QGridLayout* conferenceLayout, QWidget* layoutwidget)
{
  layoutMutex_.lock();
  layout_ = conferenceLayout;
  layoutMutex_.unlock();
  layoutWidget_ = layoutwidget;
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

  addWidgetToLayout(WAITINGPEER, label, name, callID);
}

void ConferenceView::addWidgetToLayout(CallState state, QWidget* widget, QString name, QString callID)
{
  locMutex_.lock();
  int row = row_;
  int column = column_;

  if(!freedLocs_.empty())
  {
    row = freedLocs_.front().row;
    column = freedLocs_.front().column;
  }

  layoutMutex_.lock();
  layout_->addWidget(widget, row, column);

  activeCalls_[callID] = new CallInfo{state, name, layout_->itemAtPosition(row,column),
                         row, column};
  layoutMutex_.unlock();

  if(!freedLocs_.empty()){
    freedLocs_.pop_front();
  }
  else
  {
    nextSlot();
  }
  locMutex_.unlock();
}

void ConferenceView::incomingCall(QString callID, QString name)
{
  if(activeCalls_.find(callID) != activeCalls_.end())
  {
    qWarning() << "WARNING: Incoming call already has an allocated view";
    return;
  }
  qDebug() << "Displaying pop-up for somebody calling in slot:" << row_ << "," << column_;
  QWidget* holder = new QWidget;
  attachCallingWidget(holder, name + " is calling..");
  addWidgetToLayout(ASKINGUSER, holder, name, callID);
}

void ConferenceView::attachCallingWidget(QWidget* holder, QString text)
{

  Ui::CallerWidget *calling = new Ui::CallerWidget;

  calling->setupUi(holder);
  connect(calling->AcceptButton, SIGNAL(clicked()), this, SLOT(accept()));
  connect(calling->DeclineButton, SIGNAL(clicked()), this, SLOT(reject()));

  calling->CallerLabel->setText(text);
  holder->show();
}

// if our call is accepted or we accepted their call
VideoWidget* ConferenceView::addVideoStream(QString callID)
{
  VideoWidget* view = new VideoWidget(NULL,1);
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

bool ConferenceView::removeCaller(QString callID)
{
  if(activeCalls_.find(callID) == activeCalls_.end() || activeCalls_[callID]->item == NULL )
  {
    qWarning() << "WARNING: Trying to remove nonexisting call from ConferenceView";
    return !activeCalls_.empty();
  }
  if(activeCalls_[callID]->state == ACTIVE || activeCalls_[callID]->state == ASKINGUSER)
  {
    activeCalls_[callID]->item->widget()->hide();
    delete activeCalls_[callID]->item->widget();
  }
  layoutMutex_.lock();
  layout_->removeItem(activeCalls_[callID]->item);
  layoutMutex_.unlock();

  locMutex_.lock();
  if(activeCalls_.size() != 1)
  {
    freedLocs_.push_back({activeCalls_[callID]->row, activeCalls_[callID]->column});
  }
  else
  {
    freedLocs_.clear();
    row_ = 0;
    column_ = 0;
    rowMaxLength_ = 2;
    qDebug() << "Removed last video view. Clearing previous data";
  }
  locMutex_.unlock();
  activeCalls_.erase(callID);

  return !activeCalls_.empty();
}

void ConferenceView::nextSlot()
{
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

  locMutex_.lock();
  row_ = 0;
  column_ = 0;
  rowMaxLength_ = 2;

  locMutex_.unlock();
}

QString ConferenceView::findInvoker(QString buttonName)
{
  QString callID = "";
  QPushButton* button = qobject_cast<QPushButton*>(sender());
  for(auto callinfo : activeCalls_)
  {
    QPushButton* pb = callinfo.second->item->widget()->findChildren<QPushButton *>(buttonName).first();
    if(pb == button)
    {
      callID = callinfo.first;
      qDebug() << "Found invoking" << buttonName << "CallID:" << callID;
    }
  }

  return callID;
}

void ConferenceView::accept()
{
  QString callID = findInvoker("AcceptButton");
  if(callID == "")
  {
    qWarning() << "WARNING: Could not find invoking push_button";
    return;
  }
  Q_ASSERT(activeCalls_[callID]->state != ASKINGUSER);
  emit acceptCall(callID);
}

void ConferenceView::reject()
{
  QString callID = findInvoker("DeclineButton");
  if(callID == "")
  {
    qWarning() << "WARNING: Could not find invoking reject button";
    return;
  }
  Q_ASSERT(activeCalls_[callID]->state != ASKINGUSER);
  removeCaller(callID);
  emit rejectCall(callID);
}
