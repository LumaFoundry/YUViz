#include "videoWindow.h"
#include "mywindow.h"
#include <QSurface>
#include <QEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QDebug>

VideoWindow::VideoWindow(QObject* parent, QRhi::Implementation impl)
    : QObject(parent)
{
    m_window = new MyWindow();
#if defined(Q_OS_MACOS)
    m_window->setSurfaceType(QSurface::MetalSurface);
#elif defined(Q_OS_WIN)
    m_window->setSurfaceType(QSurface::Direct3DSurface);
#elif defined(Q_OS_LINUX)
    QVulkanInstance* vulkanInst = new QVulkanInstance(this);
    vulkanInst->setLayers({ "VK_LAYER_KHRONOS_validation" });
    vulkanInst->setExtensions(QRhiVulkanInitParams::preferredInstanceExtensions());
    m_window->setSurfaceType(QSurface::VulkanSurface);
    if (!vulkanInst->create()) {
        qWarning("Failed to create Vulkan instance, switching to OpenGL");
        m_window->setSurfaceType(QSurface::OpenGLSurface);
    } else {
        m_window->setVulkanInstance(vulkanInst);
    }
#else
    m_window->setSurfaceType(QSurface::OpenGLSurface);
#endif
    m_window->setVisibility(QWindow::Windowed);
    m_window->requestActivate();

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