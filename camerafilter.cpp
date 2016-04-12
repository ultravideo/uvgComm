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
    while(running_)
    {
        sleep();

        if(!running_) break;
    }
}

void CameraFilter::handleFrame(QImage image)
{
    Data * newImage = new Data;

    newImage->type = RPG32VIDEO;
    std::unique_ptr<uchar> uu(new uchar[image.byteCount()]);
    newImage->data = std::unique_ptr<uchar[]>(new uchar[image.byteCount()]);

    uchar *bits = image.bits();

    memcpy(newImage->data.get(), bits, image.byteCount());
    newImage->data_size = image.byteCount();
    newImage->width = image.width();
    newImage->height = image.height();

    std::unique_ptr<Data> u_newImage( newImage );

    Q_ASSERT(u_newImage->data);

    putOutput(std::move(u_newImage));
}
