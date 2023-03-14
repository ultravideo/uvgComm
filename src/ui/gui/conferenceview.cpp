#include "conferenceview.h"

#include "ui_incomingcallwidget.h"
#include "ui_outgoingcallwidget.h"
#include "ui_messagewidget.h"
#include "ui_avatarholder.h"

#include "logger.h"

#include <QLabel>
#include <QGridLayout>
#include <QDir>

ConferenceView::ConferenceView(QWidget *parent):
  parent_(parent),
  layout_(nullptr),
  layoutWidget_(nullptr),
  activeViews_(),
  locations_(),
  nextLocation_({0,0}),
  rowMaxLength_(2)
{}


void ConferenceView::init(QGridLayout* conferenceLayout, QWidget* layoutwidget)
{
  layout_ = conferenceLayout;
  layoutWidget_ = layoutwidget;

  connect(&timeoutTimer_, &QTimer::timeout, this, &ConferenceView::updateTimes);
}


void ConferenceView::callingTo(uint32_t sessionID, QString name)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, 
             "Adding widget to display that we are calling someone.",
              {"SessionID"}, {QString::number(sessionID)});

  Q_ASSERT(sessionID);
  if(activeViews_.find(sessionID) != activeViews_.end())
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, this, 
                                    "Outgoing call already has an allocated view.");
    return;
  }
  attachOutgoingCallWidget(name, sessionID);
}


void ConferenceView::updateSessionState(SessionViewState state,
                                        QWidget* widget,
                                        uint32_t sessionID,
                                        QString name)
{
  if (activeViews_[sessionID]->state != VIEW_INACTIVE)
  {
    removeItemFromLayout(activeViews_[sessionID]->item);
    activeViews_[sessionID]->item = nullptr;
  }

  attachWidget(sessionID, activeViews_[sessionID]->item, activeViews_[sessionID]->loc, widget);
  activeViews_[sessionID]->state = state;
}


void ConferenceView::incomingCall(uint32_t sessionID, QString name)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, 
             "Adding widget to display that someone is calling us.",
              {"SessionID"}, {QString::number(sessionID)});

  if(activeViews_.find(sessionID) != activeViews_.end())
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, this, 
                                    "Incoming call already has an allocated view.",
                                    {"SessionID"}, {QString::number(sessionID)});
    return;
  }

  Logger::getLogger()->printNormal(this, "Displaying pop-up for incoming call", "Location",
                                   QString::number(nextLocation_.row) + "," +
                                   QString::number(nextLocation_.column));

  attachIncomingCallWidget(name, sessionID);
}


void ConferenceView::attachIncomingCallWidget(QString name, uint32_t sessionID)
{
  initializeSession(sessionID, name);

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

  QPixmap pixmap2(QDir::currentPath() + "/icons/start_call.svg");
  QIcon ButtonIcon2(pixmap2);
  in->acceptButton->setIcon(ButtonIcon2);
  in->acceptButton->setText("");
  in->acceptButton->setIconSize(QSize(35,35));

  frame->show();
}


