#pragma once

#include <stdint.h>


class QWidget;
class VideoInterface;

class VideoviewFactory
{
public:
  VideoviewFactory();

  QWidget* getView(uint32_t sessionID);

  VideoInterface* getVideo(uint32_t sessionID);

private:


};
