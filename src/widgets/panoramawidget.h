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

// Panorama viewer. An equirectangular image is textured onto the inside of a
// sphere; a cylindrical one onto the inside of a cylinder arc. Drag rotates the
// view, wheel / +/- change the field of view.
class PanoramaWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit PanoramaWidget(QWidget* parent = nullptr);
    ~PanoramaWidget() override;

    enum class Projection
    {
        Equirectangular,
        Cylindrical
    };

    // Equirectangular: 'image' must be a full 360x180 canvas (the caller composites
    // partial panoramas into it). Cylindrical: 'image' is the crop itself, drawn on
    // an arc 'cylFovRad' wide and 'cylHalfHeight' tall (in unit-radius cylinder
    // coordinates) that faces the viewer.
    void setPanorama(const QImage& image,
                     Projection    proj          = Projection::Equirectangular,
                     float         cylHalfHeight = 0.5f,
                     float         cylFovRad     = 6.2831853f);

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
    void buildGeometry();   // (re)builds the sphere or cylinder mesh for m_projection

    QOpenGLShaderProgram            m_program;
    QOpenGLBuffer                   m_vbo;
    QOpenGLBuffer                   m_ibo{ QOpenGLBuffer::IndexBuffer };
    std::unique_ptr<QOpenGLTexture> m_texture;
    int                             m_indexCount = 0;

    QImage     m_pending;   // texture to upload once GL is ready
    Projection m_projection = Projection::Equirectangular;
    float      m_cylHalfH   = 0.5f;         // cylinder half-height (unit radius)
    float      m_cylFov     = 6.2831853f;   // cylinder arc width, radians
    float      m_yaw        = 0.0f;         // degrees
    float      m_pitch      = 0.0f;         // degrees
    float      m_fov        = 75.0f;        // degrees, vertical
    QPoint     m_lastPos;
};

#endif   // PANORAMAWIDGET_H
