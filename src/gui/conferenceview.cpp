#include "conferenceview.h"

#include "ui_incomingcallwidget.h"
#include "ui_outgoingcallwidget.h"
//#include "ui_popupholder.h"

#include "gui/videoviewfactory.h"
#include "gui/videowidget.h"

#include <QLabel>
#include <QGridLayout>
#include <QDebug>
#include <QDir>

ConferenceView::ConferenceView(QWidget *parent):
  parent_(parent),
  layout_(nullptr),
  layoutWidget_(nullptr),
  activeCalls_(),
  detachedWidgets_(),
  freedLocs_(),
  row_(0),
  column_(0),
  rowMaxLength_(2)
{}

void ConferenceView::init(QGridLayout* conferenceLayout, QWidget* layoutwidget)
{
  layoutMutex_.lock();
  layout_ = conferenceLayout;
  layoutMutex_.unlock();
  layoutWidget_ = layoutwidget;

  connect(&timeoutTimer_, &QTimer::timeout, this, &ConferenceView::updateTimes);
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
  activeCallMutex_.lock();
  activeCalls_[sessionID] = std::unique_ptr<CallInfo>
      (new CallInfo{state, name, layout_->itemAtPosition(row,column),
                    row, column, nullptr, nullptr});
  activeCallMutex_.unlock();

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
    qWarning() << "WARNING," << metaObject()->className()
               << "Incoming call already has an allocated view. Session:" << sessionID;
    return;
  }
  qDebug() << "Incoming call," << metaObject()->className()
           << "Displaying pop-up for somebody calling in slot:" << row_ << "," << column_;

  attachIncomingCallWidget(name, sessionID);
}

void ConferenceView::attachIncomingCallWidget(QString name, uint32_t sessionID)
{
  QFrame* frame = new QFrame;
  Ui::IncomingCall *in = new Ui::IncomingCall;

  frame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
  frame->setLineWidth(2);
  frame->setMidLineWidth(1);

  in->setupUi(frame);

  connect(in->acceptButton, SIGNAL(clicked()), this, SLOT(accept()));
  connect(in->declineButton, SIGNAL(clicked()), this, SLOT(reject()));

  in->NameLabel->setText(name);
  in->StatusLabel->setText("is calling ...");

  addWidgetToLayout(VIEWASKING, frame, name, sessionID);
  activeCalls_[sessionID]->in = in;

  in->acceptButton->setProperty("sessionID", QVariant(sessionID));
  in->declineButton->setProperty("sessionID", QVariant(sessionID));

  QPixmap pixmap(QDir::currentPath() + "/icons/end_call.svg");
  QIcon ButtonIcon(pixmap);
  in->declineButton->setIcon(ButtonIcon);
  in->declineButton->setText("");
  in->declineButton->setIconSize(QSize(35,35));

  QPixmap pixmap2(QDir::currentPath() + "/icons/call.svg");
  QIcon ButtonIcon2(pixmap2);
  in->acceptButton->setIcon(ButtonIcon2);
  in->acceptButton->setText("");
  in->acceptButton->setIconSize(QSize(35,35));

  frame->show();
}

void ConferenceView::attachOutgoingCallWidget(QString name, uint32_t sessionID)
{
  QFrame* holder = new QFrame;
  Ui::OutgoingCall *out = new Ui::OutgoingCall;
  out->setupUi(holder);
  out->NameLabel->setText(name);
  out->StatusLabel->setText("Connecting ...");

  if(!timeoutTimer_.isActive())
  {
    timeoutTimer_.start(1000);
  }

  addWidgetToLayout(VIEWWAITINGPEER, holder, name, sessionID);

  activeCalls_[sessionID]->out = out;
  out->cancelCall->setProperty("sessionID", QVariant(sessionID));
  connect(out->cancelCall, SIGNAL(clicked()), this, SLOT(cancel()));

  QPixmap pixmap(QDir::currentPath() + "/icons/end_call.svg");
  QIcon ButtonIcon(pixmap);
  out->cancelCall->setIcon(ButtonIcon);
  out->cancelCall->setText("");
  out->cancelCall->setIconSize(QSize(35,35));

  holder->show();
}

void ConferenceView::attachWidget(uint32_t sessionID, QWidget* view)
{
  layoutMutex_.lock();
  if(activeCalls_.find(sessionID) != activeCalls_.end())
  {
    if(activeCalls_[sessionID]->item)
    {
      layout_->removeItem(activeCalls_[sessionID]->item);
    }
    layout_->addWidget(view, activeCalls_[sessionID]->row, activeCalls_[sessionID]->column);
    activeCalls_[sessionID]->item = layout_->itemAtPosition(activeCalls_[sessionID]->row,
                                                            activeCalls_[sessionID]->column);
    detachedWidgets_.erase(sessionID);

    if(detachedWidgets_.empty())
    {
      parent_->show();
    }
  }
  else {
    qWarning() << "ERROR," << metaObject()->className() << ": Trying to attach fullscreenview back to layout when the sessionID"
             << sessionID << " does not exist in conference view.";
  }
  layoutMutex_.unlock();
}

