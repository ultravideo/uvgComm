#include "conferenceview.h"

#include "ui_incomingcallwidget.h"
#include "ui_outgoingcallwidget.h"
#include "ui_popupholder.h"

#include "gui/videoviewfactory.h"
#include "gui/videowidget.h"

#include <QLabel>
#include <QGridLayout>
#include <QDebug>

ConferenceView::ConferenceView(QWidget *parent):
  parent_(parent),
  layout_(nullptr),
  layoutWidget_(nullptr),
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

void ConferenceView::callingTo(uint32_t sessionID, QString name)
{
  Q_ASSERT(sessionID);
  if(activeCalls_.find(sessionID) != activeCalls_.end())
  {
    qWarning() << "WARNING: Outgoing call already has an allocated view";
    return;
  }

  attachOutgoingCallWidget(name, sessionID);
}

void ConferenceView::addWidgetToLayout(ViewState state, QWidget* widget, QString name, uint32_t sessionID)
{
  locMutex_.lock();

  // TODO: more checks if something goes wrong. There is some kind of bug when adding the caller

  uint16_t row = row_;
  uint16_t column = column_;

  if(!freedLocs_.empty())
  {
    row = freedLocs_.front().row;
    column = freedLocs_.front().column;
  }

  layoutMutex_.lock();

  if(widget != nullptr)
  {
    layout_->addWidget(widget, row, column);
  }

  activeCalls_[sessionID] = std::unique_ptr<CallInfo>
      (new CallInfo{state, name, layout_->itemAtPosition(row,column), row, column});

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

void ConferenceView::incomingCall(uint32_t sessionID, QString name)
{
  if(activeCalls_.find(sessionID) != activeCalls_.end())
  {
    qWarning() << "WARNING: Incoming call already has an allocated view. Session:" << sessionID;
    return;
  }
  qDebug() << "Displaying pop-up for somebody calling in slot:" << row_ << "," << column_;

  attachIncomingCallWidget(name, sessionID);
}

void ConferenceView::attachIncomingCallWidget(QString name, uint32_t sessionID)
{
  QFrame* frame = new QFrame;
  Ui::IncomingCall *calling = new Ui::IncomingCall;

  frame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  frame->setLineWidth(2);
  frame->setMidLineWidth(1);

  calling->setupUi(frame);

  connect(calling->acceptButton, SIGNAL(clicked()), this, SLOT(accept()));
  connect(calling->declineButton, SIGNAL(clicked()), this, SLOT(reject()));

  calling->CallerLabel->setText(name + " is calling..");
  addWidgetToLayout(VIEWASKING, frame, name, sessionID);

  calling->acceptButton->setProperty("sessionID", QVariant(sessionID));
  calling->declineButton->setProperty("sessionID", QVariant(sessionID));

  frame->show();
}

void ConferenceView::attachOutgoingCallWidget(QString name, uint32_t sessionID)
{
  QFrame* holder = new QFrame;
  Ui::OutgoingCall *widget = new Ui::OutgoingCall;
  widget->setupUi(holder);
  widget->CallingLabel->setText("Calling " + name);

  addWidgetToLayout(VIEWWAITINGPEER, holder, name, sessionID);

  widget->cancelCall->setProperty("sessionID", QVariant(sessionID));
  connect(widget->cancelCall, SIGNAL(clicked()), this, SLOT(cancel()));

  holder->show();
}


void ConferenceView::attachWidget(uint32_t sessionID, QWidget* view)
{
  layoutMutex_.lock();
  if(activeCalls_[sessionID]->item)
  {
    layout_->removeItem(activeCalls_[sessionID]->item);
  }
  layout_->addWidget(view, activeCalls_[sessionID]->row, activeCalls_[sessionID]->column);
  activeCalls_[sessionID]->item = layout_->itemAtPosition(activeCalls_[sessionID]->row,
                                                       activeCalls_[sessionID]->column);
  layoutMutex_.unlock();
}

// if our call is accepted or we accepted their call
void ConferenceView::addVideoStream(uint32_t sessionID, std::shared_ptr<VideoviewFactory> factory)
{
  factory->createWidget(sessionID, nullptr, this);
  QWidget* view = factory->getView(sessionID);

  if(activeCalls_.find(sessionID) == activeCalls_.end())
  {
    qDebug() << "Did not find asking widget. Assuming auto-accept and adding widget";
    addWidgetToLayout(VIEWVIDEO, nullptr, "Auto-Accept", sessionID);
  }
  else if(activeCalls_[sessionID]->state != VIEWASKING
          && activeCalls_[sessionID]->state != VIEWWAITINGPEER)
  {
    qWarning() << "WARNING: activating stream with wrong state";
    return;
  }

  // add the widget in place of previous one
  activeCalls_[sessionID]->state = VIEWVIDEO;

  // TODO delete previous widget now instead of with parent.
  // Now they accumulate in memory until call has ended
  if(activeCalls_[sessionID]->item != nullptr
     && activeCalls_[sessionID]->item->widget() != nullptr)
  {
    delete activeCalls_[sessionID]->item->widget();
  }

  attachWidget(sessionID, view);

  //view->setParent(0);
  //view->showMaximized();
  //view->show();
  //view->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);

  view->show();
}

void ConferenceView::ringing(uint32_t sessionID)
{
  // get widget from layout and change the text.
  qDebug() << sessionID << "call is ringing. TODO: display it to user";
}

bool ConferenceView::removeCaller(uint32_t sessionID)
{
  if(activeCalls_.find(sessionID) == activeCalls_.end())
  {
    Q_ASSERT(false);
    qWarning() << "ERROR: Tried to remove the view of non-existing sessionID!";
  }
  else
  {
    uninitCaller(std::move(activeCalls_[sessionID]));
    activeCalls_.erase(sessionID);
  }

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
  for (std::map<uint32_t, std::unique_ptr<CallInfo>>::iterator view = activeCalls_.begin();
       view != activeCalls_.end(); ++view)
  {
    uninitCaller(std::move(view->second));
  }

  activeCalls_.clear();
  freedLocs_.clear();

  locMutex_.lock();
  row_ = 0;
  column_ = 0;
  rowMaxLength_ = 2;

  locMutex_.unlock();
}

void ConferenceView::uninitCaller(std::unique_ptr<CallInfo> peer)
{
  if(peer->item != nullptr)
  {
    layoutMutex_.lock();
    layout_->removeItem(peer->item);
    layoutMutex_.unlock();
  }

  if(peer->state == VIEWVIDEO
     || peer->state == VIEWASKING
     || peer->state == VIEWWAITINGPEER)
  {
    peer->item->widget()->hide();
    delete peer->item->widget();
  }

  locMutex_.lock();

  // record the freed locations so we can later insert new views to them if need be
  // TODO: Reorder the layout everytime a view is removed.
  if(activeCalls_.size() != 1)
  {
    freedLocs_.push_back({peer->row, peer->column});
  }
  else
  {
    freedLocs_.clear();
    row_ = 0;
    column_ = 0;
    rowMaxLength_ = 2;
    qDebug() << "Removing last video view. Clearing previous data";
  }
  locMutex_.unlock();
}


void ConferenceView::accept()
{
  uint32_t sessionID = sender()->property("sessionID").toUInt();
  if (sessionID != 0)
  {
    emit acceptCall(sessionID);
  }
  else
  {
    qCritical() << "ERROR: Couldn't find the invoker for accept";
  }
}

void ConferenceView::reject()
{
  uint32_t sessionID = sender()->property("sessionID").toUInt();
  emit rejectCall(sessionID);
}

void ConferenceView::cancel()
{
  uint32_t sessionID = sender()->property("sessionID").toUInt();
  emit cancelCall(sessionID);
}
