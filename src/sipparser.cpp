#include "sipparser.h"

#include <iostream>
#include <sstream>

#include <QList>

struct HeaderLine
{
  QString name;
  quint8 words;
};

// remember to update tableToInfo if adding new header fields!
const QList<HeaderLine> HEADERLINES(
{
      {"Via:", 3},
      {"Max-Forwards:", 2},
      {"To:", 3},
      {"From:", 3},
      {"Call-ID:", 2},
      {"Cseq:", 3},
      {"Contact:", 2},
      {"Content-Type:", 2},
      {"Content-Length:", 2},
}
);

// TODO this changes depending on the message type, maybe make a function/table for that
const uint16_t MANDATORYLINES = 9;
const uint16_t FIRSTLINEWORDS = 3;


bool parseMethodName(SIPMessageInfo* info, QString line);

void messageToTable(QStringList& lines, QList<QStringList> &values);
void tableToInfo(QList<QStringList>& values, SIPMessageInfo* info);

bool checkSIPMessage(QList<QStringList>& values);

void cleanup(SIPMessageInfo* info)
{
  if(info != 0)
    delete info;
}


// The basi logic goes as follow:
// 1. Parse first line with method name
// 2. then sort the header-fields to an array
// 3. then set info fields from sorted array where everything is in place
std::unique_ptr<SIPMessageInfo> parseSIPMessage(QString& header)
{
  SIPMessageInfo* info = new SIPMessageInfo;

  QStringList lines = header.split("\r\n");

  qDebug() << "SIP message split into"  << lines.length() << "lines";

  QList<QStringList> values;


  if(!parseMethodName(info, lines.at(0)))
  {
     cleanup(info);
     return NULL;
  }

  messageToTable(lines, values);
  if(!checkSIPMessage(values))
    return NULL;
  tableToInfo(values, info);

  return std::unique_ptr<SIPMessageInfo> (info);
}

bool parseMethodName(SIPMessageInfo* info, QString line)
{
  qDebug() << "Parsing method name";
  QStringList words = line.split(" ");

  if(words.length() != FIRSTLINEWORDS)
  {
    qDebug() << "First line had wrong number of words:"<< words.length()
             << "Expected:" << FIRSTLINEWORDS;
    qDebug() << "Tried to parse following line:" << line;
    return false;
  }

  if(words.at(0) == "INVITE")
  {
    info->request = INVITE;
    qDebug() << "INVITE found";
  }
  else
  {
    qDebug() << "Unrecognized Method:" << words.at(0);
  }

  return true;
}

void messageToTable(QStringList& lines, QList<QStringList> &values)
{
  qDebug() << "Sorting sip message to table for lookup";

  for(unsigned int i = 1;  i < lines.length(); ++i)
  {
    qDebug() << "Line" << i + 1 << "contents:" << lines.at(i);
    QStringList words = lines.at(i).split(" ");

    for(auto field : HEADERLINES)
    {
      if(QString::compare(words.at(0), field.name, Qt::CaseInsensitive) == 0)
      {
        // increase the list size to
        while(values.size() < i)
        {
          values.append(QStringList(""));
        }

        values[i - 1] = words;
      }
    }
  }
}

void tableToInfo(QList<QStringList>& values, SIPMessageInfo* info)
{
  qDebug() << "";
}

bool checkSIPMessage(QList<QStringList>& values)
{
  qDebug() << "Checking SIP message";

  if(values.size() < MANDATORYLINES)
  {
    qDebug() << "Found SIP message with too few lines:" << values.size() << "lines";
    return false;
  }

  for(unsigned int i = 0; i < MANDATORYLINES; ++i)
  {
    if(values[i].length() != HEADERLINES.at(i).words)
    {
      qDebug() << "Wrong number of words in a line for:" << HEADERLINES.at(i).name
               << "(" << i << ")"
               <<". Found:" << values[i].length()
               << "Expecting:" << HEADERLINES.at(i).words;
      qDebug() << "Line:" << values[i];
      return false;
    }

  }
}
