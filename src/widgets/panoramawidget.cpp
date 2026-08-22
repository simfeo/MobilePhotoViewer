#include "panoramawidget.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <vector>
#include <cmath>

PanoramaWidget::PanoramaWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);
}

PanoramaWidget::~PanoramaWidget()
{
    makeCurrent();
    m_vbo.destroy();
    m_ibo.destroy();
    m_texture.reset();
    doneCurrent();
}

void PanoramaWidget::setPanorama(const QImage& image, Projection proj,
                                 float cylHalfHeight, float cylFovRad)
{
    m_pending    = image;
    m_projection = proj;
    m_cylHalfH   = cylHalfHeight;
    m_cylFov     = cylFovRad;
    m_yaw        = 0.0f;
    m_pitch      = 0.0f;
    m_fov        = 75.0f;
    if (context() && isValid())
    {
        makeCurrent();
        buildGeometry();
        m_texture.reset(new QOpenGLTexture(m_pending));
        // A full sphere wraps horizontally; a cylinder arc is a bounded strip.
        m_texture->setWrapMode(QOpenGLTexture::DirectionS,
                               proj == Projection::Cylindrical ? QOpenGLTexture::ClampToEdge : QOpenGLTexture::Repeat);
        m_texture->setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
        m_texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
        m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_pending = QImage();
        doneCurrent();
    }
    update();
}

namespace
{
    constexpr float kPI = 3.14159265358979323846f;

    // Full inside-out UV sphere: position (xyz) + equirectangular uv.
    void fillSphere(std::vector<float>& verts, std::vector<GLuint>& idx, int sectors, int stacks)
    {
        for (int i = 0; i <= stacks; ++i)
        {
            const float stackAngle = kPI / 2.0f - kPI * i / stacks;   // +pi/2 .. -pi/2
            const float xy         = std::cos(stackAngle);
            const float z          = std::sin(stackAngle);
            for (int j = 0; j <= sectors; ++j)
            {
                const float sectorAngle = 2.0f * kPI * j / sectors;
                verts.push_back(xy * std::cos(sectorAngle));
                verts.push_back(z);
                verts.push_back(xy * std::sin(sectorAngle));
                verts.push_back(static_cast<float>(j) / sectors);   // u
                verts.push_back(static_cast<float>(i) / stacks);    // v
            }
        }
        for (int i = 0; i < stacks; ++i)
        {
            int k1 = i * (sectors + 1);
            int k2 = k1 + sectors + 1;
            for (int j = 0; j < sectors; ++j, ++k1, ++k2)
            {
                if (i != 0)
                {
                    idx.push_back(k1);
                    idx.push_back(k2);
                    idx.push_back(k1 + 1);
                }
                if (i != stacks - 1)
                {
                    idx.push_back(k1 + 1);
                    idx.push_back(k2);
                    idx.push_back(k2 + 1);
                }
            }
        }
    }

    // Cylinder arc of width 'fovRad' (radians) and total height 2*halfH, centred on
    // -Z so it faces the camera at yaw 0. Straight sides, so one stack is enough.
    void fillCylinderArc(std::vector<float>& verts, std::vector<GLuint>& idx,
                         int sectors, float halfH, float fovRad)
    {
        const int   stacks = 1;
        const float aMid   = 3.0f * kPI / 2.0f;   // -Z, in front of the camera
        const float a0     = aMid - fovRad / 2.0f;
        for (int i = 0; i <= stacks; ++i)
        {
            const float v = static_cast<float>(i) / stacks;
            const float y = halfH - 2.0f * halfH * v;   // top (v=0) .. bottom (v=1)
            for (int j = 0; j <= sectors; ++j)
            {
                const float t = static_cast<float>(j) / sectors;
                const float a = a0 + fovRad * t;
                verts.push_back(std::cos(a));
                verts.push_back(y);
                verts.push_back(std::sin(a));
                verts.push_back(t);   // u
                verts.push_back(v);   // v
            }
        }
        for (int i = 0; i < stacks; ++i)
        {
            int k1 = i * (sectors + 1);
            int k2 = k1 + sectors + 1;
            for (int j = 0; j < sectors; ++j, ++k1, ++k2)
            {
                idx.push_back(k1);
                idx.push_back(k2);
                idx.push_back(k1 + 1);
                idx.push_back(k1 + 1);
                idx.push_back(k2);
                idx.push_back(k2 + 1);
            }
        }
    }
}   // namespace

