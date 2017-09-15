#pragma once

#include <QList>
#include <QString>
#include <QMutex>

#include <map>
#include <deque>

// Does the mapping of calls to their streams and upkeeps the layout of stream widgets

enum CallState {EMPTY, ASKINGUSER, WAITINGPEER, RINGING, ACTIVE};

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

  void callingTo(QString callID, QString name);
  void ringing(QString callID);
  void incomingCall(QString callID, QString name);

  QString acceptNewest();
  QString rejectNewest();

  // if our call is accepted or we accepted their call
  VideoWidget* addVideoStream(QString callID);

  void declineCall();

  // return whether there are still participants left in call view
  bool removeCaller(QString callID);

  void close();

signals:

  void acceptCall(QString callID);
  void rejectCall(QString callID);

private slots:

  void accept();
  void reject();

private:

  //TODO: also some way to keep track of freed positions
  void nextSlot();

  void attachCallingWidget(QWidget* holder, QString text);
  void addWidgetToLayout(CallState state, QWidget* widget, QString name, QString callID);

  QString findInvoker(QString buttonName);

  QWidget *parent_;

  QMutex layoutMutex_;
  QGridLayout* layout_;
  QWidget* layoutWidget_;

  // dynamic videowidget adding to layout
  // mutex takes care of locations accessing and changes
  QMutex locMutex_;
  uint8_t row_;
  uint8_t column_;

  uint16_t rowMaxLength_;

  struct CallInfo
  {
    CallState state;
    QString name;
    QLayoutItem* item;

    uint8_t row;
    uint8_t column;

    //QWidget* holder;
  };

  QMutex callsMutex_; // TODO: missing implementation
  std::map<QString, CallInfo*> activeCalls_;

  // keeping track of freed places
  // TODO: update the whole layout with each added and removed participant. Use window width.
  struct LayoutLoc
  {
    uint8_t row;
    uint8_t column;
  };

  std::deque<LayoutLoc> freedLocs_;
};
