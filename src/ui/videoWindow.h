#pragma once

#include <QObject>
#include <QWindow>
#include <QString>
#include <rhi/qrhi.h>

class VideoWindow : public QObject
{
    Q_OBJECT
public:
    explicit VideoWindow(QObject* parent = nullptr,
                         QRhi::Implementation impl);
    ~VideoWindow();

    QWindow* window() const;

    void show();
    void resize(int width, int height);
    void setTitle(const QString& title);

signals:
    void resized();

private:
    QWindow* m_window;
};