void ConferenceView::attachOutgoingCallWidget(QString name, uint32_t sessionID)
{
  initializeSession(sessionID, name);

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


void ConferenceView::attachAvatarWidget(QString name, uint32_t sessionID)
{
  QFrame* holder = new QFrame;
  Ui::AvatarHolder *avatar = new Ui::AvatarHolder;
  avatar->setupUi(holder);
  avatar->name->setText(name);

  if(!timeoutTimer_.isActive())
  {
    timeoutTimer_.start(1000);
  }

  updateSessionState(VIEW_AVATAR, holder, sessionID, name);

  activeViews_[sessionID]->avatar = avatar;

  holder->show();
}


void ConferenceView::removeWithMessage(uint32_t sessionID, QString text, int timeout)
{
  QFrame* holder = new QFrame;
  Ui::MessageWidget *message = new Ui::MessageWidget;
  message->setupUi(holder);
  message->message_text->setText(text);

  if (timeout > 0)
  {
    expiringSessions_.push_back(sessionID);
    message->ok_button->hide();
    removeSessionTimer_.setSingleShot(true);
    removeSessionTimer_.start(timeout);

    connect(&removeSessionTimer_, &QTimer::timeout,
            this, &ConferenceView::expireSessions);
  }
  else
  {
    message->ok_button->setProperty("sessionID", QVariant(sessionID));
    connect(message->ok_button, SIGNAL(clicked()), this, SLOT(removeSessionFromProperty()));
  }

  updateSessionState(VIEW_MESSAGE, holder, sessionID);

  activeViews_[sessionID]->message = message;

  holder->show();
}


void ConferenceView::removeSessionFromProperty()
{
  QVariant sessionID = sender()->property("sessionID");

  if (sessionID.isValid())
  {
    unitializeSession(sessionID.toUInt());
  }
}


void ConferenceView::expireSessions()
{
  for (auto& sessionID : expiringSessions_)
  {
    unitializeSession(sessionID);
  }
  expiringSessions_.clear();

  if (activeViews_.empty())
  {
    emit lastSessionRemoved();
  }
}


void ConferenceView::removeWidget(LayoutLoc& location)
{
  QLayoutItem* item = layout_->itemAtPosition(location.row, location.column);
  QWidget* widget = item->widget();
  widget->hide();
  layout_->removeItem(item);
  freeSlot(location);
  delete item;
  delete widget;
}


void ConferenceView::attachWidget(uint32_t sessionID, QLayoutItem* item, LayoutLoc loc, QWidget *view)
{
  Q_ASSERT(view != nullptr);
  Q_ASSERT(sessionID != 0);

  // remove this item from previous position if it has one
  if(item != nullptr)
  {
    layout_->removeItem(item);
  }

  // add to layout
  layout_->addWidget(view, loc.row, loc.column);

  // get the layoutitem
  activeViews_[sessionID]->item = layout_->itemAtPosition(loc.row, loc.column);
}


void ConferenceView::reattachWidget(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  Logger::getLogger()->printNormal(this, "Reattaching widget");

  if (activeViews_.find(sessionID) == activeViews_.end() ||
      activeViews_[sessionID]->video == nullptr)
  {
    Logger::getLogger()->printProgramError(this, "Invalid state when reattaching widget");
    return;
  }
  parent_->show();
  attachWidget(sessionID,
               activeViews_[sessionID]->item,
               activeViews_[sessionID]->loc,
               activeViews_[sessionID]->video);
}


void ConferenceView::detachWidget(uint32_t sessionID)
{
  Q_ASSERT(sessionID != 0);

  Logger::getLogger()->printNormal(this, "Detaching widget from layout");

  if(activeViews_[sessionID]->item)
  {
    layout_->removeItem(activeViews_[sessionID]->item);
  }

  activeViews_[sessionID]->item = nullptr;
  parent_->hide();
}


// if our call is accepted or we accepted their call
void ConferenceView::callStarted(uint32_t sessionID, QWidget* video,
                                 bool videoEnabled, bool audioEnabled, QString name)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
                                  "Adding Videostream.", {"SessionID"}, {QString::number(sessionID)});

  // auto-accept enters straight into video mode
  if (activeViews_.find(sessionID) == activeViews_.end() || activeViews_[sessionID] == nullptr)
  {
    initializeSession(sessionID, name);
  }
  else if(activeViews_[sessionID]->state == VIEW_INACTIVE)
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_WARNING, this,
                                    "Activating video view for session state which should not be possible.",
                                    {"SessionID"}, {QString::number(sessionID)});
  }

  if (videoEnabled)
  {
    if (video != nullptr)
    {
      if (activeViews_[sessionID]->state != VIEW_VIDEO)
      {
        activeViews_[sessionID]->video = video;
        updateSessionState(VIEW_VIDEO, video, sessionID);

        // signals for double click attach/detach
        QObject::connect(video, SIGNAL(reattach(uint32_t)),
                         this,  SLOT(reattachWidget(uint32_t)));
        QObject::connect(video, SIGNAL(detach(uint32_t)),
                         this,  SLOT(detachWidget(uint32_t)));

        video->show();
      }
    }
    else
    {
      Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                      "Failed to get view.");
      return;
    }
  }
  else
  {
    attachAvatarWidget(name, sessionID);
  }
}


