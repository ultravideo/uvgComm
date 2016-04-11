#pragma once

#include <QWaitCondition>
#include <QThread>
#include <QMutex>

#include <cstdint>
#include <vector>
#include <queue>
#include <memory>

enum DataType {RPG32VIDEO = 0, RAWAUDIO, HEVCVIDEO, OPUSAUDIO};

struct Data
{
    uint8_t type;
    std::unique_ptr<uchar> data;
    uint32_t data_size;

    Data(uint8_t t, uchar* d, uint32_t ds):
    type(t), data(std::unique_ptr<uchar>(d)), data_size(ds){}
};

class Filter : public QThread
{

    Q_OBJECT

public:
    Filter();
    virtual ~Filter();

    // adds one outbound connection to this filter.
    void addOutconnection(Filter *out);

    // empties the input buffer
    void emptyBuffer();

    void putInput(std::unique_ptr<Data> data);

    // for debug information only
    virtual bool canHaveInputs() const = 0;
    virtual bool canHaveOutputs() const = 0;

protected:

    // return: oldest element in buffer, empty if none found
    std::unique_ptr<Data> getInput();
    void putOutput(std::unique_ptr<Data> data);

    // QThread function that runs the processing itself
    void run() = 0;

    void sleep()
    {
        waitMutex_->lock();
        hasInput_.wait(waitMutex_);
        waitMutex_->unlock();
    }

private:
    QMutex *waitMutex_;
    QWaitCondition hasInput_;

    bool running_;

    std::vector<Filter*> outConnections_;

    QMutex bufferMutex_;
    std::queue<std::unique_ptr<Data>> inBuffer_;
};
