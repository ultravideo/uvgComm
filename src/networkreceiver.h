#pragma once


#include <QByteArray>
#include <QtNetwork>
#include <QThread>

#include <functional>


/* Receives all the SIP messages */


class NetworkReceiver : public QThread
{
public:
  NetworkReceiver();

  void init(uint16_t port);

  //void run();

  void addIP(QHostAddress address);

  // callback
  template <typename Class>
  void addDataOutCallback (Class* o, void (Class::*method) (QByteArray& data))
  {
    outDataCallback_ = ([o,method] (QByteArray& data)
    {
      return (o->*method)(data);
    });
  }

private:

  QUdpSocket socket_;

  std::function<void(QByteArray& data)> outDataCallback_;

};
