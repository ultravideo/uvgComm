#include "videoyuvwidget.h"

#include "statisticsinterface.h"
#include "logger.h"

#include <QPaintEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLayout>

VideoYUVWidget::VideoYUVWidget(QWidget* parent, uint32_t sessionID,
                               uint32_t index, uint8_t borderSize)
  : QOpenGLWidget(parent),
  stats_(nullptr),
  sessionID_(sessionID),
  helper_(sessionID, index, borderSize),
  texture_(0),
  prog_(nullptr)

{
  helper_.initWidget(this);

  // the new syntax does not work for some reason (unresolved overloaded function type)
  QObject::connect(this, SIGNAL(newImage()), this, SLOT(repaint()));
  QObject::connect(&helper_, &VideoDrawHelper::detach, this, &VideoYUVWidget::detach);
  QObject::connect(&helper_, &VideoDrawHelper::reattach, this, &VideoYUVWidget::reattach);
}


VideoYUVWidget::~VideoYUVWidget()
{}


void VideoYUVWidget::enableOverlay(int roiQP, int backgroundQP)
{
  helper_.enableOverlay(roiQP, backgroundQP);
}


void VideoYUVWidget::resetOverlay()
{
  helper_.resetOverlay();
}


std::unique_ptr<int8_t[]> VideoYUVWidget::getRoiMask(int& width, int& height, int qp, bool scaleToInput)
{
  return helper_.getRoiMask(width, height, qp, scaleToInput);
}


void VideoYUVWidget::inputImage(std::unique_ptr<uchar[]> data, QImage &image, int64_t timestamp)
{
  Q_ASSERT(data != nullptr);
  drawMutex_.lock();
  // if the resolution has changed in video

  helper_.inputImage(this, std::move(data), image, timestamp);

  //update();

  emit newImage();
  drawMutex_.unlock();
}

/*
static const char *fragmentShaderSource =
    "uniform sampler2D s_texture;\n"
    "void main() {\n"
        "float y = texture2D(s_texture, gl_FragCoord.xy).r;\n"
        "float u = texture2D(s_texture, gl_FragCoord.xy).r - 0.5;\n"
        "float v = texture2D(s_texture, gl_FragCoord.xy).r - 0.5;\n"
        "float r = y +             1.402 * v;\n"
        "float g = y - 0.344 * u - 0.714 * v;\n"
        "float b = y + 1.772 * u;\n"
       // "gl_FragColor = vec4(r,g,b,1.0);\n"
        "gl_FragColor = vec4(1.0,0.0,0.0,1.0);\n"
    "}\n";
*/

/*
static const char *vertexShaderSource =
    "attribute vec4 vertex;\n"
    "void main() {\n"
    "   gl_Position = vertex;\n"
    "}\n";
*/

static const char *vertexShaderSource2 =
    "attribute vec4 posAttr;\n"
    "attribute lowp vec4 colAttr;\n"
    "varying lowp vec4 col;\n"
    "uniform mat4 matrix;\n"
    "void main() {\n"
    "   col = colAttr;\n"
    "   gl_Position = matrix * posAttr;\n"
    "}\n";

static const char *fragmentShaderSource2 =
    "varying lowp vec4 col;\n"
    "void main() {\n"
    "   gl_FragColor = col;\n"
    "}\n";


