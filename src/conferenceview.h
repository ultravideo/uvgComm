#pragma once

#include <QString>

class QGridLayout;
class VideoWidget;
class QWidget;

class ConferenceView
{
public:
  ConferenceView(QWidget *parent);

  void init(QGridLayout* conferenceLayout);

  void callingTo(QString callID);
  void incomingCall();

  // if our call is accepted or we accepted their call
  VideoWidget* addParticipant(QString callID);

  void removeCaller(QString callID);

private:

  void hideLabel();

  QWidget *parent_;

  QGridLayout* layout_;

  // dynamic videowidget adding to layout
  int row_;
  int column_;

};
