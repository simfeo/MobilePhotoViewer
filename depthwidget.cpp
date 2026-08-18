#include "depthwidget.h"

#include <QMouseEvent>
#include <QMatrix4x4>
#include <algorithm>
#include <cmath>

namespace {
// The warp can only bend the image at a vertex, hence the cell size.
constexpr int kTargetCellPx = 4;
constexpr int kMinCells     = 64;
constexpr int kMaxCells     = 512;

constexpr float kOrbitScale     = 0.9f;
constexpr double kOrbitPeriodSec = 7.0;
}   // namespace

DepthWidget::DepthWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    m_orbitTimer.setInterval(16);
    connect(&m_orbitTimer, &QTimer::timeout, this, [this] {
        if (m_autoOrbit && !m_dragging && m_hasImage)
            update();
    });
    m_clock.start();
}

DepthWidget::~DepthWidget()
{
    makeCurrent();
    m_vbo.destroy();
    m_ibo.destroy();
    m_texture.reset();
    doneCurrent();
}

void DepthWidget::setImage(const QImage& color, const QImage& depth)
{
    if (color.isNull())
    {
        clear();
        return;
    }

    m_pendingColor = color.convertToFormat(QImage::Format_RGB888);
    // One format for the mesh builder, whatever the map arrived as.
    m_pendingDepth = depth.isNull() ? QImage()
                                    : depth.convertToFormat(QImage::Format_Grayscale8);
    m_aspect = color.height() > 0 ? float(color.width()) / float(color.height()) : 1.0f;
    m_hasImage = true;
    m_offset = QPointF();

    if (context() && isValid())
    {
        makeCurrent();
        uploadPending();
        doneCurrent();
    }
    if (m_autoOrbit)
        m_orbitTimer.start();
    update();
}

void DepthWidget::clear()
{
    m_orbitTimer.stop();
    m_hasImage = false;
    m_indexCount = 0;
    m_pendingColor = QImage();
    m_pendingDepth = QImage();
    m_disparity = QImage();
    if (context() && isValid())
    {
        makeCurrent();
        m_texture.reset();
        doneCurrent();
    }
    update();
}

void DepthWidget::setAmplitude(float a)
{
    m_amplitude = std::clamp(a, 0.0f, 0.25f);
    update();
}

void DepthWidget::setFocusPlane(float f)
{
    m_focus = std::clamp(f, 0.0f, 1.0f);
    if (m_hasImage && context() && isValid())
    {
        makeCurrent();
        buildMesh();       // focus is baked per vertex
        doneCurrent();
    }
    update();
}

void DepthWidget::setAutoOrbit(bool on)
{
    m_autoOrbit = on;
    if (on && m_hasImage)
    {
        m_orbitTimer.start();
    }
    else
    {
        m_orbitTimer.stop();
        m_offset = QPointF();
    }
    update();
}

void DepthWidget::resetView()
{
    m_offset = QPointF();
    update();
}

void DepthWidget::uploadPending()
{
    if (!m_pendingColor.isNull())
    {
        m_disparity = m_pendingDepth;
        m_texture.reset(new QOpenGLTexture(m_pendingColor));
        m_texture->setWrapMode(QOpenGLTexture::ClampToEdge);
        m_texture->setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
        m_texture->setMagnificationFilter(QOpenGLTexture::Linear);
        m_pendingColor = QImage();
        m_pendingDepth = QImage();
        buildMesh();
    }
}

void DepthWidget::buildMesh()
{
    const bool haveDepth = !m_disparity.isNull();
    const int dw = haveDepth ? m_disparity.width()  : 1;
    const int dh = haveDepth ? m_disparity.height() : 1;

    const int cellsX = haveDepth
        ? std::clamp(dw / kTargetCellPx, kMinCells, kMaxCells) : kMinCells;
    const int cellsY = haveDepth
        ? std::clamp(dh / kTargetCellPx, kMinCells, kMaxCells) : kMinCells;

    std::vector<float> verts;
    verts.reserve(std::size_t(cellsX + 1) * (cellsY + 1) * 5);
    for (int iy = 0; iy <= cellsY; ++iy)
    {
        const float v = float(iy) / cellsY;
        for (int ix = 0; ix <= cellsX; ++ix)
        {
            const float u = float(ix) / cellsX;
            float disparity = 0.0f;
            if (haveDepth)
            {
                const int sx = std::min(int(u * dw), dw - 1);
                const int sy = std::min(int(v * dh), dh - 1);
                disparity = m_disparity.constScanLine(sy)[sx] / 255.0f;
            }
            verts.push_back(u * 2.0f - 1.0f);
            verts.push_back(1.0f - v * 2.0f);
            verts.push_back(u);
            verts.push_back(v);
            verts.push_back(disparity - m_focus);
        }
    }

    std::vector<GLuint> idx;
    idx.reserve(std::size_t(cellsX) * cellsY * 6);
    for (int iy = 0; iy < cellsY; ++iy)
    {
        for (int ix = 0; ix < cellsX; ++ix)
        {
            const GLuint a = GLuint(iy * (cellsX + 1) + ix);
            const GLuint b = a + 1;
            const GLuint c = a + GLuint(cellsX + 1);
            const GLuint d = c + 1;
            idx.push_back(a); idx.push_back(c); idx.push_back(b);
            idx.push_back(b); idx.push_back(c); idx.push_back(d);
        }
    }
    m_indexCount = int(idx.size());

    if (!m_vbo.isCreated()) m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(verts.data(), int(verts.size() * sizeof(float)));
    if (!m_ibo.isCreated()) m_ibo.create();
    m_ibo.bind();
    m_ibo.allocate(idx.data(), int(idx.size() * sizeof(GLuint)));
}

void DepthWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // z is biased by disparity so nearer geometry wins where the sheet folds over.
    m_program.addShaderFromSourceCode(QOpenGLShader::Vertex,
        "attribute vec2 aPos;\n"
        "attribute vec2 aUV;\n"
        "attribute float aDisp;\n"
        "uniform mat4 mvp;\n"
        "uniform vec2 shift;\n"
        "varying vec2 vUV;\n"
        "void main(){\n"
        "  vUV = aUV;\n"
        "  vec2 p = aPos + aDisp * shift;\n"
        "  gl_Position = mvp * vec4(p, -aDisp, 1.0);\n"
        "}\n");
    m_program.addShaderFromSourceCode(QOpenGLShader::Fragment,
        "uniform sampler2D tex;\n"
        "varying vec2 vUV;\n"
        "void main(){ gl_FragColor = texture2D(tex, vUV); }\n");
    m_program.link();

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);   // the warped sheet can face either way

    uploadPending();
}

void DepthWidget::paintGL()
{
    glViewport(0, 0, int(width() * devicePixelRatioF()), int(height() * devicePixelRatioF()));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_texture || m_indexCount == 0)
        return;

    QPointF offset = m_offset;
    if (m_autoOrbit && !m_dragging)
    {
        const double t = m_clock.elapsed() / 1000.0;
        const double a = 2.0 * M_PI * t / kOrbitPeriodSec;
        offset = QPointF(std::sin(a) * kOrbitScale, std::cos(a) * kOrbitScale * 0.4);
        m_offset = offset;   // so a drag picks up where the orbit left off
    }

    // Draw oversized into a fitted viewport, so the ragged warped border is clipped.
    const qreal dpr = devicePixelRatioF();
    const int fbW = int(width() * dpr);
    const int fbH = int(height() * dpr);
    int vw = fbW, vh = int(fbW / m_aspect);
    if (vh > fbH)
    {
        vh = fbH;
        vw = int(fbH * m_aspect);
    }
    glViewport((fbW - vw) / 2, (fbH - vh) / 2, vw, vh);

    // A vertex moves at most amplitude*2*|disparity-focus| in NDC, so a little
    // over the amplitude is enough margin -- more just crops the photo for nothing.
    const float overscan = 1.0f + m_amplitude * 1.6f;
    QMatrix4x4 mvp;
    mvp.ortho(-1.0f, 1.0f, -1.0f, 1.0f, -2.0f, 2.0f);
    mvp.scale(overscan, overscan, 1.0f);

    m_program.bind();
    m_program.setUniformValue("mvp", mvp);
    m_program.setUniformValue("tex", 0);
    m_program.setUniformValue("shift",
        QVector2D(float(offset.x()) * m_amplitude * 2.0f,
                  float(offset.y()) * m_amplitude * 2.0f));
    m_texture->bind(0);

    m_vbo.bind();
    m_ibo.bind();
    const int stride = 5 * sizeof(float);
    m_program.enableAttributeArray("aPos");
    m_program.setAttributeBuffer("aPos", GL_FLOAT, 0, 2, stride);
    m_program.enableAttributeArray("aUV");
    m_program.setAttributeBuffer("aUV", GL_FLOAT, 2 * sizeof(float), 2, stride);
    m_program.enableAttributeArray("aDisp");
    m_program.setAttributeBuffer("aDisp", GL_FLOAT, 4 * sizeof(float), 1, stride);

    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);

    m_program.disableAttributeArray("aPos");
    m_program.disableAttributeArray("aUV");
    m_program.disableAttributeArray("aDisp");
    m_program.release();
}

void DepthWidget::applyOffsetFromCursor(const QPoint& pos)
{
    const QPoint d = pos - m_pressPos;
    const float span = float(std::max(width(), 1)) * 0.5f;
    m_offset = QPointF(std::clamp(m_pressOffset.x() + d.x() / span, -1.0, 1.0),
                       std::clamp(m_pressOffset.y() + d.y() / span, -1.0, 1.0));
    update();
}

void DepthWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton || !m_hasImage)
        return;
    m_dragging = true;
    m_pressPos = e->pos();
    m_pressOffset = m_offset;
    setCursor(Qt::ClosedHandCursor);
}

void DepthWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging && (e->buttons() & Qt::LeftButton))
        applyOffsetFromCursor(e->pos());
}

void DepthWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return;
    m_dragging = false;
    unsetCursor();
    update();
}

void DepthWidget::showEvent(QShowEvent* e)
{
    QOpenGLWidget::showEvent(e);
    if (m_autoOrbit && m_hasImage)
        m_orbitTimer.start();
}

void DepthWidget::hideEvent(QHideEvent* e)
{
    // Leaving the mode: stop driving repaints until it is shown again.
    m_orbitTimer.stop();
    QOpenGLWidget::hideEvent(e);
}

void DepthWidget::leaveEvent(QEvent* e)
{
    m_dragging = false;
    unsetCursor();
    QOpenGLWidget::leaveEvent(e);
}
