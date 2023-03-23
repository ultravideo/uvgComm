#pragma once

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
  void attachIncomingCallWidget(uint32_t layoutID, QString name);

  // attach widget to display that we are calling somebody
  void attachOutgoingCallWidget(uint32_t layoutID, QString name);

  // attach widget to display that we are calling somebody
  void attachRingingWidget(uint32_t layoutID);

  void attachAvatarWidget(uint32_t layoutID, QString name);

  void attachVideoWidget(uint32_t layoutID, QWidget* widget);

  void attachMessageWidget(uint32_t layoutID, QString text, bool confirmButton);

  void removeWidget(uint32_t layoutID);

  void close();

signals:

  // user clicks a button in view.
  void acceptCall(uint32_t layoutID);
  void rejectCall(uint32_t layoutID);
  void cancelCall(uint32_t layoutID);

  void messageConfirmed(uint32_t layoutID);

public slots:

  // this is currently connected by videoviewfactory
  // slots for attaching and detaching view to/from layout
  // Currently only one widget can be detached for one sessionID
  void reattachWidget(uint32_t layoutID);
  void detachWidget(uint32_t layoutID);

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
  void attachWidget(uint32_t layoutID, QLayoutItem *item, LayoutLoc loc, QWidget *view);

  // update session state and attach widget.
  void updateLayoutState(SessionViewState state, QWidget* widget,
                          uint32_t layoutID, QString name = "");

  QLayoutItem* getSessionItem();

  void checkLayout(uint32_t layoutID);

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

  void initializeLayout(uint32_t layoutID);
  void unitializeSession(uint32_t layoutID);
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
