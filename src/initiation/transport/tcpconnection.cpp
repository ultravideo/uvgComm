#include "tcpconnection.h"

#include "common.h"
#include "logger.h"

#include <QDataStream>
#include <QtConcurrent/QtConcurrent>

#include <stdint.h>

const uint32_t MAX_READ_BYTES = 10000;

const uint8_t NUMBER_OF_RETRIES = 3;

const uint16_t CONNECTION_TIMEOUT_MS = 200;

const uint16_t DISCONNECT_TIMEOUT_MS = 1000;


TCPConnection::TCPConnection()
  :
    socket_(nullptr),
    shouldConnect_(false),
    destination_(),
    port_(0),
    socketDescriptor_(0),
    buffer_(),
    sendMutex_(),
    active_(false)
{}

TCPConnection::~TCPConnection()
{
  uninit();
}

void TCPConnection::init()
{
  QObject::connect(this, &TCPConnection::error, this, &TCPConnection::printError);
  active_ = true;

  if (socket_ == nullptr)
  {
    socket_ = new QTcpSocket();

    QObject::connect(socket_, &QAbstractSocket::disconnected,
                     this, &TCPConnection::disconnected);
    QObject::connect(socket_, &QAbstractSocket::bytesWritten, this, &TCPConnection::printBytesWritten);
    QObject::connect(socket_, &QAbstractSocket::readyRead, this, &TCPConnection::receivedMessage);
  }
}

void TCPConnection::uninit()
{
  active_ = false;

  if(socket_ != nullptr)
  {
    QObject::disconnect(socket_, &QAbstractSocket::disconnected,
                        this, &TCPConnection::disconnected);

    QObject::disconnect(socket_, &QAbstractSocket::bytesWritten,
                        this, &TCPConnection::printBytesWritten);

    QObject::disconnect(socket_, &QAbstractSocket::readyRead,
                        this, &TCPConnection::receivedMessage);
    delete socket_;
    socket_ = nullptr;
  }
}


bool TCPConnection::isConnected() const
{
  return socket_ != nullptr &&
      socket_->state() == QAbstractSocket::ConnectedState;
}


QString TCPConnection::localAddress() const
{
  if (isConnected())
  {
    return socket_->localAddress().toString();
  }
  return "";
}


QString TCPConnection::remoteAddress() const
{
  if (isConnected())
  {
    return socket_->peerAddress().toString();
  }
  return "";
}


uint16_t TCPConnection::localPort() const
{
  if (isConnected())
  {
    return socket_->localPort();
  }
  return 0;
}

uint16_t TCPConnection::remotePort() const
{
  if (isConnected())
  {
    return socket_->peerPort();
  }
  return 0;
}


QAbstractSocket::NetworkLayerProtocol TCPConnection::localProtocol() const
{
  if (isConnected())
  {
    return socket_->localAddress().protocol();
  }

  return QAbstractSocket::NetworkLayerProtocol::AnyIPProtocol;
}


QAbstractSocket::NetworkLayerProtocol TCPConnection::remoteProtocol() const
{
  if (isConnected())
  {
    return socket_->peerAddress().protocol();
  }

  return QAbstractSocket::NetworkLayerProtocol::AnyIPProtocol;
}


void TCPConnection::stopConnection()
{
  active_ = false;
  eventDispatcher()->interrupt();
}


void TCPConnection::establishConnection(const QString &destination, uint16_t port)
{
  Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Establishing connection",
                                  {"Destination", "port"}, {destination, QString::number(port)});

  destination_ = destination;
  port_ = port;
  shouldConnect_ = true;
  start();
}


void TCPConnection::setExistingConnection(qintptr socketDescriptor)
{
  Logger::getLogger()->printNormal(this, "Setting existing/incoming connection.",
                                   {"Sock desc"}, {QString::number(socketDescriptor)});

  socketDescriptor_ = socketDescriptor;
  shouldConnect_ = true;
  start();
}


void TCPConnection::sendPacket(const QString &data)
{
  //Logger::getLogger()->printNormal(this, "Adding to send buffer");

  if(active_)
  {
    sendMutex_.lock();
    buffer_.push(data);
    sendMutex_.unlock();

    eventDispatcher()->wakeUp();
  }
  else
  {
    Logger::getLogger()->printWarning(this, "Not sending message, "
                                            "because sender has been shut down.");
  }
}


void TCPConnection::receivedMessage()
{
  //Logger::getLogger()->printNormal(this, "Socket ready to read.");
  if (active_ )
  {
    eventDispatcher()->wakeUp();
  }
  else {
    Logger::getLogger()->printWarning(this, "Socket not active when receiving message");
  }
}