void ConferenceView::detachWidget(uint32_t sessionID, QWidget* view)
{
  layoutMutex_.lock();
  if(activeCalls_.find(sessionID) != activeCalls_.end())
  {
    if(activeCalls_[sessionID]->item)
    {
      layout_->removeItem(activeCalls_[sessionID]->item);
    }
    layout_->removeWidget(view);
    activeCalls_[sessionID]->item = nullptr;
    detachedWidgets_[sessionID] = view;

    if(detachedWidgets_.size() == 1)
    {
      parent_->hide();
    }
  }
  else
  {
    qWarning() << "ERROR," << metaObject()->className()
               << ": Trying to detach fullscreenview from the layout when the sessionID"
               << sessionID << " does not exist in  conference view.";
  }

  layoutMutex_.unlock();
}

// if our call is accepted or we accepted their call
void ConferenceView::addVideoStream(uint32_t sessionID, std::shared_ptr<VideoviewFactory> factory)
{
  factory->createWidget(sessionID, nullptr, this);
  QWidget* view = factory->getView(sessionID);

  if(activeCalls_.find(sessionID) == activeCalls_.end())
  {
    qDebug() << "Session construction," << metaObject()->className()
             << ": Did not find asking widget. Assuming auto-accept and adding widget";
    addWidgetToLayout(VIEWVIDEO, nullptr, "Auto-Accept", sessionID);
  }
  else if(activeCalls_[sessionID]->state != VIEWASKING
          && activeCalls_[sessionID]->state != VIEWWAITINGPEER)
  {
    qWarning() << "WARNING: activating stream with wrong state";
    return;
  }

  activeCallMutex_.lock();
  // add the widget in place of previous one
  activeCalls_[sessionID]->state = VIEWVIDEO;
  activeCalls_[sessionID]->in = nullptr;
  activeCalls_[sessionID]->out = nullptr;

  // TODO delete previous widget now instead of with parent.
  // Now they accumulate in memory until call has ended
  if(activeCalls_[sessionID]->item != nullptr
     && activeCalls_[sessionID]->item->widget() != nullptr)
  {
    delete activeCalls_[sessionID]->item->widget();
  }
  activeCallMutex_.unlock();


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
  qDebug() << "Ringing," << metaObject()->className() << ": Call is ringing. SessionID" << sessionID;

  if(activeCalls_.find(sessionID) == activeCalls_.end())
  {
    qWarning() << "ERROR: Ringing for nonexisting view. View should always exist if this function is called.";
    return;
  }
  if (activeCalls_[sessionID]->out != nullptr)
  {
    activeCallMutex_.lock();
    activeCalls_[sessionID]->out->StatusLabel->setText("Call is ringing ...");
    activeCallMutex_.unlock();
  }
  else
  {
    qWarning() << "ERROR: No incoming call widget exists when it should be ringing:" << sessionID;
  }
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
    activeCallMutex_.lock();
    uninitCaller(std::move(activeCalls_[sessionID]));
    activeCalls_.erase(sessionID);

    uninitDetachedWidget(sessionID);

    activeCallMutex_.unlock();
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
  qDebug() << "End all calls," << metaObject()->className() << ": Closing views of the call with"
           << detachedWidgets_.size() << "detached widgets";
  for (std::map<uint32_t, std::unique_ptr<CallInfo>>::iterator view = activeCalls_.begin();
       view != activeCalls_.end(); ++view)
  {
    uninitCaller(std::move(view->second));
  }

  for (std::map<uint32_t, QWidget*>::iterator view = detachedWidgets_.begin();
       view != detachedWidgets_.end(); ++view)
  {
    uninitDetachedWidget(view->first);
  }

  detachedWidgets_.clear();
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
    if(peer->state == VIEWVIDEO
       || peer->state == VIEWASKING
       || peer->state == VIEWWAITINGPEER)
    {
      if(peer->item->widget() != nullptr)
      {
        peer->item->widget()->hide();
        delete peer->item->widget();
      }
    }

    layoutMutex_.lock();
    layout_->removeItem(peer->item);
    layoutMutex_.unlock();
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
    qDebug() << "Closing," << metaObject()->className() << ": Removing last video view. Clearing previous data";
  }
  locMutex_.unlock();
}

void ConferenceView::uninitDetachedWidget(uint32_t sessionID)
{
  if(detachedWidgets_.find(sessionID) != detachedWidgets_.end())
  {
    qDebug() << "Closing," << metaObject()->className() << ": The widget was detached for sessionID" << sessionID << "so we destroy it.";
    delete detachedWidgets_[sessionID];
    detachedWidgets_.erase(sessionID);
  }
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
    qCritical() << "ERROR," << metaObject()->className() << "Couldn't find the invoker for accept";
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

void ConferenceView::updateTimes()
{
  bool countersRunning = false;
  activeCallMutex_.lock();
  for(std::map<uint32_t, std::unique_ptr<CallInfo>>::iterator i = activeCalls_.begin();
      i != activeCalls_.end(); ++i)
  {
    if(i->second->out != nullptr)
    {
      i->second->out->timeout->display(i->second->out->timeout->intValue() - 1);
      countersRunning = true;
    }
  }
  activeCallMutex_.unlock();

  if(!countersRunning)
  {
    timeoutTimer_.stop();
  }
}
