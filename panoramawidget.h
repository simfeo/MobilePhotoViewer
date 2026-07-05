#ifndef PANORAMAWIDGET_H
#define PANORAMAWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QMatrix4x4>
#include <QImage>
#include <memory>

// Equirectangular photosphere / panorama viewer: the image is textured onto the
// inside of a sphere; drag rotates the view, wheel / +/- change the field of view.
class PanoramaWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit PanoramaWidget(QWidget* parent = nullptr);
    ~PanoramaWidget() override;

    // 'equirect' must already be a full 360x180 equirectangular image (partial
    // panoramas are composited into the full canvas by the caller).
    void setPanorama(const QImage& equirect);

    void zoomIn();
    void zoomOut();
    void resetView();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    void buildSphere(int sectors, int stacks);

    QOpenGLShaderProgram        m_program;
    QOpenGLBuffer               m_vbo;
    QOpenGLBuffer               m_ibo{QOpenGLBuffer::IndexBuffer};
    std::unique_ptr<QOpenGLTexture> m_texture;
    int                         m_indexCount = 0;

    QImage  m_pending;       // texture to upload once GL is ready
    float   m_yaw   = 0.0f;  // degrees
    float   m_pitch = 0.0f;  // degrees
    float   m_fov   = 75.0f; // degrees, vertical
    QPoint  m_lastPos;
};

#endif // PANORAMAWIDGET_H
