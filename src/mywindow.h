#pragma once

#include <QMainWindow>
#include <QString>

class MyWindow : public QMainWindow {
    Q_OBJECT

public:
    MyWindow(QWidget *parent = nullptr);
    virtual ~MyWindow();

    void loadYUV(const QString &filePath, int width, int height);

private:
    QString yuvFilePath;
    int imageWidth;
    int imageHeight;
};
