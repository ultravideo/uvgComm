#pragma once

#include "global.h"

#include <QList>
#include <QString>
#include <QMutex>
#include <QLabel>
#include <QTimer>
#include <QLCDNumber>

#include <map>
#include <deque>
#include <stdint.h>
#include <memory>

// Does the mapping of calls to their streams and upkeeps the layout of stream widgets
// TODO: the view algorithm should be improved somehow to be nicer.

enum SessionViewState {VIEW_INACTIVE,
                       VIEW_ASKING,
                       VIEW_WAITING_PEER,
                       VIEW_RINGING,
                       VIEW_MESSAGE,
                       VIEW_AVATAR,
                       VIEW_VIDEO};

class QGridLayout;
class QWidget;
class QLayoutItem;
class VideoviewFactory;


namespace Ui {
class OutgoingCall;
class IncomingCall;
class AvatarHolder;
class MessageWidget;
}


class ConferenceView : public QObject
{
  Q_OBJECT
public:
  ConferenceView(QWidget *parent);

  // init layout
  void init(QGridLayout* conferenceLayout, QWidget* layoutwidget);

  uint32_t createLayoutID();

  // attach widget to display that someone is calling us
  void attachIncomingCallWidget(LayoutID layoutID, QString name);

  // attach widget to display that we are calling somebody
  void attachOutgoingCallWidget(LayoutID layoutID, QString name);

  // attach widget to display that we are calling somebody
  void attachRingingWidget(LayoutID layoutID);

  void attachAvatarWidget(LayoutID layoutID, QString name);

  void attachVideoWidget(LayoutID layoutID, QWidget* widget);

  void attachMessageWidget(LayoutID layoutID, QString text, bool confirmButton);

  void removeWidget(LayoutID layoutID);

  void close();

signals:

  // user clicks a button in view.
  void acceptCall(LayoutID layoutID);
  void rejectCall(LayoutID layoutID);
  void cancelCall(LayoutID layoutID);

  void messageConfirmed(LayoutID layoutID);

public slots:

  // this is currently connected by videoviewfactory
  // slots for attaching and detaching view to/from layout
  // Currently only one widget can be detached for one sessionID
  void reattachWidget(LayoutID layoutID);
  void detachWidget(LayoutID layoutID);

private slots:

  // slots for accept/reject buttons. The invoker is searched.
  void accept();
  void reject();
  void cancel();

  void updateTimes();

  void getConfirmID();

private:

  // Locations in the layout
  struct LayoutLoc
  {
    uint16_t row;
    uint16_t column;
  };

  // functions for getting and freeing a location in layout
  LayoutLoc getSlot();
  void freeSlot(LayoutLoc& location);
  void resetSlots();

  // attach widget to layout
  void attachWidget(LayoutID layoutID, QLayoutItem *item, LayoutLoc loc, QWidget *view);

  // update session state and attach widget.
  void updateLayoutState(SessionViewState state, QWidget* widget,
                         LayoutID layoutID, QString name = "");

  QLayoutItem* getSessionItem();

  void checkLayout(LayoutID layoutID);

  struct LayoutView
  {
    SessionViewState state;
    QLayoutItem* item;
    LayoutLoc loc;

    Ui::OutgoingCall* out; // The view for outgoing call. May be NULL
    Ui::IncomingCall*  in; // The view for incoming call. May be NULL
    Ui::AvatarHolder* avatar;

    Ui::MessageWidget *message;

    QWidget* video;
  };

  void removeItemFromLayout(QLayoutItem* item);

  void initializeLayout(LayoutID layoutID);
  void unitializeSession(LayoutID layoutID);
  void unitializeSession(std::unique_ptr<LayoutView> peer);

  uint32_t nextLayoutID_;

  QTimer timeoutTimer_;

  QWidget *parent_;

  // dynamic widget adding to layout
  // mutex takes care of locations accessing and changes
  QGridLayout* layout_;
  QWidget* layoutWidget_;

  // key is layoutID
  std::map<uint32_t, std::unique_ptr<LayoutView>> activeViews_;

  // keeping track of freed places
  // TODO: update the whole layout with each added and removed participant. Use window width.

  std::deque<LayoutLoc> locations_;
  LayoutLoc nextLocation_;
  uint16_t rowMaxLength_;
};
