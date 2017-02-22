#include "sipparser.h"

#include <iostream>
#include <sstream>





void checkSIPLegality();


std::unique_ptr<SIPMessageInfo> parseSIPMessage(QString& header)
{
  SIPMessageInfo* info = new SIPMessageInfo;

  std::istringstream sip_string(header.toStdString());
  QString line = "";

  /*
  while(std::getline(sip_string, line))
  {
    std::istringstream sip_line(line.toStdString());
    QString word = "";

    while(sip_line >> word)
    {
      QDebug() << word;
    }
  }
  */
  return 0;
}
