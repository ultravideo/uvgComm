#pragma once

#include <QPushButton>
#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>

class ContactListItem : public QWidget
{
public:
  ContactListItem(QString name, QString username, QString ip);

  // construct the widget
  void init();

signals:
  void startCall(QString name, QString username, QString ip);
  void startChat(QString name, QString username, QString ip);

public slots:
  void call();
  void chat();

private:

  QString name_;
  QString username_;
  QString ip_;

  QHBoxLayout* layout_;

  QLabel* nameLabel_;
  QPushButton* callButton_;
  QPushButton* chatButton_;
};
