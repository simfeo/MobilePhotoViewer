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

// Depth-parallax viewer. The still is drawn on a dense grid whose vertices are
// pushed sideways in proportion to their depth, so dragging shifts the near and
// far parts of the scene by different amounts and the photo reads as a shallow
// 3D scene ("move the camera after the shot").
//
// Disparity is baked into a per-vertex attribute rather than sampled from a
// texture in the vertex shader, which keeps the shader within the same GLSL 1.20
// feature set the panorama viewer targets.
class DepthWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit DepthWidget(QWidget* parent = nullptr);
    ~DepthWidget() override;

    // 'depth' is an inverse-depth map (white = near) of any size; it is sampled
    // to the grid, so it need not match 'color' pixel for pixel. A null or
    // mismatched depth map leaves the image flat rather than failing.
    void setImage(const QImage& color, const QImage& depth);
    bool hasImage() const { return m_hasImage; }
    void clear();

    // Maximum vertex shift, as a fraction of image width. Past roughly 0.06 the
    // stretched triangles over disoccluded areas become obvious.
    void setAmplitude(float a);
    float amplitude() const { return m_amplitude; }

    // Disparity held still by the parallax, 0 = far, 1 = near.
    void setFocus(float f);
    float focus() const { return m_focus; }

    // Gentle automatic orbit, used when the pointer is not driving the view.
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

private:
    void uploadPending();          // builds the mesh + texture once GL is current
    void buildMesh();              // grid geometry with baked disparity
    void applyOffsetFromCursor(const QPoint& pos);

    QOpenGLShaderProgram            m_program;
    QOpenGLBuffer                   m_vbo;
    QOpenGLBuffer                   m_ibo{QOpenGLBuffer::IndexBuffer};
    std::unique_ptr<QOpenGLTexture> m_texture;
    int                             m_indexCount = 0;

    QImage m_pendingColor;         // kept until GL is ready to take them
    QImage m_pendingDepth;
    QImage m_disparity;            // grayscale8, source for the baked attribute
    bool   m_hasImage = false;
    float  m_aspect   = 1.0f;      // image width / height

    float  m_amplitude = 0.035f;
    float  m_focus     = 0.45f;
    QPointF m_offset;              // current camera offset, -1..1 on each axis
    bool   m_dragging = false;
    QPoint m_pressPos;
    QPointF m_pressOffset;

    bool          m_autoOrbit = true;
    QTimer        m_orbitTimer;
    QElapsedTimer m_clock;
};

#endif // DEPTHWIDGET_H
