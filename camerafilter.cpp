#include "camerafilter.h"

#include "cameraframegrabber.h"

//#include <QCameraViewfinder>
#include <QCameraInfo>

CameraFilter::CameraFilter()
{
    camera = new QCamera(QCameraInfo::defaultCamera());
    cameraFrameGrabber_ = new CameraFrameGrabber();
    camera->setViewfinder(cameraFrameGrabber_);

    connect(cameraFrameGrabber_, SIGNAL(frameAvailable(QImage)), this, SLOT(handleFrame(QImage)));

    camera->start();
}

CameraFilter::~CameraFilter()
{
    delete camera;
    delete cameraFrameGrabber_;
}

void CameraFilter::run()
{
    sleep();
}


void CameraFilter::handleFrame(QImage image)
{
    uchar* bits = new uchar[image.byteCount()];

    memcpy(bits, image.bits(), image.byteCount());

    Q_ASSERT(bits);

    std::unique_ptr<Data> data( new Data(RPG32VIDEO, bits, image.byteCount()));

    putOutput(std::move(data));
}