void ConferenceView::ringing(uint32_t sessionID)
{
  // get widget from layout and change the text.
  Logger::getLogger()->printNormal(this, "Call is ringing", "SessionID", QString::number(sessionID));
  if(activeViews_.find(sessionID) == activeViews_.end())
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "Ringing for nonexisting view. View should always exist if this function is called.",
                                    {"SessionID"}, {QString::number(sessionID)});
    return;
  }
  if (activeViews_[sessionID]->out != nullptr)
  {
    activeViews_[sessionID]->out->StatusLabel->setText("Call is ringing ...");
  }
  else
  {

    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, 
                                    "No incoming call widget exists when it should be ringing.",
                                    {"SessionID"},{QString::number(sessionID)});
  }
}


void ConferenceView::removeCaller(uint32_t sessionID)
{
  if(activeViews_.find(sessionID) == activeViews_.end())
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, 
                                    "Tried to remove the view of non-existing sessionID!");
  }
  else
  {
    unitializeSession(sessionID);
  }

  if (activeViews_.empty())
  {
    emit lastSessionRemoved();
  }
}


ConferenceView::LayoutLoc ConferenceView::getSlot()
{
  LayoutLoc location = {0,0};
  // give a freed up slot
  if(!locations_.empty())
  {
    location = locations_.front();
    locations_.pop_front();
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
        ++nextLocation_.row;
        nextLocation_.column = 0;
      }
    }
  }
  return location;
}


void ConferenceView::freeSlot(LayoutLoc &location)
{
  locations_.push_back(location);
}


void ConferenceView::close()
{
  Logger::getLogger()->printNormal(this, "Remove all views");

  for (std::map<uint32_t, std::unique_ptr<SessionViews>>::iterator view = activeViews_.begin();
       view != activeViews_.end(); ++view)
  {
    unitializeSession(std::move(view->second));
  }
  activeViews_.clear();

  resetSlots();
}


void ConferenceView::unitializeSession(uint32_t sessionID)
{
  unitializeSession(std::move(activeViews_[sessionID]));
  activeViews_.erase(sessionID);
}


void ConferenceView::unitializeSession(std::unique_ptr<SessionViews> peer)
{
  if (peer->state != VIEW_INACTIVE)
  {
    removeItemFromLayout(peer->item);
    peer->item = nullptr; // deleted by above function
    peer->state = VIEW_INACTIVE;
    freeSlot(peer->loc);

    if (peer->in)
    {
      delete peer->in;
      peer->in = nullptr;
    }

    if (peer->out)
    {
      delete peer->out;
      peer->out = nullptr;
    }

    if (peer->avatar)
    {
      delete peer->avatar;
      peer->avatar = nullptr;
    }

    if (peer->message)
    {
      delete peer->message;
      peer->message = nullptr;
    }

    peer->video = nullptr; // video is deleted elsewhere
  }
}


void ConferenceView::resetSlots()
{
  locations_.clear();
  nextLocation_ = {0,0};
  rowMaxLength_ = 2;

  Logger::getLogger()->printNormal(this, "Removing last video view. Resetting state");
}


void ConferenceView::removeItemFromLayout(QLayoutItem* item)
{
  if(item != nullptr)
  {
    layout_->removeItem(item);

    if(item->widget() != nullptr)
    {
      item->widget()->hide();
    }

    delete item;
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
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, 
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
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
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
  for(std::map<uint32_t, std::unique_ptr<SessionViews>>::iterator i = activeViews_.begin();
      i != activeViews_.end(); ++i)
  {
    if(i->second->out != nullptr)
    {
      i->second->out->timeout->display(i->second->out->timeout->intValue() - 1);
      countersRunning = true;
    }
  }

  if(!countersRunning)
  {
    timeoutTimer_.stop();
  }
}


void ConferenceView::initializeSession(uint32_t sessionID, QString name)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, 
                                  "Initializing session", {"SessionID"},
                                  {QString::number(sessionID)});



  activeViews_[sessionID] = std::unique_ptr<SessionViews>
                            (new SessionViews{VIEW_INACTIVE, name, nullptr, getSlot(),
                                              nullptr, nullptr, nullptr, nullptr});
}
