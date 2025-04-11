#pragma once

#include <QString>
#include <QObject>

class Scripting : public QObject
{
  Q_OBJECT
public:
  Scripting();

  void fileScripting(const QString &filename);
  void stdinScripting();

  void start();

private:

};
