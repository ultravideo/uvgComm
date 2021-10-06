#pragma once

/* The purpose of this class is the enable filters to easily query the
 * state of hardware in terms of possible optimizations and performance. */

class HWResourceManager
{
public:
  HWResourceManager();

  bool isAVX2Enabled();
  bool isSSE41Enabled();

private:

  void setOptimizations();

  bool avx2_ = false;
  bool sse41_ = false;

};
