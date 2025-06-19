#include "videoWindow.h"
#include <QSurface>

VideoWindow::VideoWindow(QObject* parent,
                         QRhi::Implementation impl):
    QObject(parent), 
    m_window(new QWindow())
{
    switch (impl) {
        // 
        case QRhi::Null:
            break;
        case QRhi::OpenGLES2:
            m_window->setSurfaceType(QSurface::OpenGLSurface);
            break;
        case QRhi::Vulkan:
            m_window->setSurfaceType(QSurface::VulkanSurface);
            break;
        case QRhi::D3D11:
            m_window->setSurfaceType(QSurface::Direct3DSurface);
            break;
        case QRhi::D3D12:
            m_window->setSurfaceType(QSurface::Direct3DSurface);
            break;
        case QRhi::Metal:
            m_window->setSurfaceType(QSurface::MetalSurface);
            break;
    }

    connect(m_window, &QWindow::widthChanged, this, [this](int w){
        emit resized();
    });
    connect(m_window, &QWindow::heightChanged, this, [this](int h){
        emit resized();
    });
}

VideoWindow::~VideoWindow()
{
    delete m_window;
}

QWindow* VideoWindow::window() const
{
    return m_window;
}

void VideoWindow::show() {
    m_window->show();
}

void VideoWindow::resize(int width, int height) {
    m_window->resize(width, height);
}

void VideoWindow::setTitle(const QString& title){
    m_window->setTitle(title);
}