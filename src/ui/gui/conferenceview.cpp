#include "conferenceview.h"

#include "ui_incomingcallwidget.h"
#include "ui_outgoingcallwidget.h"
#include "ui_messagewidget.h"
#include "ui_avatarholder.h"

#include "logger.h"

#include <QLabel>
#include <QGridLayout>
#include <QDir>

const uint32_t INITIAL_LAYOUT_ID = 1;

ConferenceView::ConferenceView(QWidget *parent):
  nextLayoutID_(INITIAL_LAYOUT_ID),
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


uint32_t ConferenceView::createLayoutID()
{
  ++nextLayoutID_;
  return nextLayoutID_ - 1;
}


void ConferenceView::removeWidget(uint32_t layoutID)
{
  if(activeViews_.find(layoutID) != activeViews_.end())
  {
    if (activeViews_[layoutID]->item == nullptr)
    {
      reattachWidget(layoutID);
    }

    unitializeSession(layoutID);

    if (activeViews_.empty())
    {
      nextLayoutID_ = INITIAL_LAYOUT_ID;
    }
  }
}


void ConferenceView::updateLayoutState(SessionViewState state,
                                        QWidget* widget,
                                        uint32_t layoutID,
                                        QString name)
{
  if (activeViews_[layoutID]->state != VIEW_INACTIVE)
  {
    removeItemFromLayout(activeViews_[layoutID]->item);
    activeViews_[layoutID]->item = nullptr;
  }

  attachWidget(layoutID, activeViews_[layoutID]->item, activeViews_[layoutID]->loc, widget);
  activeViews_[layoutID]->state = state;
}


void ConferenceView::attachIncomingCallWidget(uint32_t layoutID, QString name)
{
  checkLayout(layoutID);

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

  updateLayoutState(VIEW_ASKING, frame, layoutID, name);

  activeViews_[layoutID]->in = in;

  in->acceptButton->setProperty("layoutID", QVariant(layoutID));
  in->declineButton->setProperty("layoutID", QVariant(layoutID));

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


void ConferenceView::attachOutgoingCallWidget(uint32_t layoutID, QString name)
{
  checkLayout(layoutID);

  QFrame* holder = new QFrame;
  Ui::OutgoingCall *out = new Ui::OutgoingCall;
  out->setupUi(holder);
  out->NameLabel->setText(name);
  out->StatusLabel->setText("Connecting ...");

  if(!timeoutTimer_.isActive())
  {
    timeoutTimer_.start(1000);
  }

  updateLayoutState(VIEW_WAITING_PEER, holder, layoutID, name);

  activeViews_[layoutID]->out = out;

  out->cancelCall->setProperty("layoutID", QVariant(layoutID));
  connect(out->cancelCall, SIGNAL(clicked()), this, SLOT(cancel()));

  QPixmap pixmap(QDir::currentPath() + "/icons/end_call.svg");
  QIcon ButtonIcon(pixmap);
  out->cancelCall->setIcon(ButtonIcon);
  out->cancelCall->setText("");
  out->cancelCall->setIconSize(QSize(35,35));

  holder->show();
}


void ConferenceView::attachRingingWidget(uint32_t layoutID)
{
  activeViews_[layoutID]->out->StatusLabel->setText("Call is ringing ...");
}


void ConferenceView::attachAvatarWidget(uint32_t layoutID, QString name)
{
  checkLayout(layoutID);
  if (activeViews_[layoutID]->item == nullptr)
  {
    reattachWidget(layoutID);
  }

  QFrame* holder = new QFrame;
  Ui::AvatarHolder *avatar = new Ui::AvatarHolder;
  avatar->setupUi(holder);
  avatar->name->setText(name);

  if(!timeoutTimer_.isActive())
  {
    timeoutTimer_.start(1000);
  }

  updateLayoutState(VIEW_AVATAR, holder, layoutID, name);

  activeViews_[layoutID]->avatar = avatar;

  holder->show();
}


// if our call is accepted or we accepted their call
void ConferenceView::attachVideoWidget(uint32_t layoutID, QWidget* widget)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this,
                                  "Adding Videostream.", {"layoutID"}, {QString::number(layoutID)});

  checkLayout(layoutID);

  if (widget != nullptr)
  {
    activeViews_[layoutID]->video = widget;
    updateLayoutState(VIEW_VIDEO, widget, layoutID);

    // signals for double click attach/detach
    QObject::connect(widget, SIGNAL(reattach(uint32_t)),
                     this,  SLOT(reattachWidget(uint32_t)));
    QObject::connect(widget, SIGNAL(detach(uint32_t)),
                     this,  SLOT(detachWidget(uint32_t)));

    widget->show();
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "Video view not provided");
    return;
  }
}


