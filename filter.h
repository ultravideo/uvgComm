#pragma once

#include <QWaitCondition>
#include <QThread>
#include <QMutex>

#include <cstdint>
#include <vector>
#include <queue>
#include <memory>

enum DataType {RAWVIDEO = 0, RAWAUDIO, HEVCVIDEO, OPUSAUDIO};

struct Data
{
    uint8_t type;
    uint8_t *data;
    uint32_t data_size;
};

class Filter : public QThread
{
public:
    Filter();

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

private:

    QWaitCondition hasInput_;
    bool running_;

    std::vector<Filter*> outConnections_;

    QMutex mutex_;
    std::queue<std::unique_ptr<Data>> inBuffer_;
};