void VideoYUVWidget::initializeGL()
{
  initializeOpenGLFunctions();

  if(prog_ == nullptr)
  {
    //makeCurrent();
    qDebug() << "Initializing OpenGLProgram:" << glGetError();
    prog_ = new QOpenGLShaderProgram();
    prog_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource2);
    prog_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource2);
    prog_->link();
     qDebug() << "Opengl initialization linking:" << glGetError() << prog_->log();
    m_posAttr = prog_->attributeLocation("posAttr");
    qDebug() << "Opengl initialization:" << glGetError();
    m_colAttr = prog_->attributeLocation("colAttr");
    qDebug() << "Opengl initialization:" << glGetError();
    m_matrixUniform = prog_->uniformLocation("matrix");
    qDebug() << "Opengl initialization:" << glGetError();

    m_frame = 0;
    //doneCurrent();
    qDebug() << "Opengl initialization:" << glGetError();
  }

  /*
  qDebug() << "=== Initializing YUV OpenGL drawing widget for sessionID:" << sessionID_;


  qDebug() << "Opengl:" << glGetError();

  if(!prog_.isLinked())
  {
    //static const GLfloat texCoords[] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f };
    static const GLfloat vertices[]= {1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f,  -1.0f, -1.0f };

    vbo_.create();
    vbo_.bind();
    vbo_.allocate(vertices, 8*sizeof(GLfloat));

    prog_.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    qDebug() << "Opengl:" << glGetError();

    prog_.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    qDebug() << "Opengl source:" << glGetError();

    prog_.bindAttributeLocation("vertex", 0);
    qDebug() << "Opengl attribute:" << glGetError();

    prog_.link();
    qDebug() << "Opengl Link:" << glGetError();
  }

  if(texture_ == 0)
  {
    prog_.bind();
    qDebug() << "Opengl bind:" << glGetError();

    glEnable(GL_TEXTURE_2D);
    qDebug() << "Opengl:" << glGetError();

    glGenTextures(1, &texture_);
    qDebug() << "Opengl:" << glGetError();

    glBindTexture(GL_TEXTURE_2D, texture_);
    qDebug() << "Opengl:" << glGetError();

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    qDebug() << "Opengl:" << glGetError();

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    qDebug() << "Opengl:" << glGetError();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1600, 1344, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, lastImage_.size().width(), lastImage_.size().height(), 0,
    //              GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    //prog_.bindAttributeLocation("vertex", 0);
    int loc = prog_.uniformLocation("s_texture");
    prog_.setUniformValue(loc, texture_);

    prog_.release();
  }

  //glOrtho(-1.0, 1.0, -1.0, 1.0, 0.1, 50);

  qDebug() << "Opengl Initialization:" << glGetError() << ":" << prog_.log();
  qDebug() << "Image size:" << lastImage_.size();
  */
}
void VideoYUVWidget::drawOpenGL(bool updateImage)
{
  Q_UNUSED(updateImage);
  /*
  qDebug() << "Drawing with OpenGL --------------------------------------------------";
  glClear(GL_COLOR_BUFFER_BIT);
  glBindTexture(GL_TEXTURE_2D, texture_);

  if(updateImage)
  {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 400, 1344,
                   GL_BGRA,  GL_UNSIGNED_BYTE, dataBuffer_.back().get());
  }
  prog_.bind();
*/
  //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, lastImage_.size().width(), lastImage_.size().height(),
  //                 GL_RGB, GL_UNSIGNED_BYTE, dataBuffer_.back().get());

  //prog_.enableAttributeArray(0);
  //prog_.setAttributeBuffer(0, GL_FLOAT, 0, 2, 2*sizeof(GLfloat));

  //glBindBuffer();
  //glEnableVertexAttribArray();
  //glVertexAttribPointer();
  //glDrawArrays(GL_TRIANGLES, 0, 3);

  qDebug() << "Drawing with OpenGL shaders";

  if(prog_ == nullptr)
  {
    initializeGL();
  }

 // makeCurrent();

  const qreal retinaScale = devicePixelRatio();
  glViewport(0, 0, width() * retinaScale, height() * retinaScale);
  qDebug() << "Opengl:" << glGetError();

  glClear(GL_COLOR_BUFFER_BIT);
  qDebug() << "Opengl:" << glGetError();

  prog_->bind();
  qDebug() << "Opengl bind:" << glGetError();

  QMatrix4x4 matrix;
  matrix.perspective(60.0f, 4.0f/3.0f, 0.1f, 100.0f);
  matrix.translate(0, 0, -2);
  matrix.rotate(100.0f * m_frame / 30, 0, 1, 0);

  prog_->setUniformValue(m_matrixUniform, matrix);
  qDebug() << "Opengl:" << glGetError() << prog_->log();

  GLfloat vertices[] = {
      0.0f, 0.707f,
      -0.5f, -0.5f,
      0.5f, -0.5f
  };

  GLfloat colors[] = {
      1.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 1.0f
  };

  glVertexAttribPointer(m_posAttr, 2, GL_FLOAT, GL_FALSE, 0, vertices);
  qDebug() << "Opengl:" << glGetError();
  glVertexAttribPointer(m_colAttr, 3, GL_FLOAT, GL_FALSE, 0, colors);
  qDebug() << "Opengl:" << glGetError();

  glEnableVertexAttribArray(0);
  qDebug() << "Opengl:" << glGetError();
  glEnableVertexAttribArray(1);
  qDebug() << "Opengl:" << glGetError();

  glDrawArrays(GL_TRIANGLES, 0, 3);
  qDebug() << "Opengl draw:" << glGetError();

  glDisableVertexAttribArray(1);
  qDebug() << "Opengl:" << glGetError();
  glDisableVertexAttribArray(0);
  qDebug() << "Opengl:" << glGetError();

  prog_->release();

  ++m_frame;

  //doneCurrent();
  qDebug() << "Opengl done:" << glGetError();
}

void VideoYUVWidget::paintGL()
{
  //qDebug() << "PaintEvent for widget:" << sessionID_;
  QPainter painter(this);

  if(helper_.readyToDraw())
  {
    drawMutex_.lock();

    QImage frame;
    if(helper_.getRecentImage(frame))
    {
      // sessionID 0 is the self display and we are not interested
      // update stats only for each new image.
      if(stats_ && sessionID_ != 0)
      {
        stats_->presentPackage(sessionID_, "Video");
      }
    }

    drawOpenGL(true);
    drawMutex_.unlock();
  }
  else
  {
    drawOpenGL(false);
  }
}

void VideoYUVWidget::resizeGL(int width, int height)
{
  Q_UNUSED(width);
  Q_UNUSED(height);

  //int side = qMin(width, height);
  //glViewport((width - side) / 2, (height - side) / 2, side, side);
}

void VideoYUVWidget::resizeEvent(QResizeEvent *event)
{
  QOpenGLWidget::resizeEvent(event); // its important to call this resize function, not the qwidget one.
}


void VideoYUVWidget::keyPressEvent(QKeyEvent *event)
{
  helper_.keyPressEvent(this, event);
}


void VideoYUVWidget::mouseDoubleClickEvent(QMouseEvent *e) {
  QWidget::mouseDoubleClickEvent(e);
  helper_.mouseDoubleClickEvent(this);
}
