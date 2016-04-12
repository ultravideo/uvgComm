#include "filter.h"

Filter::Filter():running_(true), waitMutex_(NULL)
{
    waitMutex_ = new QMutex;
}


Filter::~Filter()
{
    delete waitMutex_;
}

void Filter::addOutconnection(Filter *out)
{
    outConnections_.push_back(out);
}

void Filter::emptyBuffer()
{
    bufferMutex_.lock();
    std::queue<std::unique_ptr<Data>> empty;
    std::swap( inBuffer_, empty );
    bufferMutex_.unlock();
}

void Filter::putInput(std::unique_ptr<Data> data)
{
    Q_ASSERT(data);

    bufferMutex_.lock();
    inBuffer_.push(std::move(data));
    hasInput_.wakeOne();
    bufferMutex_.unlock();
}



std::unique_ptr<Data> Filter::getInput()
{
    bufferMutex_.lock();
    std::unique_ptr<Data> r;
    if(!inBuffer_.empty())
    {
        std::unique_ptr<Data> r = std::move(inBuffer_.front());
        inBuffer_.pop();
    }
    bufferMutex_.unlock();
    return r;
}

void Filter::putOutput(std::unique_ptr<Data> data)
{
    Q_ASSERT(data);

    for(Filter *f : outConnections_)
    {
        f->putInput(std::move(data));
    }
}


void Filter::stop()
{
    running_ = false;
    hasInput_.wakeAll();
}

