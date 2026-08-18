#ifndef DEPTHWIDGET_H
#define DEPTHWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QElapsedTimer>
#include <QImage>
#include <QPoint>
#include <QTimer>
#include <memory>
#include <vector>

// Depth-parallax viewer: the still is drawn on a grid whose vertices are pushed
// sideways by their depth, so dragging reads as moving the camera.
// Disparity is a vertex attribute rather than a vertex-shader texture fetch, to
// stay within the GLSL 1.20 feature set the panorama viewer targets.
class DepthWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit DepthWidget(QWidget* parent = nullptr);
    ~DepthWidget() override;

    // 'depth' is white-is-near and need not match 'color' in size; a null one
    // renders flat rather than failing.
    void setImage(const QImage& color, const QImage& depth);
    bool hasImage() const { return m_hasImage; }
    void clear();

    // Fraction of image width; past ~0.12 the stretching over disocclusions shows.
    void setAmplitude(float a);
    float amplitude() const { return m_amplitude; }

    // Disparity that stays put, 0 = far, 1 = near. Not called setFocus(): that
    // would shadow QWidget::setFocus().
    void setFocusPlane(float f);
    float focusPlane() const { return m_focus; }

    void setAutoOrbit(bool on);
    bool autoOrbit() const { return m_autoOrbit; }

    void resetView();

protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;

private:
    void uploadPending();
    void buildMesh();
    void applyOffsetFromCursor(const QPoint& pos);

    QOpenGLShaderProgram            m_program;
    QOpenGLBuffer                   m_vbo;
    QOpenGLBuffer                   m_ibo{QOpenGLBuffer::IndexBuffer};
    std::unique_ptr<QOpenGLTexture> m_texture;
    int                             m_indexCount = 0;

    QImage m_pendingColor;         // held until GL is ready
    QImage m_pendingDepth;
    QImage m_disparity;
    bool   m_hasImage = false;
    float  m_aspect   = 1.0f;

    float  m_amplitude = 0.09f;
    float  m_focus     = 0.45f;
    QPointF m_offset;              // -1..1 on each axis
    bool   m_dragging = false;
    QPoint m_pressPos;
    QPointF m_pressOffset;

    bool          m_autoOrbit = true;
    QTimer        m_orbitTimer;
    QElapsedTimer m_clock;
};

#endif // DEPTHWIDGET_H
