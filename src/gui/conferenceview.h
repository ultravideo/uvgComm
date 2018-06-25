#pragma once

#include <QList>
#include <QString>
#include <QMutex>

#include <map>
#include <deque>
#include <stdint.h>

// Does the mapping of calls to their streams and upkeeps the layout of stream widgets

enum CallState {EMPTY, ASKINGUSER, WAITINGPEER, CALL_RINGING, CALL_ACTIVE};

class QGridLayout;
class VideoWidget;
class QWidget;
class QLayoutItem;

namespace Ui {
class CallerWidget;
}

class ConferenceView : public QObject
{
  Q_OBJECT
public:
  ConferenceView(QWidget *parent);

  void init(QGridLayout* conferenceLayout, QWidget* layoutwidget);

  void callingTo(uint32_t sessionID, QString name);
  void ringing(uint32_t sessionID);
  void incomingCall(uint32_t sessionID, QString name);

  uint32_t acceptNewest();
  uint32_t rejectNewest();

  // if our call is accepted or we accepted their call
  VideoWidget* addVideoStream(uint32_t sessionID);

  void declineCall();

  // return whether there are still participants left in call view
  bool removeCaller(uint32_t sessionID);

  void close();

signals:

  void acceptCall(uint32_t sessionID);
  void rejectCall(uint32_t sessionID);

private slots:

  void accept();
  void reject();

private:

  //TODO: also some way to keep track of freed positions
  void nextSlot();

  void attachCallingWidget(QWidget* holder, QString text);
  void addWidgetToLayout(CallState state, QWidget* widget, QString name, uint32_t sessionID);

  uint32_t findInvoker(QString buttonName);

  QWidget *parent_;

  QMutex layoutMutex_;
  QGridLayout* layout_;
  QWidget* layoutWidget_;

  // dynamic videowidget adding to layout
  // mutex takes care of locations accessing and changes
  QMutex locMutex_;
  uint16_t row_;
  uint16_t column_;

  uint16_t rowMaxLength_;

  struct CallInfo
  {
    CallState state;
    QString name;
    QLayoutItem* item;

    uint16_t row;
    uint16_t column;

    //QWidget* holder;
  };

  QMutex callsMutex_; // TODO: missing implementation

  // matches sessionID - 1, but is not the definitive source of sessionID.
  QList<CallInfo*> activeCalls_;

  // keeping track of freed places
  // TODO: update the whole layout with each added and removed participant. Use window width.
  struct LayoutLoc
  {
    uint16_t row;
    uint16_t column;
  };

  std::deque<LayoutLoc> freedLocs_;
};