bool TCPConnection::connectLoop()
{
  Logger::getLogger()->printNormal(this, "Starting to connect TCP");

  if (socket_ == nullptr)
  {
    Logger::getLogger()->printProgramWarning(this, "Socket not initialized before connection");
    init();
  }

  if (socketDescriptor_ != 0)
  {
    Logger::getLogger()->printNormal(this, "Setting existing socket descriptor",
        {"Sock desc"}, {QString::number(socketDescriptor_)});

    if (!socket_->setSocketDescriptor(socketDescriptor_))
    {
      Logger::getLogger()->printProgramError(this, "Could not set socket descriptor "
                                                   "for existing connection.");
      return false;
    }
  }
  else
  {
    Logger::getLogger()->printNormal(this, "Checking status",
                {"Address"}, {destination_ + ":" + QString::number(port_)});

    for (unsigned int i = 0; i < NUMBER_OF_RETRIES &&
         socket_->state() != QAbstractSocket::ConnectedState; ++i)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Attempting to connect",
                {"Attempt"}, {QString::number(i + 1)});

      socket_->connectToHost(destination_, port_);
      // attempt connection with increasing wait time
      socket_->waitForConnected(CONNECTION_TIMEOUT_MS * (i + 1));
    }
  }

  if (socket_->state() != QAbstractSocket::ConnectedState)
  {
    emit error(socket_->error(), socket_->errorString());
    emit unableToConnect(destination_);
    return false;
  }

  Logger::getLogger()->printNormal( this, "Connected succesfully", {"Connection"},
              {socket_->localAddress().toString() + ":" + QString::number(socket_->localPort()) + " <-> " +
               socket_->peerAddress().toString() + ":" + QString::number(socket_->peerPort())});


  emit socketConnected(socket_->localAddress().toString(), socket_->peerAddress().toString());
  return true;
}


void TCPConnection::run()
{
  Logger::getLogger()->printImportant(this, "Starting TCP loop");

  init();

  if(eventDispatcher() == nullptr)
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, this, "No event dispatcher for this connection.");
    return;
  }
  while(active_)
  {
    // TODO: Stop trying if connection can't be established.
    if(!isConnected() && shouldConnect_)
    {
      if (!connectLoop())
      {
        Logger::getLogger()->printWarning(this, "Failed to connect TCP connection");
        shouldConnect_ = false;
      }
    }

    if(socket_->bytesToWrite() > 0)
    {
      Logger::getLogger()->printDebug(DEBUG_NORMAL, this, "Detected need to send.", {"Bytes", "State"},
                  {QString::number(socket_->bytesToWrite()), QString::number(socket_->state())});
    }

    if(socket_->state() == QAbstractSocket::ConnectedState)
    {
      if(socket_->isValid() && socket_->canReadLine() && socket_->bytesAvailable() < MAX_READ_BYTES)
      {
        Logger::getLogger()->printNormal(this, "Can read one line", {"Bytes available"},
                    {QString::number(socket_->bytesAvailable())});

        // TODO: This should probably be some other stream, because we get also non text stuff in content?

        QTextStream in(socket_);
        QString message;
        message = in.read(MAX_READ_BYTES);

        emit messageAvailable(message);
      }
      else if(socket_->bytesAvailable() > MAX_READ_BYTES)
      {
        Logger::getLogger()->printDebug(DEBUG_WARNING, this, 
                                        "Flushing the socket because of too much data!");
        socket_->flush();
      }

      sendMutex_.lock();
      while(buffer_.size() > 0 && socket_->state() == QAbstractSocket::ConnectedState)
      {
        bufferToSocket();
      }
      sendMutex_.unlock();
    }

    eventDispatcher()->processEvents(QEventLoop::WaitForMoreEvents);
  }

  // send all remaining messages in buffer
  while(buffer_.size() > 0 && socket_->state() == QAbstractSocket::ConnectedState)
  {
    bufferToSocket();
  }

  eventDispatcher()->processEvents(QEventLoop::ExcludeUserInputEvents);

  Logger::getLogger()->printNormal(this, "Disconnecting TCP connection");
  disconnect();
  uninit();

  Logger::getLogger()->printImportant(this, "Ended TCP loop");
}


void TCPConnection::bufferToSocket()
{
  Logger::getLogger()->printNormal(this, "Writing buffer to TCP socket",
              {"Buffer size"}, {QString::number(buffer_.size())});

  if(buffer_.size() > MAX_READ_BYTES)
  {
    Logger::getLogger()->printDebug(DEBUG_WARNING, this, 
                                    "We are sending too much stuff to the other end",
                                    {"Buffer size"}, {QString::number(buffer_.size())});
  }

  QString message = buffer_.front();
  buffer_.pop();

  // For some reason write stream has to be created every write, otherwise
  // a mysterious crash appears.
  QTextStream stream (socket_);
  stream << message;
}


void TCPConnection::disconnect()
{
  active_ = false;
  shouldConnect_ = false;
  if (socket_)
  {
    socket_->disconnectFromHost();
    if(socket_->state() == QAbstractSocket::UnconnectedState ||
       socket_->waitForDisconnected(DISCONNECT_TIMEOUT_MS))
    {
      Logger::getLogger()->printNormal(this, "TCP disconnected");
    }
    else
    {
      emit error(socket_->error(), socket_->errorString());
      return;
    }
  }
}


void TCPConnection::disconnected()
{
  Logger::getLogger()->printWarning(this, "TCP socket disconnected");
}


void TCPConnection::printError(int socketError, const QString &message)
{
  Logger::getLogger()->printDebug(DEBUG_ERROR, this, "Socket Error",
             {"Code", "Message"}, {QString::number(socketError), message});
}


void TCPConnection::printBytesWritten(qint64 bytes)
{
  Logger::getLogger()->printNormal(this, "Written to socket", {"Bytes"}, 
                                   {QString::number(bytes)});
}