void ConferenceView::attachMessageWidget(uint32_t layoutID, QString text, bool confirmButton)
{
  if (activeViews_.find(layoutID) != activeViews_.end() && activeViews_[layoutID] != nullptr)
  {
    if (activeViews_[layoutID]->item == nullptr)
    {
      reattachWidget(layoutID);
    }

    QFrame* holder = new QFrame;
    Ui::MessageWidget *message = new Ui::MessageWidget;
    message->setupUi(holder);
    message->message_text->setText(text);

    if (confirmButton)
    {
      message->ok_button->setProperty("layoutID", QVariant(layoutID));
      connect(message->ok_button, &QPushButton::clicked, this, &ConferenceView::getConfirmID);
    }
    else
    {
      message->ok_button->hide();
    }

    updateLayoutState(VIEW_MESSAGE, holder, layoutID);
    activeViews_[layoutID]->message = message;
    holder->show();
  }
}


void ConferenceView::attachWidget(uint32_t layoutID, QLayoutItem* item, LayoutLoc loc, QWidget *view)
{
  Q_ASSERT(view != nullptr);
  Q_ASSERT(layoutID != 0);

  // remove this item from previous position if it has one
  if(item != nullptr)
  {
    layout_->removeItem(item);
  }

  // add to layout
  layout_->addWidget(view, loc.row, loc.column);

  // get the layoutitem
  activeViews_[layoutID]->item = layout_->itemAtPosition(loc.row, loc.column);
}


void ConferenceView::reattachWidget(uint32_t layoutID)
{
  Q_ASSERT(layoutID != 0);

  Logger::getLogger()->printNormal(this, "Reattaching widget");

  if (activeViews_.find(layoutID) == activeViews_.end() ||
      activeViews_[layoutID]->video == nullptr)
  {
    Logger::getLogger()->printProgramError(this, "Invalid state when reattaching widget");
    return;
  }
  parent_->show();
  attachWidget(layoutID,
               activeViews_[layoutID]->item,
               activeViews_[layoutID]->loc,
               activeViews_[layoutID]->video);
}


void ConferenceView::detachWidget(uint32_t layoutID)
{
  Q_ASSERT(layoutID != 0);

  Logger::getLogger()->printNormal(this, "Detaching widget from layout");

  if(activeViews_[layoutID]->item)
  {
    layout_->removeItem(activeViews_[layoutID]->item);
  }

  activeViews_[layoutID]->item = nullptr;
  parent_->hide();
}


void ConferenceView::getConfirmID()
{
  QVariant layoutID = sender()->property("layoutID");

  if (layoutID.isValid())
  {
    emit messageConfirmed(layoutID.toUInt());
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

  for (std::map<uint32_t, std::unique_ptr<LayoutView>>::iterator view = activeViews_.begin();
       view != activeViews_.end(); ++view)
  {
    unitializeSession(std::move(view->second));
  }
  activeViews_.clear();

  resetSlots();
}


void ConferenceView::unitializeSession(uint32_t layoutID)
{
  unitializeSession(std::move(activeViews_[layoutID]));
  activeViews_.erase(layoutID);
}


void ConferenceView::unitializeSession(std::unique_ptr<LayoutView> peer)
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
  uint32_t layoutID = sender()->property("layoutID").toUInt();
  if (layoutID != 0
      && activeViews_.find(layoutID) != activeViews_.end()
      && activeViews_[layoutID]->state == VIEW_ASKING
      && activeViews_[layoutID]->in != nullptr)
  {
    activeViews_[layoutID]->in->acceptButton->hide();
    activeViews_[layoutID]->in->declineButton->hide();
    emit acceptCall(layoutID);
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this, 
                                    "Couldn't find the invoker for accept.");
  }
}


void ConferenceView::reject()
{
  uint32_t layoutID = sender()->property("layoutID").toUInt();
  if (layoutID != 0
      && activeViews_.find(layoutID) != activeViews_.end()
      && activeViews_[layoutID]->state == VIEW_ASKING
      && activeViews_[layoutID]->in != nullptr)
  {
    activeViews_[layoutID]->in->acceptButton->hide();
    activeViews_[layoutID]->in->declineButton->hide();

    emit rejectCall(layoutID);
  }
  else
  {
    Logger::getLogger()->printDebug(DEBUG_PROGRAM_ERROR, this,
                                    "Couldn't find the invoker for reject.");
  }
}


void ConferenceView::cancel()
{
  uint32_t layoutID = sender()->property("layoutID").toUInt();
  emit cancelCall(layoutID);
}


void ConferenceView::updateTimes()
{
  bool countersRunning = false;
  for(std::map<uint32_t, std::unique_ptr<LayoutView>>::iterator i = activeViews_.begin();
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


void ConferenceView::initializeLayout(uint32_t layoutID)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, 
                                  "Initializing session", {"layoutID"},
                                  {QString::number(layoutID)});

  activeViews_[layoutID] = std::unique_ptr<LayoutView>
                            (new LayoutView{VIEW_INACTIVE, nullptr, getSlot(),
                                              nullptr, nullptr, nullptr, nullptr, nullptr});
}


void ConferenceView::checkLayout(uint32_t layoutID)
{
  if (activeViews_.find(layoutID) == activeViews_.end())
  {
    initializeLayout(layoutID);
  }
}
