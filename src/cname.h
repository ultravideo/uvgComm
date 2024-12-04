#pragma once

#include <QString>

class CName
{
public:
  CName();

  static QString cname();

private:

  void generateCName();
  QString getCName() const;

  static std::shared_ptr<CName> instance_;
  QString cname_;
};