void PanoramaWidget::buildGeometry()
{
    std::vector<float>  verts;   // x,y,z,u,v
    std::vector<GLuint> idx;
    if (m_projection == Projection::Cylindrical)
    {
        fillCylinderArc(verts, idx, 128, m_cylHalfH, m_cylFov);
    }
    else
    {
        fillSphere(verts, idx, 64, 32);
    }
    m_indexCount = static_cast<int>(idx.size());

    if (!m_vbo.isCreated())
    {
        m_vbo.create();
    }
    m_vbo.bind();
    m_vbo.allocate(verts.data(), int(verts.size() * sizeof(float)));
    if (!m_ibo.isCreated())
    {
        m_ibo.create();
    }
    m_ibo.bind();
    m_ibo.allocate(idx.data(), int(idx.size() * sizeof(GLuint)));
}

void PanoramaWidget::initializeGL()
{
    initializeOpenGLFunctions();

    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                      "attribute vec3 aPos;\n"
                                      "attribute vec2 aUV;\n"
                                      "uniform mat4 mvp;\n"
                                      "varying vec2 vUV;\n"
                                      "void main(){ vUV = aUV; gl_Position = mvp * vec4(aPos, 1.0); }\n");
    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                      "uniform sampler2D tex;\n"
                                      "varying vec2 vUV;\n"
                                      "void main(){ gl_FragColor = texture2D(tex, vUV); }\n");
    m_program.link();

    buildGeometry();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);   // we view the surface from the inside

    if (!m_pending.isNull())
    {
        setPanorama(m_pending, m_projection, m_cylHalfH, m_cylFov);
    }
}

void PanoramaWidget::resizeGL(int, int)
{
}

void PanoramaWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_texture || m_indexCount == 0)
    {
        return;
    }

    QMatrix4x4  proj;
    const float aspect = height() > 0 ? float(width()) / float(height()) : 1.0f;
    proj.perspective(m_fov, aspect, 0.05f, 10.0f);

    QMatrix4x4 view;
    view.rotate(m_pitch, 1, 0, 0);
    view.rotate(m_yaw, 0, 1, 0);

    m_program.bind();
    m_program.setUniformValue("mvp", proj * view);
    m_program.setUniformValue("tex", 0);
    m_texture->bind(0);

    m_vbo.bind();
    m_ibo.bind();
    const int stride = 5 * sizeof(float);
    m_program.enableAttributeArray("aPos");
    m_program.setAttributeBuffer("aPos", GL_FLOAT, 0, 3, stride);
    m_program.enableAttributeArray("aUV");
    m_program.setAttributeBuffer("aUV", GL_FLOAT, 3 * sizeof(float), 2, stride);

    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);

    m_program.disableAttributeArray("aPos");
    m_program.disableAttributeArray("aUV");
    m_program.release();
}

void PanoramaWidget::mousePressEvent(QMouseEvent* e)
{
    m_lastPos = e->pos();
    setCursor(Qt::ClosedHandCursor);
}

void PanoramaWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (e->buttons() & Qt::LeftButton)
    {
        const QPoint d = e->pos() - m_lastPos;
        m_lastPos      = e->pos();
        // Slower rotation when zoomed in (small fov). Inverted so the scene
        // follows the cursor ("grab and pull") on both axes.
        const float k = m_fov / 75.0f;
        m_yaw -= d.x() * 0.25f * k;
        m_pitch -= d.y() * 0.25f * k;
        m_pitch = std::max(-89.0f, std::min(89.0f, m_pitch));
        update();
    }
}

void PanoramaWidget::wheelEvent(QWheelEvent* e)
{
    (e->angleDelta().y() > 0) ? zoomIn() : zoomOut();
}

void PanoramaWidget::zoomIn()
{
    m_fov = std::max(20.0f, m_fov - 5.0f);
    update();
}

void PanoramaWidget::zoomOut()
{
    m_fov = std::min(100.0f, m_fov + 5.0f);
    update();
}

void PanoramaWidget::resetView()
{
    m_yaw   = 0.0f;
    m_pitch = 0.0f;
    m_fov   = 75.0f;
    update();
}
