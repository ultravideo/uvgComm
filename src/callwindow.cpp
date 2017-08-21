#include "callwindow.h"
#include "ui_callwindow.h"

#include "ui_callingwidget.h"
#include "ui_about.h"

#include "statisticswindow.h"

#include <QCloseEvent>
#include <QTimer>
#include <QMetaType>

CallWindow::CallWindow(QWidget *parent):
  QMainWindow(parent),
  ui_(new Ui::CallWindow),
  settingsView_(),
  stats_(new StatisticsWindow(this)),
  conference_(this),
  manager_(stats_),
  callNeg_(),
  timer_(new QTimer(this))
{
  ui_->setupUi(this);

  // GUI updates are handled solely by timer
  timer_->setInterval(30);
  timer_->setSingleShot(false);
  connect(timer_, SIGNAL(timeout()), ui_->participants, SLOT(update()));
  connect(timer_, SIGNAL(timeout()), ui_->SelfView, SLOT(update()));
  connect(timer_, SIGNAL(timeout()), stats_, SLOT(update()));
  timer_->start();

  setWindowTitle("Kvazzup");
}

CallWindow::~CallWindow()
{
  timer_->stop();
  delete timer_;
  stats_->close();
  delete stats_;
  delete ui_;
}

void CallWindow::addContact()
{
  contacts_.addContact(this, ui_->peerName->text(), "anonymous", ui_->ip->text());
}

