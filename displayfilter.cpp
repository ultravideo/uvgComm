#include "displayfilter.h"


#include <QtDebug>

DisplayFilter::DisplayFilter()
{

}

void DisplayFilter::process()
{
  std::unique_ptr<Data> input = getInput();
  while(input)
  {
      qDebug() << "Displaying frame";
      input = getInput();
  }

}
