#include "filter.h"

Filter::Filter()
{

}

void Filter::addOutconnection(Filter *out)
{
    outConnections_.push_back(out);
}

void Filter::emptyBuffer()
{
    mutex_.lock();
    std::queue<std::unique_ptr<Data>> empty;
    std::swap( inBuffer_, empty );
    mutex_.unlock();
}

void Filter::putInput(std::unique_ptr<Data> data)
{
    mutex_.lock();
    inBuffer_.push(std::move(data));
    hasInput_.wakeOne();
    mutex_.unlock();
}

std::unique_ptr<Data> Filter::getInput()
{
    mutex_.lock();
    std::unique_ptr<Data> r;
    if(!inBuffer_.empty())
    {
        std::unique_ptr<Data> r = std::move(inBuffer_.front());
        inBuffer_.pop();
    }
    mutex_.unlock();
    return r;
}

void Filter::putOutput(std::unique_ptr<Data> data)
{
    for(Filter *f : outConnections_)
    {
        f->putInput(std::move(data));
    }
}
