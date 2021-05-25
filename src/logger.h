#pragma once


#include <QObject>
#include <QString>
#include <QMutex>
#include <QFile>

#include <memory>

enum DebugType{DEBUG_NORMAL, DEBUG_IMPORTANT, DEBUG_ERROR, DEBUG_WARNING,
               DEBUG_PEER_ERROR, DEBUG_PROGRAM_ERROR, DEBUG_PROGRAM_WARNING};

// A singleton class. Used to uniformalize debug prints accross Kvazzup

class Logger
{
public:

  // initializes the Logger instance if it has not been initialized
  static std::shared_ptr<Logger> getLogger();

  // One time informational debug printing.
  void printNormal(const QObject* object, QString description = "",
                   QString valueName = "", QString value = "");
  void printNormal(const QString module, QString description = "",
                   QString valueName = "", QString value = "");

  // Important one time events
  void printImportant(const QObject* object, QString description = "",
                      QString valueName = "", QString value = "");
  void printImportant(const QString module, QString description = "",
                      QString valueName = "", QString value = "");

  // Events outside the program that can be dealt with, but are unusual
  // and/or can cause problems in the future
  void printWarning(const QObject* object, QString description = "",
                    QString valueName = "", QString value = "");
  void printWarning(const QString module, QString description = "",
                    QString valueName = "", QString value = "");

  // Events outside the program that will cause problems
  // for the functionality of the Kvazzup.
  void printError(const QObject* object, QString description = "",
                  QString valueName = "", QString value = "");
  void printError(const QString module, QString description = "",
                  QString valueName = "", QString value = "");

  // Impossible situation that will cause problems to functionality of Kvazzup.
  void printProgramError(const QObject* object, QString description = "",
                        QString valueName = "", QString value = "");
  void printProgramError(const QString module, QString description = "",
                        QString valueName = "", QString value = "");

  // Impossible situation that can be dealt without causing problems to functionality
  // of Kvazzup.
  void printProgramWarning(const QObject* object, QString description = "",
                           QString valueName = "", QString value = "");
  void printProgramWarning(const QString module, QString description = "",
                           QString valueName = "", QString value = "");

  // Errors in behavior of entities that we think are caused by other entities.
  // Most commonly a protocol.
  void printPeerError(const QObject* object, QString description = "",
                      QString valueName = "", QString value = "");
  void printPeerError(const QString module, QString description = "",
                      QString valueName = "", QString value = "");

  // Print when something is unimplemented. Does not replace TODO comments
  void printUnimplemented(const QObject* object, QString whatIsNotImplemented);
  void printUnimplemented(const QString module, QString whatIsNotImplemented);



  bool checkError(QObject* object, bool check, DebugType type = DEBUG_ERROR,
                  QString description = "", QStringList values = {});


  // Print debug information with custom class name. Use this and getname with filters.
  // context is a general context that makes it easier to link different prints to one another.
  // TODO: Fix different threads printing at the same time.
  // TODO: Make into a template
  void printDebug(DebugType type, QString className, QString description = "",
                  QStringList valueNames = {}, QStringList values = {});


  // use this if printing is inside class derived from QObject which is most classes in Kvazzup
  void printDebug(DebugType type, const QObject* object, QString description = "",
                  QStringList valueNames = {}, QStringList values = {});


private:
  Logger();

  void printHelper(QString color, QString beginString, QString valueString, QString description, int valuenames);

  static std::shared_ptr<Logger> instance_;

  // global variable until this becomes a static class
  QMutex printMutex_;
};
