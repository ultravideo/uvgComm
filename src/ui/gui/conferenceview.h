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
}


class ConferenceView : public QObject
{
  Q_OBJECT
public:
  ConferenceView(QWidget *parent);

  // init layout
  void init(QGridLayout* conferenceLayout, QWidget* layoutwidget);

  // showing information to user and reserving the slot in view.
  void callingTo(uint32_t sessionID, QString name);
  void ringing(uint32_t sessionID);
  void incomingCall(uint32_t sessionID, QString name);

  uint32_t acceptNewest();
  uint32_t rejectNewest();

  // if our call is accepted or we accepted their call
  void callStarted(uint32_t sessionID, QWidget *video,
                   bool videoEnabled, bool audioEnabled, QString name);

  // return whether there are still participants left in call view
  bool removeCaller(uint32_t sessionID);

  void attachMessageWidget(QString text, int timeout);

  void close();

signals:

  // user clicks a button in view.
  void acceptCall(uint32_t sessionID);
  void rejectCall(uint32_t sessionID);
  void cancelCall(uint32_t sessionID);

public slots:

  // this is currently connected by videoviewfactory
  // slots for attaching and detaching view to/from layout
  // Currently only one widget can be detached for one sessionID
  void reattachWidget(uint32_t sessionID);
  void detachWidget(uint32_t sessionID);

private slots:

  // slots for accept/reject buttons. The invoker is searched.
  void accept();
  void reject();
  void cancel();

  void updateTimes();

  void expireMessages();
  void removeMessage();

private:

  // Locations in the layout
  struct LayoutLoc
  {
    uint16_t row;
    uint16_t column;
  };

  // functions for getting and freeing a location in layout
  LayoutLoc nextSlot();
  void freeSlot(LayoutLoc& location);
  void resetSlots();

  // attach widget to layout
  void attachWidget(uint32_t sessionID, QLayoutItem *item, LayoutLoc loc, QWidget *view);

  // attach widget to display that someone is calling us
  void attachIncomingCallWidget(QString name, uint32_t sessionID);

  // attach widget to display that we are calling somebody
  void attachOutgoingCallWidget(QString name, uint32_t sessionID);


  void attachAvatarWidget(QString name, uint32_t sessionID);

  // update session state and attach widget.
  void updateSessionState(SessionViewState state, QWidget* widget,
                          uint32_t sessionID, QString name = "");

  QLayoutItem* getSessionItem();

  struct SessionViews
  {
    SessionViewState state;
    QString name;
    QLayoutItem* item;
    LayoutLoc loc;

    Ui::OutgoingCall* out; // The view for outgoing call. May be NULL
    Ui::IncomingCall*  in; // The view for incoming call. May be NULL
    Ui::AvatarHolder* avatar;

    QWidget* video;
  };


  void uninitializeView(QLayoutItem* item, LayoutLoc loc);

  void removeWidget(LayoutLoc& location);

  void initializeSession(uint32_t sessionID, QString name);
  void unitializeSession(std::unique_ptr<SessionViews> peer);

  QTimer timeoutTimer_;
  QTimer removeMessageTimer_;

  QList<LayoutLoc> expiringWidgets_;

  QWidget *parent_;

  // dynamic widget adding to layout
  // mutex takes care of locations accessing and changes
  QGridLayout* layout_;
  QWidget* layoutWidget_;

  // key is sessionID
  std::map<uint32_t, std::unique_ptr<SessionViews>> activeViews_;

  // keeping track of freed places
  // TODO: update the whole layout with each added and removed participant. Use window width.

  QMutex locMutex_; // prevent reserving same location by two threads
  std::deque<LayoutLoc> freedLocs_;
  LayoutLoc nextLocation_;
  uint16_t rowMaxLength_;
};
