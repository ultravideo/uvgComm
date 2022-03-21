#pragma once

#include <QObject>

/* The purpose of this class is the enable filters to easily query the
 * state of hardware in terms of possible optimizations and performance. */

class ResourceAllocator : public QObject
{
  Q_OBJECT
public:
  ResourceAllocator();

  bool isAVX2Enabled();
  bool isSSE41Enabled();

  void updateSettings();

private:

  bool avx2_ = false;
  bool sse41_ = false;

};
