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

void ConferenceView::callingTo(uint32_t sessionID, QString name)
{
  Q_ASSERT(sessionID);
  if(activeCalls_.size() >= sessionID && activeCalls_.at(sessionID - 1) != NULL )
  {
    qWarning() << "WARNING: Outgoing call already has an allocated view";
    return;
  }

  QLabel* label = new QLabel(parent_);
  label->setText("Calling " + name);

  QFont font = QFont("Times", 16); // TODO: change font
  label->setFont(font);
  label->setAlignment(Qt::AlignHCenter);

  addWidgetToLayout(WAITINGPEER, label, name, sessionID);
}

void ConferenceView::addWidgetToLayout(CallState state, QWidget* widget, QString name, uint32_t sessionID)
{
  locMutex_.lock();
  uint16_t row = row_;
  uint16_t column = column_;

  if(!freedLocs_.empty())
  {
    row = freedLocs_.front().row;
    column = freedLocs_.front().column;
  }

  layoutMutex_.lock();
  layout_->addWidget(widget, row, column);

  while(activeCalls_.size() < sessionID)
  {
    activeCalls_.append(NULL);
  }

  activeCalls_[sessionID - 1] = new CallInfo{state, name, layout_->itemAtPosition(row,column),
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

void ConferenceView::incomingCall(uint32_t sessionID, QString name)
{
  if(activeCalls_.size() >= sessionID && activeCalls_.at(sessionID - 1) != NULL)
  {
    qWarning() << "WARNING: Incoming call already has an allocated view. Session:" << sessionID
               << "Slots:" << activeCalls_.size();
    return;
  }
  qDebug() << "Displaying pop-up for somebody calling in slot:" << row_ << "," << column_;
  QWidget* holder = new QWidget;
  attachCallingWidget(holder, name + " is calling..");
  addWidgetToLayout(ASKINGUSER, holder, name, sessionID);
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


void ConferenceView::attachVideoWidget(uint32_t sessionID, VideoWidget* view)
{
  layoutMutex_.lock();
  layout_->removeItem(activeCalls_[sessionID - 1]->item);
  layout_->addWidget(view, activeCalls_[sessionID - 1]->row, activeCalls_[sessionID - 1]->column);
  activeCalls_[sessionID - 1]->item = layout_->itemAtPosition(activeCalls_[sessionID - 1]->row,
                                                       activeCalls_[sessionID - 1]->column);
  layoutMutex_.unlock();
}

// if our call is accepted or we accepted their call
VideoWidget* ConferenceView::addVideoStream(uint32_t sessionID)
{
  VideoWidget* view = new VideoWidget(NULL, sessionID, 1);
  if(activeCalls_.size() < sessionID || activeCalls_.at(sessionID - 1) == NULL)
  {
    qWarning() << "WARNING: Adding a videostream without previous.";
    return NULL;
  }
  else if(activeCalls_[sessionID - 1]->state != ASKINGUSER
          && activeCalls_[sessionID - 1]->state != WAITINGPEER)
  {
    qWarning() << "WARNING: activating stream without previous state set";
    return NULL;
  }

  // add the widget in place of previous one
  activeCalls_[sessionID - 1]->state = CALL_ACTIVE;

  // TODO delete previous widget now instead of with parent.
  // Now they accumulate in memory until call has ended
  delete activeCalls_[sessionID - 1]->item->widget();

  attachVideoWidget(sessionID, view);

  QObject::connect(view, &VideoWidget::reattach, this, &ConferenceView::attachVideoWidget);

  //view->setParent(0);
  //view->showMaximized();
  //view->show();
  //view->setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint);

  return view;
}

void ConferenceView::ringing(uint32_t sessionID)
{
  // get widget from layout and change the text.
  qDebug() << sessionID << "call is ringing. TODO: display it to user";
}

bool ConferenceView::removeCaller(uint32_t sessionID)
{
  qDebug() << "Removing call window. Session:"
           << sessionID << "Total slots:" << activeCalls_.size();
  if(activeCalls_.size() < sessionID
     || activeCalls_[sessionID - 1] == NULL)
  {
    qWarning() << "WARNING: Trying to remove nonexisting call from ConferenceView.";
    return !activeCalls_.empty();
  }

  if(activeCalls_[sessionID - 1]->item != NULL)
  {
    layoutMutex_.lock();
    layout_->removeItem(activeCalls_[sessionID - 1]->item);
    layoutMutex_.unlock();
  }

  if(activeCalls_[sessionID - 1]->state == CALL_ACTIVE
     || activeCalls_[sessionID - 1]->state == ASKINGUSER
     || activeCalls_[sessionID - 1]->state == WAITINGPEER)
  {
    activeCalls_[sessionID - 1]->item->widget()->hide();
    delete activeCalls_[sessionID - 1]->item->widget();
  }

  locMutex_.lock();
  if(activeCalls_.size() != 1)
  {
    freedLocs_.push_back({activeCalls_[sessionID - 1]->row, activeCalls_[sessionID - 1]->column});
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
  delete activeCalls_.at(sessionID - 1);
  activeCalls_[sessionID - 1] = NULL;

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
  for(unsigned int i = 0; i < activeCalls_.size(); ++i)
  {
    if(activeCalls_.at(i) != NULL)
    {
      layoutMutex_.lock();
      layout_->removeItem(activeCalls_.at(i)->item);
      layoutMutex_.unlock();
      removeCaller(i + 1);
    }
  }

  // not strictly needed, but just in case.
  activeCalls_.clear();

  locMutex_.lock();
  row_ = 0;
  column_ = 0;
  rowMaxLength_ = 2;

  locMutex_.unlock();
}

uint32_t ConferenceView::findInvoker(QString buttonName)
{
  // TODO: bug warning: what if a sessionID has left the call and the sessionID no longer matches the widget positions?

  uint32_t sessionID = 0;
  QPushButton* button = qobject_cast<QPushButton*>(sender());
  for(unsigned int i = 0; i < activeCalls_.size(); ++i)
  {
    if(activeCalls_.at(i) != NULL)
    {
      if(!activeCalls_.at(i)->item->widget()->findChildren<QPushButton *>(buttonName).isEmpty())
      {
        QPushButton* pb = activeCalls_.at(i)->item->widget()->findChildren<QPushButton *>(buttonName).first();
        if(pb == button)
        {
          sessionID = i + 1; // sessionID is never 0
          qDebug() << "Found invoking" << buttonName << "Session ID:" << sessionID;
        }
      }
    }
  }

  Q_ASSERT(sessionID != 0);
  Q_ASSERT(sessionID <= activeCalls_.size());
  return sessionID;
}

void ConferenceView::accept()
{
  uint32_t sessionID = findInvoker("AcceptButton");
  emit acceptCall(sessionID);
}

void ConferenceView::reject()
{
  uint32_t sessionID = findInvoker("DeclineButton");
  emit rejectCall(sessionID);
}
