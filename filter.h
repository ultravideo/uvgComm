#pragma once


#include <cstdint>
#include <vector>
#include <memory>

enum DataType {RAWVIDEO = 0, RAWAUDIO, HEVCVIDEO, OPUSAUDIO};

struct Data
{
    uint8_t type;
    uint8_t *data;
    uint32_t data_size;
};

class Filter
{
public:
    Filter();


    void initialize(unsigned int inputs, unsigned int outputs);

    void empty();
    virtual void process() = 0;

    // for checking the correct place in filter graph
    bool isInputFilter() const
    {
        return inBuffer_.size() > 0 && outBuffer_.size() == 0;
    }
    bool isOutputFilter() const
    {
        return outBuffer_.size() > 0 && inBuffer_.size() == 0;
    }

protected:

    void getInput();
    void putOutput();

    // for checking that this filter makes sense during Initialization
    virtual bool canHaveInputs() const = 0;
    virtual bool canHaveOutputs() const = 0;

private:

    std::vector<std::unique_ptr<Data> > inBuffer_;
    std::vector<std::unique_ptr<Data> > outBuffer_;
};
