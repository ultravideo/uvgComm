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
                       VIEW_VIDEO};

class QGridLayout;
class QWidget;
class QLayoutItem;
class VideoviewFactory;


namespace Ui {
class OutgoingCall;
class IncomingCall;
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
  void addVideoStream(uint32_t sessionID, std::shared_ptr<VideoviewFactory> factory);

  // return whether there are still participants left in call view
  bool removeCaller(uint32_t sessionID);

  void close();

signals:

  // user clicks a button in view.
  void acceptCall(uint32_t sessionID);
  void rejectCall(uint32_t sessionID);
  void cancelCall(uint32_t sessionID);

public slots:

  // this is currently connected by videoviewfactory
  // slots for attaching and detaching view to/from layout
  void reattachWidget(uint32_t sessionID);
  void detachWidget(uint32_t sessionID, QWidget *view);

private slots:

  // slots for accept/rejectu buttons. The invoker is searched.
  void accept();
  void reject();
  void cancel();

  void updateTimes();

private:

  //TODO: also some way to keep track of freed positions
  void nextSlot();

  void attachWidget(uint32_t sessionID, uint32_t index, QWidget *view);

  void attachIncomingCallWidget(QString name, uint32_t sessionID);
  void attachOutgoingCallWidget(QString name, uint32_t sessionID);
  void addWidgetToLayout(SessionViewState state, QWidget* widget,
                         QString name, uint32_t sessionID);

  QLayoutItem* getSessionItem();

  void resetFreedLocations();

  struct ViewInfo
  {
    QLayoutItem* item;

    uint16_t row;
    uint16_t column;
  };

  struct SessionViews
  {
    SessionViewState state;
    QString name;

    std::vector<ViewInfo> views_;

    Ui::OutgoingCall* out; // The view for outgoing call. May be NULL
    Ui::IncomingCall*  in; // The view for incoming call. May be NULL
  };

  // low level function which handles the destruction of callInfo struct
  void uninitDetachedWidget(uint32_t sessionID);

  void uninitializeView(ViewInfo& view);

  // return true if session is exists and is initialized correctly
  bool checkSession(uint32_t sessionID, uint32_t minViewCount = 0);
  void initializeSession(uint32_t sessionID, QString name);
  void unitializeSession(std::unique_ptr<SessionViews> peer);

  QTimer timeoutTimer_;

  QWidget *parent_;

  QMutex layoutMutex_; // prevent modifying the layout at the same time
  QGridLayout* layout_;
  QWidget* layoutWidget_;

  // dynamic widget adding to layout
  // mutex takes care of locations accessing and changes
  QMutex locMutex_; // prevent reserving same location by two threads
  QMutex viewMutex_; // prevent modifying activeViews at the same time

  struct DetachedWidget
  {
    QWidget* widget;
    uint32_t index;
  };

  // matches sessionID - 1, but is not the definitive source of sessionID.
  std::map<uint32_t, std::unique_ptr<SessionViews>> activeViews_;
  std::map<uint32_t, DetachedWidget> detachedWidgets_;

  // keeping track of freed places
  // TODO: update the whole layout with each added and removed participant. Use window width.
  struct LayoutLoc
  {
    uint16_t row;
    uint16_t column;
  };

  std::deque<LayoutLoc> freedLocs_;

  uint16_t row_;
  uint16_t column_;
  uint16_t rowMaxLength_;
};