void CallWindow::startStream()
{
  contacts_.initializeList(ui_->contactList, this);

  Ui::CallerWidget *ui_widget = new Ui::CallerWidget;
  QWidget* holderWidget = new QWidget;
  ui_widget->setupUi(holderWidget);
  connect(ui_widget->AcceptButton, SIGNAL(clicked()), this, SLOT(acceptCall()));
  connect(ui_widget->DeclineButton, SIGNAL(clicked()), this, SLOT(rejectCall()));

  aboutWidget_ = new Ui::AboutWidget;
  aboutWidget_->setupUi(&about_);

  // TODO: I don't know why this is required.
  qRegisterMetaType<QVector<int> >("QVector<int>");


  QObject::connect(&callNeg_, SIGNAL(incomingINVITE(QString, QString)),
                   this, SLOT(incomingCall(QString, QString)));

  QObject::connect(&callNeg_, SIGNAL(callingOurselves(QString, std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(callOurselves(QString, std::shared_ptr<SDPMessageInfo>)));

  QObject::connect(&callNeg_, SIGNAL(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>,
                                                              std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>,
                                                      std::shared_ptr<SDPMessageInfo>)));

  QObject::connect(&callNeg_, SIGNAL(ringing(QString)),
                   this, SLOT(ringing(QString)));

  QObject::connect(&callNeg_, SIGNAL(ourCallAccepted(QString, std::shared_ptr<SDPMessageInfo>,
                                                               std::shared_ptr<SDPMessageInfo>)),
                   this, SLOT(callNegotiated(QString, std::shared_ptr<SDPMessageInfo>,
                                                      std::shared_ptr<SDPMessageInfo>)));

  QObject::connect(&callNeg_, SIGNAL(ourCallRejected(QString)),
                   this, SLOT(ourCallRejected(QString)));

  QObject::connect(&callNeg_, SIGNAL(callEnded(QString, QString)),
                   this, SLOT(endCall(QString, QString)));

  QObject::connect(&settingsView_, SIGNAL(settingsChanged()),
                   &manager_, SLOT(recordChangedSettings()));

  conferenceMutex_.lock();
  conference_.init(ui_->participantLayout, ui_->participants, ui_widget, holderWidget);
  conferenceMutex_.unlock();

  // TODO move these closer to useagepoint
  QSettings settings;
  QString localName = settings.value("local/Name").toString();
  QString localUsername = settings.value("local/Username").toString();

  callNeg_.init(localName, localUsername);

  manager_.init(ui_->SelfView);
}

void CallWindow::callToParticipant(QString name, QString username, QString ip)
{
  if(manager_.roomForMoreParticipants())
  {
    QString ip_str = ip;

    Contact con;
    con.contactAddress = ip_str;
    con.name = "Anonymous";
    con.username = "unknown";

    QList<Contact> list;
    list.append(con);

    //start negotiations for this connection

    QList<QString> callIDs = callNeg_.startCall(list);

    for(auto callID : callIDs)
    {
      conferenceMutex_.lock();
      conference_.callingTo(callID, "Contact List Missing!"); // TODO get name from contact list
      conferenceMutex_.unlock();
    }
  }
  else
  {
    qDebug() << "No room for more participants.";
  }
}

void CallWindow::chatWithParticipant(QString name, QString username, QString ip)
{
  qDebug("Chat not implemented yet");
}

void CallWindow::openStatistics()
{
  stats_->show();
}

void CallWindow::closeEvent(QCloseEvent *event)
{
  endAllCalls();

  callNeg_.uninit();
  manager_.uninit();

  stats_->hide();
  stats_->finished(0);

  QMainWindow::closeEvent(event);
}

void CallWindow::setMicState(bool on)
{}

void CallWindow::setCameraState(bool on)
{}

void CallWindow::incomingCall(QString callID, QString caller)
{
  if(!manager_.roomForMoreParticipants())
  {
    qWarning() << "WARNING: Could not fit more participants to this call";
    //rejectCall(); // TODO: send a not possible message instead of reject.
    return;
  }

  conferenceMutex_.lock();
  conference_.incomingCall(callID, caller);
  conferenceMutex_.unlock();
}

void CallWindow::callOurselves(QString callID, std::shared_ptr<SDPMessageInfo> info)
{
  if(manager_.roomForMoreParticipants())
  {
    conferenceMutex_.lock();
    VideoWidget* view = conference_.addVideoStream(callID);
    conferenceMutex_.unlock();

    qDebug() << "Calling ourselves, how boring.";

    manager_.createParticipant(callID, info, info, view, stats_);
  }
}

void CallWindow::acceptCall()
{
  qDebug() << "We accepted";
  conferenceMutex_.lock();
  QString callID = conference_.acceptNewest();
  conferenceMutex_.unlock();
  callNeg_.acceptCall(callID);
}

void CallWindow::rejectCall()
{
  qDebug() << "We rejected";
  conferenceMutex_.lock();
  QString callID = conference_.rejectNewest();
  conferenceMutex_.unlock();
  callNeg_.rejectCall(callID);
}

void CallWindow::ringing(QString callID)
{
  qDebug() << "call is ringing. TODO: display it to user";
}

void CallWindow::ourCallRejected(QString callID)
{
  qDebug() << "Our Call was rejected. TODO: display it to user";
  conferenceMutex_.lock();
  conference_.removeCaller(callID);
  conferenceMutex_.unlock();
}

void CallWindow::callNegotiated(QString callID, std::shared_ptr<SDPMessageInfo> peerInfo,
                                std::shared_ptr<SDPMessageInfo> localInfo)
{
  qDebug() << "Our call has been accepted.";

  qDebug() << "Sending media to IP:" << peerInfo->globalAddress
           << "to port:" << peerInfo->media.first().port;

  conferenceMutex_.lock();
  VideoWidget* view = conference_.addVideoStream(callID);
  conferenceMutex_.unlock();

  // TODO check the SDP info and do ports and rtp numbers properly
  manager_.createParticipant(callID, peerInfo, localInfo, view, stats_);
}

void CallWindow::endCall(QString callID, QString ip)
{
  qDebug() << "Received the end of call message";

  manager_.removeParticipant(callID);

  conferenceMutex_.lock();
  conference_.removeCaller(callID);
  conferenceMutex_.unlock();

  stats_->removeParticipant(ip);
}

void CallWindow::on_settings_clicked()
{
  settingsView_.showBasicSettings();
}

void CallWindow::on_advanced_settings_clicked()
{
  settingsView_.showAdvancedSettings();
}

void CallWindow::on_about_clicked()
{
  about_.show();
}

void CallWindow::endAllCalls()
{
  callNeg_.endAllCalls();
  manager_.endCall();
  conferenceMutex_.lock();
  conference_.close();
  conferenceMutex_.unlock();
}
