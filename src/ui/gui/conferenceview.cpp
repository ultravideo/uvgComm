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
  nextLocation_({0,0}),
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
  printDebug(DEBUG_NORMAL, this, DC_START_CALL,
             "Adding widget to display that we are calling someone.",
              {"SessionID"}, {QString::number(sessionID)});

  Q_ASSERT(sessionID);
  if(activeViews_.find(sessionID) != activeViews_.end())
  {
    printDebug(DEBUG_WARNING, this, DC_START_CALL, "Outgoing call already has an allocated view.");
    return;
  }

  attachOutgoingCallWidget(name, sessionID);
}

void ConferenceView::updateSessionState(SessionViewState state, QWidget* widget, uint32_t sessionID,
                                       QString name)
{
  LayoutLoc location = nextSlot();

  // initialize if session has not been initialized
  if (!checkSession(sessionID))
  {
    initializeSession(sessionID, name);
  }

  viewMutex_.lock();
  // remove all previous widgets if they are not videostreams
  if (activeViews_[sessionID]->state != VIEW_VIDEO)
  {
    printDebug(DEBUG_NORMAL, this, DC_START_CALL,
               "Clearing all previous views.");
    for (auto view : activeViews_[sessionID]->views_)
    {
      uninitializeView(view);
    }

    activeViews_[sessionID]->views_.clear();

    activeViews_[sessionID]->in = nullptr;
    activeViews_[sessionID]->out = nullptr;
  }
  viewMutex_.unlock();

  activeViews_[sessionID]->state = state;
  activeViews_[sessionID]->views_.push_back({nullptr, location}); // item is set by attachwidget

  attachWidget(sessionID, activeViews_[sessionID]->views_.size() - 1, widget);
}

