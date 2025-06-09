#ifndef MYWINDOW_H
#define MYWINDOW_H

#include <QMainWindow>
#include <QString>

class MyWindow : public QMainWindow {
    Q_OBJECT

public:
    MyWindow(QWidget *parent = nullptr);
    void loadYUV(const QString &filePath, int width, int height);

private:
    QString yuvFilePath;
    int imageWidth;
    int imageHeight;
};

#endif // MYWINDOW_H