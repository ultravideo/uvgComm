#pragma once

#include <QList>
#include <QString>
#include <QMutex>

#include <map>

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
  void removeCaller(QString callID);

  void close();

signals:

  void acceptCall(QString callID);
  void rejectCall(QString callID);

private slots:

  void accept();
  void reject();

private:

  void nextSlot();

  QWidget *parent_;

  QMutex layoutMutex_;
  QGridLayout* layout_;
  QWidget* layoutWidget_;
  Ui::CallerWidget* callingWidget_;
  QWidget* holdingWidget_;
  // dynamic videowidget adding to layout

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
  };

  QMutex callsMutex_; // TODO: missing implementation
  std::map<QString, CallInfo*> activeCalls_;

  // TODO: enable more than one call ringing at the same time
  QList<QString> askingQueue_;
  //QList<CallInfo*> deniedCalls_;

};