void ConferenceView::incomingCall(uint32_t sessionID, QString name)
{
  printDebug(DEBUG_NORMAL, this, DC_START_CALL,
             "Adding widget to display that someone is calling us.",
              {"SessionID"}, {QString::number(sessionID)});

  if(activeViews_.find(sessionID) != activeViews_.end())
  {
    printDebug(DEBUG_WARNING, this, DC_START_CALL, "Incoming call already has an allocated view.",
      {"SessionID"}, {QString::number(sessionID)});
    return;
  }
  qDebug() << "Incoming call," << metaObject()->className()
           << "Displaying pop-up for somebody. Next layout location: "
           << nextLocation_.row << "," << nextLocation_.column;

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

  updateSessionState(VIEW_ASKING, frame, sessionID, name);
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

  updateSessionState(VIEW_WAITING_PEER, holder, sessionID, name);

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

void ConferenceView::attachWidget(uint32_t sessionID, uint32_t index, QWidget *view)
{
  layoutMutex_.lock();
  if(checkSession(sessionID, index + 1))
  {
    // remove this item from previous position if it has one
    if(activeViews_[sessionID]->views_.at(index).item)
    {
      layout_->removeItem(activeViews_[sessionID]->views_.at(index).item);
    }

    // add to layout
    layout_->addWidget(view,
                       activeViews_[sessionID]->views_.at(index).location.row,
                       activeViews_[sessionID]->views_.at(index).location.column);

    // get the layoutitem
    activeViews_[sessionID]->views_.at(index).item =
        layout_->itemAtPosition(activeViews_[sessionID]->views_.at(index).location.row,
                                activeViews_[sessionID]->views_.at(index).location.column);
  }
  else {

    printDebug(DEBUG_PROGRAM_ERROR, this, DC_FULLSCREEN,
               "Trying to attach fullscreenview back to layout when sessionID does not exist in conference view.",
               {"SessionID"}, {QString::number(sessionID)});
  }
  layoutMutex_.unlock();
}

void ConferenceView::reattachWidget(uint32_t sessionID)
{
  if (detachedWidgets_.find(sessionID) != detachedWidgets_.end())
  {
    attachWidget(sessionID, detachedWidgets_[sessionID].index, detachedWidgets_[sessionID].widget);

    // remove from detached widgets
    detachedWidgets_.erase(sessionID);

    // if there are no detached widgets blocking the view, then show the window
    if(detachedWidgets_.empty())
    {
      parent_->show();
    }
  }
  else {
    printDebug(DEBUG_PROGRAM_WARNING, this, DC_FULLSCREEN,
               "Tried reattaching widget without detaching");
  }
}

void ConferenceView::detachWidget(uint32_t sessionID, QWidget *view)
{
  layoutMutex_.lock();

  if(checkSession(sessionID))
  {
    uint32_t index = 0;
    bool found = false;
    for (unsigned int i = 0; i < activeViews_[sessionID]->views_.size(); ++i)
    {
      if (activeViews_[sessionID]->views_.at(i).item != nullptr &&
          activeViews_[sessionID]->views_.at(i).item->widget() == view)
      {
        index = i;
        found = true;
      }
    }

    if (!found)
    {
      printDebug(DEBUG_PROGRAM_ERROR, this, DC_FULLSCREEN, "Could not find widget to be detached");
      return;
    }

    if(activeViews_[sessionID]->views_.at(index).item)
    {
      layout_->removeItem(activeViews_[sessionID]->views_.at(index).item);
    }
    layout_->removeWidget(view);
    activeViews_[sessionID]->views_.at(index).item = nullptr;
    detachedWidgets_[sessionID] = {view, index};

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
void ConferenceView::addVideoStream(uint32_t sessionID,
                                    std::shared_ptr<VideoviewFactory> factory)
{
  printDebug(DEBUG_NORMAL, this, DC_ADD_MEDIA,
             "Adding Videostream.", {"SessionID"}, {QString::number(sessionID)});

  // create the view
  uint32_t id = factory->createWidget(sessionID, nullptr, this);
  QWidget* view = factory->getView(sessionID, id);

  if(!checkSession(sessionID))
  {
    printDebug(DEBUG_NORMAL, this, DC_ADD_MEDIA,
               "Did not find previous session. Assuming auto-accept and adding widget anyway",
              {"SessionID"}, {QString::number(sessionID)});
  }
  else if(activeViews_[sessionID]->state == VIEW_INACTIVE)
  {
    printDebug(DEBUG_PROGRAM_WARNING, this, DC_ADD_MEDIA,
                     "Activating video view for session state which should not be possible.",
                    {"SessionID"}, {QString::number(sessionID)});
  }

  updateSessionState(VIEW_VIDEO, view, sessionID);
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
    unitializeSession(std::move(activeViews_[sessionID]));
    activeViews_.erase(sessionID);

    uninitDetachedWidget(sessionID);

    viewMutex_.unlock();
  }

  return !activeViews_.empty();
}

ConferenceView::LayoutLoc ConferenceView::nextSlot()
{
  LayoutLoc location = {0,0};
  locMutex_.lock();
  // give a freed up slot
  if(!freedLocs_.empty())
  {
    location = freedLocs_.front();
    freedLocs_.pop_front();
  }
  else // get the next slot
  {
    location = nextLocation_;

    // update the values for next slot
    ++nextLocation_.column;
    if(nextLocation_.column == rowMaxLength_)
    {
      // move to first row, third place
      if(nextLocation_.row == 1 && nextLocation_.column == 2)
      {
        rowMaxLength_ = 3;
        nextLocation_.row = 0;
        nextLocation_.column = 2;
      }
      // move to second row, third place
      else if(nextLocation_.row == 0 && nextLocation_.column == 3)
      {
        nextLocation_.row = 1;
        nextLocation_.column = 2;
      }
      else if(nextLocation_.row == 2 && nextLocation_.column == 3)
      {
        ++nextLocation_.row;
        nextLocation_.column = 1;
      }
      else // move forward
      {
        nextLocation_.column = 0;
        ++nextLocation_.row;
      }
    }
  }
  locMutex_.unlock();
  return location;
}


void ConferenceView::freeSlot(LayoutLoc &location)
{
  locMutex_.lock();
  freedLocs_.push_back(location);
  // TODO: reorder these
  locMutex_.unlock();
}

void ConferenceView::close()
{
  qDebug() << "End all calls," << metaObject()->className() << ": Closing views of the call with"
           << detachedWidgets_.size() << "detached widgets";
  for (std::map<uint32_t, std::unique_ptr<SessionViews>>::iterator view = activeViews_.begin();
       view != activeViews_.end(); ++view)
  {
    unitializeSession(std::move(view->second));
  }

  for (std::map<uint32_t, DetachedWidget>::iterator view = detachedWidgets_.begin();
       view != detachedWidgets_.end(); ++view)
  {
    uninitDetachedWidget(view->first);
  }

  detachedWidgets_.clear();
  activeViews_.clear();

  resetSlots();
}

void ConferenceView::unitializeSession(std::unique_ptr<SessionViews> peer)
{
  if(peer->state != VIEW_INACTIVE)
  {
    for (auto view : peer->views_)
    {
      uninitializeView(view);
    }
  }

  // record the freed locations so we can later insert new views to them if need be
  // TODO: Reorder the layout everytime a view is removed.
  if(activeViews_.size() == 1)
  {
    resetSlots();
  }
}

void ConferenceView::resetSlots()
{
  locMutex_.lock();
  freedLocs_.clear();
  nextLocation_ = {0,0};
  rowMaxLength_ = 2;
  locMutex_.unlock();
  qDebug() << "Closing," << metaObject()->className() << ": Removing last video view. Clearing previous data";
}

void ConferenceView::uninitializeView(ViewInfo& view)
{
  if(view.item != nullptr)
  {
    layoutMutex_.lock();
    layout_->removeItem(view.item);
    layoutMutex_.unlock();

    if(view.item->widget() != nullptr)
    {
      view.item->widget()->hide();
      delete view.item->widget();
    }

    freeSlot(view.location);

    view.item = nullptr;
  }
}

void ConferenceView::uninitDetachedWidget(uint32_t sessionID)
{
  if(detachedWidgets_.find(sessionID) != detachedWidgets_.end())
  {
    qDebug() << "Closing," << metaObject()->className() << ": The widget was detached for sessionID" << sessionID << "so we destroy it.";
    delete detachedWidgets_[sessionID].widget;
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
  for(std::map<uint32_t, std::unique_ptr<SessionViews>>::iterator i = activeViews_.begin();
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


// return true if session is exists and is initialized correctly
bool ConferenceView::checkSession(uint32_t sessionID, uint32_t minViewCount)
{
  if (activeViews_.find(sessionID) == activeViews_.end() ||
      (activeViews_[sessionID]->state != VIEW_INACTIVE
       && activeViews_[sessionID]->views_.empty())
      || activeViews_[sessionID]->views_.size() < minViewCount)
  {
    return false;
  }

  return true;
}


void ConferenceView::initializeSession(uint32_t sessionID, QString name)
{
  if (checkSession(sessionID))
  {
    printDebug(DEBUG_PROGRAM_WARNING, this, DC_START_CALL,
               "Tried to initialize an already initialized view session");
    return;
  }

  viewMutex_.lock();
  activeViews_[sessionID] = std::unique_ptr<SessionViews>
                            (new SessionViews{VIEW_INACTIVE, name,
                                              {}, nullptr, nullptr});
  viewMutex_.unlock();
}
