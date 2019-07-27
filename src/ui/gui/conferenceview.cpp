#include "conferenceview.h"

#include "ui_incomingcallwidget.h"
#include "ui_outgoingcallwidget.h"
//#include "ui_popupholder.h"

#include "ui/gui/videoviewfactory.h"
#include "ui/gui/videowidget.h"

#include "common.h"

#include <QDebug>
#include <QLabel>
#include <QGridLayout>
#include <QDir>

ConferenceView::ConferenceView(QWidget *parent):
  parent_(parent),
  layout_(nullptr),
  layoutWidget_(nullptr),
  activeViews_(),
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
  if(activeViews_.find(sessionID) != activeViews_.end())
  {
    printDebug(DEBUG_WARNING, this, DC_START_CALL, "Outgoing call already has an allocated view.");
    return;
  }

  attachOutgoingCallWidget(name, sessionID);
}

void ConferenceView::addWidgetToLayout(SessionViewState state, QWidget* widget, QString name, uint32_t sessionID)
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
  viewMutex_.lock();
  activeViews_[sessionID] = std::unique_ptr<ViewInfo>
      (new ViewInfo{state, name, layout_->itemAtPosition(row,column),
                    row, column, nullptr, nullptr});
  viewMutex_.unlock();

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
  if(activeViews_.find(sessionID) != activeViews_.end())
  {
    printDebug(DEBUG_WARNING, this, DC_START_CALL, "Incoming call already has an allocated view.",
      {"SessionID"}, {QString::number(sessionID)});
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

  addWidgetToLayout(VIEW_ASKING, frame, name, sessionID);
  activeViews_[sessionID]->in = in;

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

  addWidgetToLayout(VIEW_WAITING_PEER, holder, name, sessionID);

  activeViews_[sessionID]->out = out;
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
  if(activeViews_.find(sessionID) != activeViews_.end())
  {
    if(activeViews_[sessionID]->item)
    {
      layout_->removeItem(activeViews_[sessionID]->item);
    }
    layout_->addWidget(view, activeViews_[sessionID]->row, activeViews_[sessionID]->column);
    activeViews_[sessionID]->item = layout_->itemAtPosition(activeViews_[sessionID]->row,
                                                            activeViews_[sessionID]->column);
    detachedWidgets_.erase(sessionID);

    if(detachedWidgets_.empty())
    {
      parent_->show();
    }
  }
  else {

    printDebug(DEBUG_PROGRAM_ERROR, this, DC_FULLSCREEN,
               "Trying to attach fullscreenview back to layout when sessionID does not exist in conference view.",
               {"SessionID"}, {QString::number(sessionID)});
  }
  layoutMutex_.unlock();
}

void ConferenceView::detachWidget(uint32_t sessionID, QWidget* view)
{
  layoutMutex_.lock();
  if(activeViews_.find(sessionID) != activeViews_.end())
  {
    if(activeViews_[sessionID]->item)
    {
      layout_->removeItem(activeViews_[sessionID]->item);
    }
    layout_->removeWidget(view);
    activeViews_[sessionID]->item = nullptr;
    detachedWidgets_[sessionID] = view;

    if(detachedWidgets_.size() == 1)
    {
      parent_->hide();
    }
  }
  else
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_FULLSCREEN,
               "Trying to detach fullscreenview from the layout when the sessionID does not exist in conference view.",
               {"SessionID"}, {QString::number(sessionID)});
  }

  layoutMutex_.unlock();
}

// if our call is accepted or we accepted their call
void ConferenceView::addVideoStream(uint32_t sessionID, std::shared_ptr<VideoviewFactory> factory)
{
  uint32_t id = factory->createWidget(sessionID, nullptr, this);
  QWidget* view = factory->getView(sessionID, id);

  if(activeViews_.find(sessionID) == activeViews_.end())
  {
    qDebug() << "Session construction," << metaObject()->className()
             << ": Did not find asking widget. Assuming auto-accept and adding widget";
    addWidgetToLayout(VIEW_VIDEO, nullptr, "Auto-Accept", sessionID);
  }
  else if(activeViews_[sessionID]->state != VIEW_ASKING
          && activeViews_[sessionID]->state != VIEW_WAITING_PEER)
  {
    printDebug(DEBUG_WARNING, this, DC_ADD_MEDIA,
                     "Activating stream with wrong state.",
                    {"SessionID", "State"},
                    {QString::number(sessionID), QString::number(activeViews_[sessionID]->state)});
    return;
  }

  viewMutex_.lock();
  // add the widget in place of previous one
  activeViews_[sessionID]->state = VIEW_VIDEO;
  activeViews_[sessionID]->in = nullptr;
  activeViews_[sessionID]->out = nullptr;

  // TODO delete previous widget now instead of with parent.
  // Now they accumulate in memory until call has ended
  if(activeViews_[sessionID]->item != nullptr
     && activeViews_[sessionID]->item->widget() != nullptr)
  {
    delete activeViews_[sessionID]->item->widget();
  }
  viewMutex_.unlock();


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

  if(activeViews_.find(sessionID) == activeViews_.end())
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_RINGING,
                     "Ringing for nonexisting view. View should always exist if this function is called.",
                    {"SessionID"}, {QString::number(sessionID)});
    return;
  }
  if (activeViews_[sessionID]->out != nullptr)
  {
    viewMutex_.lock();
    activeViews_[sessionID]->out->StatusLabel->setText("Call is ringing ...");
    viewMutex_.unlock();
  }
  else
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_RINGING, "No incoming call widget exists when it should be ringing.",
      {"SessionID"},{QString::number(sessionID)});
  }
}

bool ConferenceView::removeCaller(uint32_t sessionID)
{
  if(activeViews_.find(sessionID) == activeViews_.end())
  {
    Q_ASSERT(false);
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_END_CALL,
                     "Tried to remove the view of non-existing sessionID!");
  }
  else
  {
    viewMutex_.lock();
    uninitCaller(std::move(activeViews_[sessionID]));
    activeViews_.erase(sessionID);

    uninitDetachedWidget(sessionID);

    viewMutex_.unlock();
  }

  return !activeViews_.empty();
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
  for (std::map<uint32_t, std::unique_ptr<ViewInfo>>::iterator view = activeViews_.begin();
       view != activeViews_.end(); ++view)
  {
    uninitCaller(std::move(view->second));
  }

  for (std::map<uint32_t, QWidget*>::iterator view = detachedWidgets_.begin();
       view != detachedWidgets_.end(); ++view)
  {
    uninitDetachedWidget(view->first);
  }

  detachedWidgets_.clear();
  activeViews_.clear();
  freedLocs_.clear();

  locMutex_.lock();
  row_ = 0;
  column_ = 0;
  rowMaxLength_ = 2;

  locMutex_.unlock();
}

void ConferenceView::uninitCaller(std::unique_ptr<ViewInfo> peer)
{
  if(peer->item != nullptr)
  {
    if(peer->state == VIEW_VIDEO
       || peer->state == VIEW_ASKING
       || peer->state == VIEW_WAITING_PEER)
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
  if(activeViews_.size() != 1)
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
  if (sessionID != 0
      && activeViews_.find(sessionID) != activeViews_.end()
      && activeViews_[sessionID]->state == VIEW_ASKING
      && activeViews_[sessionID]->in != nullptr)
  {
    activeViews_[sessionID]->in->acceptButton->hide();
    activeViews_[sessionID]->in->declineButton->hide();
    emit acceptCall(sessionID);
  }
  else
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_ACCEPT,
               "Couldn't find the invoker for accept.");
  }
}

void ConferenceView::reject()
{
  uint32_t sessionID = sender()->property("sessionID").toUInt();
  if (sessionID != 0
      && activeViews_.find(sessionID) != activeViews_.end()
      && activeViews_[sessionID]->state == VIEW_ASKING
      && activeViews_[sessionID]->in != nullptr)
  {
    activeViews_[sessionID]->in->acceptButton->hide();
    activeViews_[sessionID]->in->declineButton->hide();
    emit rejectCall(sessionID);
  }
  else
  {
    printDebug(DEBUG_PROGRAM_ERROR, this, DC_REJECT,
               "Couldn't find the invoker for reject.");
  }
}

void ConferenceView::cancel()
{
  uint32_t sessionID = sender()->property("sessionID").toUInt();
  emit cancelCall(sessionID);
}

void ConferenceView::updateTimes()
{
  bool countersRunning = false;
  viewMutex_.lock();
  for(std::map<uint32_t, std::unique_ptr<ViewInfo>>::iterator i = activeViews_.begin();
      i != activeViews_.end(); ++i)
  {
    if(i->second->out != nullptr)
    {
      i->second->out->timeout->display(i->second->out->timeout->intValue() - 1);
      countersRunning = true;
    }
  }
  viewMutex_.unlock();

  if(!countersRunning)
  {
    timeoutTimer_.stop();
  }
}
